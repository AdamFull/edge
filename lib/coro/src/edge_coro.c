#include "edge_coro_internal.h"

#include <stdatomic.h>
#include <string.h>
#include <assert.h>

#include <edge_allocator.h>
#include <edge_arena.h>

#define EDGE_CORO_DEFAULT_STACK_SIZE (1024 * 1024)
#define EDGE_CORO_STACK_ALIGN 16

// TODO: Make this better in future
typedef struct free_node {
    void* ptr;
    struct free_node* next;
} free_node_t;

typedef struct free_list {
    free_node_t* head;
    size_t count;
} free_list_t;

struct edge_coro_thread_context {
    edge_allocator_t allocator;

    edge_arena_t* stack_arena;
    free_list_t free_stacks;

    edge_coro_t* current_coro;
    edge_coro_t main_coro;
    struct edge_coro_context main_context;
};

static _Thread_local struct edge_coro_thread_context g_coro_thread_context = { 0 };

static void* edge_coro_malloc(size_t size) {
    if (g_coro_thread_context.main_coro.status == 0) {
        return NULL;
    }

    return edge_allocator_malloc(&g_coro_thread_context.allocator, size);
}

static void edge_coro_free(void* ptr) {
    if(!ptr || g_coro_thread_context.main_coro.status == 0) {
        return;
    }

    return edge_allocator_free(&g_coro_thread_context.allocator, ptr);
}

static void edge_coro_main(void) {
    edge_coro_t* coro = g_coro_thread_context.current_coro;
    /* Run the coroutine function */
    if (coro && coro->func) {
        coro->status = EDGE_CORO_STATE_RUNNING;
        coro->func(coro->user_data);
        coro->status = EDGE_CORO_STATE_FINISHED;
    }

    /* Return to caller */
    if (coro->caller) {
        atomic_signal_fence(memory_order_release);
        edge_coro_swap_context(coro->context, coro->caller->context);
        atomic_signal_fence(memory_order_acquire);
    }

    /* Should never reach here */
    assert(0 && "Coroutine returned without caller");
}

static void* edge_coro_alloc_stack_pointer() {
    free_node_t* free_node = g_coro_thread_context.free_stacks.head;
    if (free_node) {
        void* stack_pointer = free_node->ptr;
        g_coro_thread_context.free_stacks.head = free_node->next;
        edge_coro_free(free_node);
        g_coro_thread_context.free_stacks.count--;
        return stack_pointer;
    }

    return edge_arena_alloc_ex(g_coro_thread_context.stack_arena, EDGE_CORO_DEFAULT_STACK_SIZE, EDGE_CORO_STACK_ALIGN);
}

static void edge_coro_free_stack_pointer(void* ptr) {
    if (!ptr) {
        return;
    }

    free_node_t* free_node = (free_node_t*)edge_coro_malloc(sizeof(free_node_t));
    if (!free_node) {
        return;
    }

    free_node->ptr = ptr;
    free_node->next = g_coro_thread_context.free_stacks.head;
    g_coro_thread_context.free_stacks.head = free_node;
    g_coro_thread_context.free_stacks.count++;
}

void edge_coro_init_thread_context(edge_allocator_t* allocator) {
    if (g_coro_thread_context.main_coro.status == 0) {
        g_coro_thread_context.stack_arena = edge_arena_create(allocator, 0, true);
        g_coro_thread_context.free_stacks.head = NULL;
        g_coro_thread_context.free_stacks.count = 0;

        g_coro_thread_context.allocator = *allocator;
        g_coro_thread_context.main_coro.status = EDGE_CORO_STATE_RUNNING;
        g_coro_thread_context.main_coro.context = &g_coro_thread_context.main_context;
        g_coro_thread_context.current_coro = &g_coro_thread_context.main_coro;
    }
}

void edge_coro_shutdown_thread_context(void) {
    if (g_coro_thread_context.main_coro.status != 0) {
        free_node_t* free_node = g_coro_thread_context.free_stacks.head;
        while (free_node) {
            free_node_t* next = free_node->next;
            edge_coro_free(free_node);
            free_node = next;
        }

        edge_arena_destroy(g_coro_thread_context.stack_arena);
    }
}

