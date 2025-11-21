#include "edge_coro.h"
#include "edge_fiber.h"

#include <stdatomic.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

#include <edge_allocator.h>
#include <edge_arena.h>
#include <edge_list.h>
#include <edge_queue.h>
#include <edge_vector.h>

struct edge_coro {
    edge_fiber_context_t* context;
    edge_coro_fn func;
    void* user_data;
    edge_coro_state_t state;
    struct edge_coro* caller;
};

struct edge_coro_thread_context {
    edge_allocator_t allocator;

    edge_arena_t* stack_arena;
    edge_list_t* free_stacks;

    edge_coro_t* current_coro;
    edge_coro_t main_coro;
    edge_fiber_context_t* main_context;
};

static _Thread_local struct edge_coro_thread_context thread_context = { 0 };

static void* edge_coro_malloc(size_t size) {
    if (thread_context.main_coro.state == 0) {
        return NULL;
    }

    return edge_allocator_malloc(&thread_context.allocator, size);
}

static void edge_coro_free(void* ptr) {
    if(!ptr || thread_context.main_coro.state == 0) {
        return;
    }

    return edge_allocator_free(&thread_context.allocator, ptr);
}

static void edge_coro_main(void) {
    edge_coro_t* coro = thread_context.current_coro;
    /* Run the coroutine function */
    if (coro && coro->func) {
        coro->state = EDGE_CORO_STATE_RUNNING;
        coro->func(coro->user_data);
        coro->state = EDGE_CORO_STATE_FINISHED;
    }

    /* Return to caller */
    if (coro->caller) {
        edge_fiber_context_switch(coro->context, coro->caller->context);
    }

    /* Should never reach here */
    assert(0 && "Coroutine returned without caller");
}

static void* edge_coro_alloc_stack_pointer() {
    struct edge_coro_thread_context* ctx = &thread_context;

    uintptr_t ptr_addr = 0x0;
    if (!edge_list_pop_back(ctx->free_stacks, &ptr_addr)) {
        return edge_arena_alloc_ex(thread_context.stack_arena, EDGE_FIBER_STACK_SIZE, EDGE_FIBER_STACK_ALIGN);
    }

    return (void*)ptr_addr;
}

static void edge_coro_free_stack_pointer(void* ptr) {
    if (!ptr) {
        return;
    }

    struct edge_coro_thread_context* ctx = &thread_context;

    uintptr_t ptr_addr = (uintptr_t)ptr;
    if (!edge_list_push_back(ctx->free_stacks, &ptr_addr)) {
        return;
    }
}

void edge_coro_init_thread_context(edge_allocator_t* allocator) {
    if (thread_context.main_coro.state == 0) {
        thread_context.stack_arena = edge_arena_create(allocator, 0);
        thread_context.free_stacks = edge_list_create(allocator, sizeof(uintptr_t));

        thread_context.allocator = *allocator;
        thread_context.main_context = edge_fiber_context_create(allocator, NULL, NULL, 0);
        thread_context.main_coro.state = EDGE_CORO_STATE_RUNNING;
        thread_context.main_coro.context = thread_context.main_context;
        thread_context.current_coro = &thread_context.main_coro;
    }
}

void edge_coro_shutdown_thread_context(void) {
    if (thread_context.main_coro.state != 0) {
        if (thread_context.main_context) {
            edge_fiber_context_destroy(&thread_context.allocator, thread_context.main_context);
        }
        
        if (thread_context.free_stacks) {
            edge_list_destroy(thread_context.free_stacks);
        }
        
        if (thread_context.stack_arena) {
            edge_arena_destroy(thread_context.stack_arena);
        }
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

    void* stack_ptr = edge_coro_alloc_stack_pointer();
    if (!stack_ptr) {
        edge_coro_free(coro);
        return NULL;
    }

    /* Allocate context */
    coro->context = edge_fiber_context_create(&thread_context.allocator, edge_coro_main, stack_ptr, EDGE_FIBER_STACK_SIZE);
    if (!coro->context) {
        edge_coro_free_stack_pointer(stack_ptr);
        edge_coro_free(coro);
        return NULL;
    }

    coro->state = EDGE_CORO_STATE_SUSPENDED;
    coro->caller = NULL;
    coro->func = function;
    coro->user_data = arg;

    return coro;
}

void edge_coro_destroy(edge_coro_t* coro) {
    if (!coro) {
        return;
    }

    if (coro->context) {
        void* stack_ptr = edge_fiber_get_stack_ptr(coro->context);
        if (stack_ptr) {
            edge_coro_free_stack_pointer(stack_ptr);
        }

        edge_fiber_context_destroy(&thread_context.allocator, coro->context);
    }

    edge_coro_free(coro);
}

bool edge_coro_resume(edge_coro_t* coro) {
    if (!coro || coro->state != EDGE_CORO_STATE_SUSPENDED) {
        return false;
    }

    edge_coro_t* caller = edge_coro_current();
    coro->caller = caller;

    caller->state = EDGE_CORO_STATE_SUSPENDED;
    coro->state = EDGE_CORO_STATE_RUNNING;

    edge_coro_t* volatile target_coro = coro;
    thread_context.current_coro = target_coro;

    edge_fiber_context_switch(caller->context, coro->context);

    /* When we return here, coroutine has yielded or finished */
    thread_context.current_coro = caller;
    caller->state = EDGE_CORO_STATE_RUNNING;

    return coro->state != EDGE_CORO_STATE_FINISHED;
}

void edge_coro_yield(void) {
    struct edge_coro_thread_context* ctx = &thread_context;

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

    edge_fiber_context_switch(coro->context, caller->context);

    /* When resumed, we continue here */
    coro->state = EDGE_CORO_STATE_RUNNING;
}

edge_coro_t* edge_coro_current(void) {
    return thread_context.current_coro;
}

edge_coro_state_t edge_coro_state(edge_coro_t* coro) {
    return coro ? coro->state : EDGE_CORO_STATE_FINISHED;
}

bool edge_coro_alive(edge_coro_t* coro) {
    return coro && coro->state != EDGE_CORO_STATE_FINISHED;
}
