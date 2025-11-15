#include "edge_coro_internal.h"
#include "edge_threads_ext.h"

#include <stdatomic.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

#include <edge_allocator.h>
#include <edge_arena.h>
#include <edge_list.h>
#include <edge_queue.h>
#include <edge_vector.h>

struct edge_coro_thread_context {
    edge_allocator_t allocator;

    edge_arena_t* stack_arena;
    edge_list_t* free_stacks;

    edge_coro_t* current_coro;
    edge_coro_t main_coro;
    struct edge_coro_context main_context;
};

static _Thread_local struct edge_coro_thread_context g_coro_thread_context = { 0 };

static void* edge_coro_malloc(size_t size) {
    if (g_coro_thread_context.main_coro.state == 0) {
        return NULL;
    }

    return edge_allocator_malloc(&g_coro_thread_context.allocator, size);
}

static void edge_coro_free(void* ptr) {
    if(!ptr || g_coro_thread_context.main_coro.state == 0) {
        return;
    }

    return edge_allocator_free(&g_coro_thread_context.allocator, ptr);
}

static void edge_coro_main(void) {
    edge_coro_t* coro = g_coro_thread_context.current_coro;
    /* Run the coroutine function */
    if (coro && coro->func) {
        coro->state = EDGE_CORO_STATE_RUNNING;
        coro->func(coro->user_data);
        coro->state = EDGE_CORO_STATE_FINISHED;
    }

    /* Return to caller */
    if (coro->caller) {
        edge_coro_swap_context(coro->context, coro->caller->context);
    }

    /* Should never reach here */
    assert(0 && "Coroutine returned without caller");
}

static void* edge_coro_alloc_stack_pointer() {
    struct edge_coro_thread_context* ctx = &g_coro_thread_context;

    uintptr_t ptr_addr = 0x0;
    if (!edge_list_pop_back(ctx->free_stacks, &ptr_addr)) {
        return edge_arena_alloc_ex(g_coro_thread_context.stack_arena, EDGE_CORO_STACK_SIZE, EDGE_CORO_STACK_ALIGN);
    }

    return (void*)ptr_addr;
}

static void edge_coro_free_stack_pointer(void* ptr) {
    if (!ptr) {
        return;
    }

    struct edge_coro_thread_context* ctx = &g_coro_thread_context;

    uintptr_t ptr_addr = (uintptr_t)ptr;
    if (!edge_list_push_back(ctx->free_stacks, &ptr_addr)) {
        return;
    }
}

void edge_coro_init_thread_context(edge_allocator_t* allocator) {
    if (g_coro_thread_context.main_coro.state == 0) {
        g_coro_thread_context.stack_arena = edge_arena_create(allocator, 0, false);
        g_coro_thread_context.free_stacks = edge_list_create(allocator, sizeof(uintptr_t));

        g_coro_thread_context.allocator = *allocator;
        g_coro_thread_context.main_coro.state = EDGE_CORO_STATE_RUNNING;
        g_coro_thread_context.main_coro.context = &g_coro_thread_context.main_context;
        g_coro_thread_context.current_coro = &g_coro_thread_context.main_coro;
    }
}

void edge_coro_shutdown_thread_context(void) {
    if (g_coro_thread_context.main_coro.state != 0) {
        edge_list_destroy(g_coro_thread_context.free_stacks);
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

    coro->state = EDGE_CORO_STATE_READY;
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
    void* stack_top = (char*)coro->stack + EDGE_CORO_STACK_SIZE;
    stack_top = (void*)((uintptr_t)stack_top & ~(EDGE_CORO_STACK_ALIGN - 1));

#if defined(__x86_64__)

#ifdef _WIN32
    /* Windows x64 ABI: Reserve 32 bytes shadow space + 8 bytes for alignment */
    stack_top = (char*)stack_top - 40;
#else
    stack_top = (void*)(((uintptr_t)stack_top - 8) & ~15UL);
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
    if (!coro || (coro->state != EDGE_CORO_STATE_READY && coro->state != EDGE_CORO_STATE_SUSPENDED)) {
        return false;
    }

    edge_coro_t* caller = edge_coro_current();
    coro->caller = caller;

    caller->state = EDGE_CORO_STATE_SUSPENDED;
    coro->state = EDGE_CORO_STATE_RUNNING;

    edge_coro_t* volatile target_coro = coro;
    g_coro_thread_context.current_coro = target_coro;

    /* Swap to coroutine */
    atomic_signal_fence(memory_order_release);
    edge_coro_swap_context(caller->context, coro->context);
    atomic_signal_fence(memory_order_acquire);

    /* When we return here, coroutine has yielded or finished */
    g_coro_thread_context.current_coro = caller;
    caller->state = EDGE_CORO_STATE_RUNNING;

    return coro->state != EDGE_CORO_STATE_FINISHED;
}

void edge_coro_yield(void) {
    struct edge_coro_thread_context* ctx = &g_coro_thread_context;

    edge_coro_t* coro = ctx->current_coro;
    if (!coro || coro == &ctx->main_coro || coro->state == EDGE_CORO_STATE_FINISHED) {
        return; /* Cannot yield from main */
    }

    edge_coro_t* caller = coro->caller;
    if (!caller) {
        return;
    }

    coro->state = EDGE_CORO_STATE_SUSPENDED;
    caller->state = EDGE_CORO_STATE_RUNNING;

    /* Swap back to caller */
    atomic_signal_fence(memory_order_release);
    edge_coro_swap_context(coro->context, caller->context);
    atomic_signal_fence(memory_order_acquire);

    /* When resumed, we continue here */
    coro->state = EDGE_CORO_STATE_RUNNING;
}

edge_coro_t* edge_coro_current(void) {
    return g_coro_thread_context.current_coro;
}

edge_coro_state_t edge_coro_state(edge_coro_t* coro) {
    return coro ? coro->state : EDGE_CORO_STATE_FINISHED;
}

bool edge_coro_alive(edge_coro_t* coro) {
    return coro && coro->state != EDGE_CORO_STATE_FINISHED;
}