edge_coro_t* edge_coro_create(edge_coro_fn function, void* arg) {
    if (!function) {
        return NULL;
    }

    /* Allocate coroutine structure */
    edge_coro_t* coro = (edge_coro_t*)edge_coro_malloc(sizeof(edge_coro_t));
    if (!coro) {
        return NULL;
    }

    memset(coro, 0, sizeof(edge_coro_t));

    /* Allocate context */
    coro->context = (struct edge_coro_context*)edge_coro_malloc(sizeof(struct edge_coro_context));
    if (!coro->context) {
        edge_coro_free(coro);
        return NULL;
    }

    memset(coro->context, 0, sizeof(struct edge_coro_context));

    coro->status = EDGE_CORO_STATE_READY;
    coro->caller = NULL;
    coro->func = function;
    coro->user_data = arg;

    /* Allocate stack */
    coro->stack = edge_coro_alloc_stack_pointer();
    if (!coro->stack) {
        edge_coro_free(coro->context);
        edge_coro_free(coro);
        return NULL;
    }

    /* Align stack (grows downward, so align the top) */
    void* stack_top = (char*)coro->stack + EDGE_CORO_DEFAULT_STACK_SIZE;
    stack_top = (void*)((uintptr_t)stack_top & ~(EDGE_CORO_STACK_ALIGN - 1));

#if defined(__x86_64__)

#ifdef _WIN32
    /* Windows x64 ABI: Reserve 32 bytes shadow space + 8 bytes for alignment */
    stack_top = (char*)stack_top - 40;
#endif

    coro->context->rip = edge_coro_main;
    coro->context->rsp = stack_top;
#elif defined(__aarch64__)
    coro->context->lr = edge_coro_main;
    coro->context->sp = stack_top;
#endif

    return coro;
}

void edge_coro_destroy(edge_coro_t* coro) {
    if (!coro) {
        return;
    }

    if (coro->stack) {
        edge_coro_free_stack_pointer(coro->stack);
    }

    if (coro->context) {
        edge_coro_free(coro->context);
    }

    edge_coro_free(coro);
}

bool edge_coro_resume(edge_coro_t* coro) {
    if (!coro || (coro->status != EDGE_CORO_STATE_READY && coro->status != EDGE_CORO_STATE_SUSPENDED)) {
        return false;
    }

    edge_coro_t* caller = edge_coro_current();
    coro->caller = caller;

    caller->status = EDGE_CORO_STATE_SUSPENDED;
    coro->status = EDGE_CORO_STATE_RUNNING;
    g_coro_thread_context.current_coro = coro;

    /* Swap to coroutine */
    atomic_signal_fence(memory_order_release);
    edge_coro_swap_context(caller->context, coro->context);
    atomic_signal_fence(memory_order_acquire);

    /* When we return here, coroutine has yielded or finished */
    g_coro_thread_context.current_coro = caller;
    caller->status = EDGE_CORO_STATE_RUNNING;

    return coro->status != EDGE_CORO_STATE_FINISHED;
}

void edge_coro_yield(void) {
    struct edge_coro_thread_context* ctx = &g_coro_thread_context;

    edge_coro_t* coro = ctx->current_coro;
    if (!coro || coro == &ctx->main_coro || coro->status == EDGE_CORO_STATE_FINISHED) {
        return; /* Cannot yield from main */
    }

    edge_coro_t* caller = coro->caller;
    if (!caller) {
        return;
    }

    coro->status = EDGE_CORO_STATE_SUSPENDED;
    caller->status = EDGE_CORO_STATE_RUNNING;

    /* Swap back to caller */
    atomic_signal_fence(memory_order_release);
    edge_coro_swap_context(coro->context, caller->context);
    atomic_signal_fence(memory_order_acquire);

    /* When resumed, we continue here */
    coro->status = EDGE_CORO_STATE_RUNNING;
}

edge_coro_t* edge_coro_current(void) {
    return g_coro_thread_context.current_coro;
}

edge_coro_status_t edge_coro_status(edge_coro_t* coro) {
    return coro ? coro->status : EDGE_CORO_STATE_FINISHED;
}

bool edge_coro_alive(edge_coro_t* coro) {
    return coro && coro->status != EDGE_CORO_STATE_FINISHED;
}