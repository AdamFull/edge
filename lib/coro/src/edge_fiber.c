#include "edge_fiber.h"

#include <edge_allocator.h>

#include <stdatomic.h>
#include <stdlib.h>

#ifdef ASAN_ENABLED
extern void __sanitizer_start_switch_fiber(void** fake_stack_save, const void* bottom, size_t size);
extern void __sanitizer_finish_switch_fiber(void* fake_stack_save, const void** bottom_old, size_t* size_old);
#else
void __sanitizer_start_switch_fiber(void** fake_stack_save, const void* bottom, size_t size) {}
void __sanitizer_finish_switch_fiber(void* fake_stack_save, const void** bottom_old, size_t* size_old) {}
#endif

#ifdef TSAN_ENABLED
// TSAN fiber API
extern void* __tsan_get_current_fiber(void);
extern void* __tsan_create_fiber(unsigned flags);
extern void __tsan_destroy_fiber(void* fiber);
extern void __tsan_switch_to_fiber(void* fiber, unsigned flags);
#else
void* __tsan_get_current_fiber(void) { return NULL; }
void* __tsan_create_fiber(unsigned flags) { return NULL; }
void __tsan_destroy_fiber(void* fiber) {}
void __tsan_switch_to_fiber(void* fiber, unsigned flags) {}
#endif

struct edge_fiber_context {
#if defined(__x86_64__) || defined(_M_X64)
    /* x86_64 context: save callee-saved registers */
    void* rip;    /* Return address */
    void* rsp;    /* Stack pointer */
    void* rbp;    /* Base pointer */
    void* rbx;
    void* r12;
    void* r13;
    void* r14;
    void* r15;
#ifdef _WIN32
    /* Windows x64 ABI requires saving these */
    void* rdi;
    void* rsi;
    uint64_t xmm6[2];
    uint64_t xmm7[2];
    uint64_t xmm8[2];
    uint64_t xmm9[2];
    uint64_t xmm10[2];
    uint64_t xmm11[2];
    uint64_t xmm12[2];
    uint64_t xmm13[2];
    uint64_t xmm14[2];
    uint64_t xmm15[2];
#endif

#elif defined(__aarch64__) || defined(_M_ARM64)
    /* aarch64 context: save callee-saved registers */
    /* ARM64 callee-saved registers */
    void* lr;     /* Link register (return address) */
    void* sp;     /* Stack pointer */
    void* fp;     /* Frame pointer (x29) */
    void* x19;
    void* x20;
    void* x21;
    void* x22;
    void* x23;
    void* x24;
    void* x25;
    void* x26;
    void* x27;
    void* x28;

    /* Floating point registers (d8-d15 are callee-saved) */
    uint64_t d8;
    uint64_t d9;
    uint64_t d10;
    uint64_t d11;
    uint64_t d12;
    uint64_t d13;
    uint64_t d14;
    uint64_t d15;
#else
#error "Unsupported architecture"
#endif

    void* stack_ptr;
    size_t stack_size;

    void* tsan_fiber;
    bool main_fiber;
};

extern void fiber_swap_context_internal(edge_fiber_context_t* from, edge_fiber_context_t* to);
extern void fiber_main();

void fiber_init(void) {
    atomic_signal_fence(memory_order_acquire);
    __sanitizer_finish_switch_fiber(NULL, NULL, NULL);
}

void fiber_abort(void) {
    abort();
}

edge_fiber_context_t* edge_fiber_context_create(edge_allocator_t* allocator, edge_fiber_entry_fn entry, void* stack_ptr, size_t stack_size) {
	if (!allocator) {
		return NULL;
	}

	edge_fiber_context_t* ctx = (edge_fiber_context_t*)edge_allocator_calloc(allocator, 1, sizeof(edge_fiber_context_t));
	if (!ctx) {
		return NULL;
	}

    ctx->stack_ptr = stack_ptr;
    ctx->stack_size = stack_size;

    if (ctx->stack_ptr) {
        void* stack_top = (char*)ctx->stack_ptr + ctx->stack_size;
        stack_top = (void*)(((uintptr_t)stack_top) & ~0xFULL);

        stack_top = (char*)stack_top - 8;
        *(void**)stack_top = (void*)entry;
        stack_top = (char*)stack_top - 8;
        *(void**)stack_top = (void*)fiber_abort;

#if defined(__x86_64__)
        ctx->rip = (void*)fiber_main;
        ctx->rsp = stack_top;
#elif defined(__aarch64__)
        ctx->lr = fiber_main;
        ctx->sp = stack_top;
#endif
    }

    // For main fiber
    if (!entry && !stack_ptr) {
        ctx->tsan_fiber = __tsan_get_current_fiber();
        ctx->main_fiber = true;
    }
    else {
        ctx->tsan_fiber = __tsan_create_fiber(0);
    }

    return ctx;
}

void edge_fiber_context_destroy(edge_allocator_t* allocator, edge_fiber_context_t* ctx) {
	if (!allocator || !ctx) {
		return;
	}

    if (ctx->tsan_fiber && ctx->main_fiber) {
        __tsan_destroy_fiber(ctx->tsan_fiber);
    }

	edge_allocator_free(allocator, ctx);
}

void* edge_fiber_get_stack_ptr(edge_fiber_context_t* ctx) {
    return ctx->stack_ptr;
}

size_t edge_fiber_get_stack_size(edge_fiber_context_t* ctx) {
    return ctx->stack_size;
}

bool edge_fiber_context_switch(edge_fiber_context_t* from, edge_fiber_context_t* to) {
	if (!from || !to) {
		return false;
	}

    __tsan_switch_to_fiber(to->tsan_fiber, 0);

    void* fake_stack = NULL;
    __sanitizer_start_switch_fiber(&fake_stack, (char*)to->stack_ptr, to->stack_size);

    atomic_signal_fence(memory_order_release);
    fiber_swap_context_internal(from, to);
    atomic_signal_fence(memory_order_acquire);

    __sanitizer_finish_switch_fiber(fake_stack, NULL, 0);

	return true;
}