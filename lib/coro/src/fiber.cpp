#include "fiber.hpp"
#include "platform_detect.hpp"

#include <atomic>
#include <cstdlib>

#ifdef ASAN_ENABLED
extern "C" {
	void __sanitizer_start_switch_fiber(void** fake_stack_save, const void* bottom, usize size);
	void __sanitizer_finish_switch_fiber(void* fake_stack_save, const void** bottom_old, usize* size_old);
}
#else
extern "C" {
	void __sanitizer_start_switch_fiber(void** fake_stack_save, const void* bottom, usize size) {}
	void __sanitizer_finish_switch_fiber(void* fake_stack_save, const void** bottom_old, usize* size_old) {}
}
#endif

#ifdef TSAN_ENABLED
extern "C" {
	void* __tsan_get_current_fiber(void);
	void* __tsan_create_fiber(unsigned flags);
	void __tsan_destroy_fiber(void* fiber);
	void __tsan_switch_to_fiber(void* fiber, unsigned flags);
}
#else
extern "C" {
	void* __tsan_get_current_fiber(void) { return nullptr; }
	void* __tsan_create_fiber(unsigned flags) { return nullptr; }
	void __tsan_destroy_fiber(void* fiber) {}
	void __tsan_switch_to_fiber(void* fiber, unsigned flags) {}
}
#endif

namespace edge {
	struct FiberContext {
#if defined(__x86_64__) || defined(_M_X64)
		void* rip;
		void* rsp;
		void* rbp;
		void* rbx;
		void* r12;
		void* r13;
		void* r14;
		void* r15;
#ifdef _WIN32
		void* rdi;
		void* rsi;
		u64 xmm6[2];
		u64 xmm7[2];
		u64 xmm8[2];
		u64 xmm9[2];
		u64 xmm10[2];
		u64 xmm11[2];
		u64 xmm12[2];
		u64 xmm13[2];
		u64 xmm14[2];
		u64 xmm15[2];
#endif

#elif defined(__aarch64__) || defined(_M_ARM64)
		void* lr;
		void* sp;
		void* fp;
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

		u64 d8;
		u64 d9;
		u64 d10;
		u64 d11;
		u64 d12;
		u64 d13;
		u64 d14;
		u64 d15;
#else
#error "Unsupported architecture"
#endif

		void* stack_ptr;
		usize stack_size;

		void* tsan_fiber;
		bool main_fiber;
	};

	extern "C" void fiber_swap_context_internal(FiberContext* from, FiberContext* to);
	extern "C" void fiber_main();

	extern "C" void fiber_init(void) {
		std::atomic_signal_fence(std::memory_order_acquire);
	}

	extern "C" void fiber_abort(void) {
		std::abort();
	}

	FiberContext* fiber_context_create(const Allocator* allocator, FiberEntryFn entry, void* stack_ptr, usize stack_size) {
		if (!allocator) {
			return nullptr;
		}

		FiberContext* ctx = allocate_zeroed<FiberContext>(allocator, 1);
		if (!ctx) {
			return nullptr;
		}

		ctx->stack_ptr = stack_ptr;
		ctx->stack_size = stack_size;

		if (ctx->stack_ptr) {
			void* stack_top = static_cast<char*>(ctx->stack_ptr) + ctx->stack_size;
			stack_top = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(stack_top) & ~0xFULL);

			stack_top = static_cast<char*>(stack_top) - 8;
			*static_cast<void**>(stack_top) = reinterpret_cast<void*>(entry);
			stack_top = static_cast<char*>(stack_top) - 8;
			*static_cast<void**>(stack_top) = reinterpret_cast<void*>(fiber_abort);

#if defined(__x86_64__)
			ctx->rip = reinterpret_cast<void*>(fiber_main);
			ctx->rsp = stack_top;
#elif defined(__aarch64__)
			ctx->lr = fiber_main;
			ctx->sp = stack_top;
#endif
		}

		if (!entry && !stack_ptr) {
			ctx->tsan_fiber = __tsan_get_current_fiber();
			ctx->main_fiber = true;
		}
		else {
			ctx->tsan_fiber = __tsan_create_fiber(0);
		}

		return ctx;
	}

	void fiber_context_destroy(const Allocator* allocator, FiberContext* ctx) {
		if (!allocator || !ctx) {
			return;
		}

		if (ctx->tsan_fiber && !ctx->main_fiber) {
			__tsan_destroy_fiber(ctx->tsan_fiber);
		}

		deallocate(allocator, ctx);
	}

	void* fiber_get_stack_ptr(FiberContext* ctx) {
		return ctx->stack_ptr;
	}

	usize fiber_get_stack_size(FiberContext* ctx) {
		return ctx->stack_size;
	}

	bool fiber_context_switch(FiberContext* from, FiberContext* to) {
		if (!from || !to) {
			return false;
		}

		__tsan_switch_to_fiber(to->tsan_fiber, 0);

		void* fake_stack = nullptr;
		__sanitizer_start_switch_fiber(&fake_stack, static_cast<char*>(to->stack_ptr), to->stack_size);

		std::atomic_signal_fence(std::memory_order_release);
		fiber_swap_context_internal(from, to);
		std::atomic_signal_fence(std::memory_order_acquire);

		__sanitizer_finish_switch_fiber(fake_stack, nullptr, 0);

		return true;
	}
}