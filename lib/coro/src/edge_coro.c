#include "edge_coro_internal.h"

#include <stdatomic.h>
#include <string.h>
#include <assert.h>

#include <edge_allocator.h>

#define EDGE_CORO_DEFAULT_STACK_SIZE (1024 * 1024)
#define EDGE_CORO_STACK_ALIGN 16

struct edge_coro_thread_context {
    edge_coro_t* current_coro;
    edge_coro_t main_coro;
    struct edge_coro_context main_context;
};

static struct edge_coro_thread_context* edge_coro_get_current_thread_context(void) {
    static _Thread_local struct edge_coro_thread_context context = { 0 };

    /* Initialize main coroutine on first use */
    if (!context.main_coro.status == 0) {
        context.main_coro.status = EDGE_CORO_STATE_RUNNING;
        context.main_coro.context = &context.main_context;
        context.current_coro = &context.main_coro;
    }

    return &context;
}

static void edge_coro_main(void) {
    struct edge_coro_thread_context* thread_context = edge_coro_get_current_thread_context();

    edge_coro_t* coro = thread_context->current_coro;
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

edge_coro_t* edge_coro_create(edge_allocator_t* allocator, edge_coro_fn function, void* arg, size_t stack_size) {
    if (!allocator || !function) {
        return NULL;
    }

    /* Allocate coroutine structure */
    edge_coro_t* coro = (edge_coro_t*)edge_allocator_malloc(allocator, sizeof(edge_coro_t));// , _Alignof(edge_coro_t));
    if (!coro) {
        return NULL;
    }

    memset(coro, 0, sizeof(edge_coro_t));

    /* Allocate context */
    coro->context = (struct edge_coro_context*)edge_allocator_malloc(allocator, sizeof(struct edge_coro_context));// , _Alignof(edge_coro_context_t));
    if (!coro->context) {
        edge_allocator_free(allocator, coro);
        return NULL;
    }

    memset(coro->context, 0, sizeof(struct edge_coro_context));

    coro->stack_size = stack_size > 0 ? stack_size : EDGE_CORO_DEFAULT_STACK_SIZE;
    coro->allocator = allocator;
    coro->status = EDGE_CORO_STATE_READY;
    coro->caller = NULL;
    coro->func = function;
    coro->user_data = arg;

    /* Allocate stack */
    size_t alloc_size = coro->stack_size + EDGE_CORO_STACK_ALIGN;
    coro->stack = edge_allocator_malloc(allocator, alloc_size);//, 16);
    if (!coro->stack) {
        edge_allocator_free(allocator, coro->context);
        edge_allocator_free(allocator, coro);
        return NULL;
    }

    edge_allocator_protect(coro->stack, alloc_size, EDGE_ALLOCATOR_PROTECT_READ_WRITE);

    /* Align stack (grows downward, so align the top) */
    void* stack_top = (char*)coro->stack + alloc_size;
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

    edge_allocator_t* allocator = coro->allocator;

    if (coro->stack) {
        edge_allocator_free(allocator, coro->stack);
    }

    if (coro->context) {
        edge_allocator_free(allocator, coro->context);
    }

    edge_allocator_free(allocator, coro);
}

bool edge_coro_resume(edge_coro_t* coro) {
    if (!coro || (coro->status != EDGE_CORO_STATE_READY && coro->status != EDGE_CORO_STATE_SUSPENDED)) {
        return false;
    }

    struct edge_coro_thread_context* ctx = edge_coro_get_current_thread_context();
    edge_coro_t* caller = ctx->current_coro;
    coro->caller = caller;

    caller->status = EDGE_CORO_STATE_SUSPENDED;
    coro->status = EDGE_CORO_STATE_RUNNING;
    ctx->current_coro = coro;

    /* Swap to coroutine */
    atomic_signal_fence(memory_order_release);
    edge_coro_swap_context(caller->context, coro->context);
    atomic_signal_fence(memory_order_acquire);

    /* When we return here, coroutine has yielded or finished */
    ctx->current_coro = caller;
    caller->status = EDGE_CORO_STATE_RUNNING;

    return coro->status != EDGE_CORO_STATE_FINISHED;
}

void edge_coro_yield(void) {
    struct edge_coro_thread_context* ctx = edge_coro_get_current_thread_context();

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
    struct edge_coro_thread_context* ctx = edge_coro_get_current_thread_context();
    return ctx->current_coro;
}

edge_coro_status_t edge_coro_status(edge_coro_t* coro) {
    return coro ? coro->status : EDGE_CORO_STATE_FINISHED;
}

bool edge_coro_alive(edge_coro_t* coro) {
    return coro && coro->status != EDGE_CORO_STATE_FINISHED;
}