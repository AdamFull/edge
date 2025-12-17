#include "coro.hpp"
#include "fiber.hpp"
#include "arena.hpp"
#include "array.hpp"

#include <atomic>
#include <cstring>
#include <cassert>

namespace edge {
	struct Coro {
		FiberContext* context;
		CoroFn func;
		void* user_data;
		CoroState state;
		Coro* caller;
	};

	struct CoroThreadContext {
		const Allocator* allocator;

		Arena stack_arena;
		Array<uintptr_t> free_stacks;

		Coro* current_coro;
		Coro main_coro;
		FiberContext* main_context;
	};

	static thread_local CoroThreadContext thread_context = { };

	static void* coro_malloc(usize size) {
		if (thread_context.main_coro.state == CoroState{}) {
			return nullptr;
		}

		return thread_context.allocator->malloc(size);
	}

	static void coro_free(void* ptr) {
		if (!ptr || thread_context.main_coro.state == CoroState{}) {
			return;
		}

		return thread_context.allocator->free(ptr);
	}

	extern "C" void coro_main(void) {
		Coro* coro = thread_context.current_coro;

		if (coro && coro->func) {
			coro->state = CoroState::Running;
			coro->func(coro->user_data);
			coro->state = CoroState::Finished;
		}

		if (coro->caller) {
			fiber_context_switch(coro->context, coro->caller->context);
		}

		assert(0 && "Coroutine returned without caller");
	}

	static void* coro_alloc_stack_pointer() {
		CoroThreadContext* ctx = &thread_context;

		uintptr_t ptr_addr = 0x0;
		if (!ctx->free_stacks.pop_back(&ptr_addr)) {
			return arena_alloc_ex(&thread_context.stack_arena, EDGE_FIBER_STACK_SIZE, EDGE_FIBER_STACK_ALIGN);
		}

		return reinterpret_cast<void*>(ptr_addr);
	}

	static void coro_free_stack_pointer(void* ptr) {
		if (!ptr) {
			return;
		}

		CoroThreadContext* ctx = &thread_context;

		uintptr_t ptr_addr = reinterpret_cast<uintptr_t>(ptr);
		if (!ctx->free_stacks.push_back(ctx->allocator, ptr_addr)) {
			return;
		}
	}

	void coro_init_thread_context(const Allocator* allocator) {
		if (thread_context.main_coro.state == CoroState{}) {
			arena_create(allocator, &thread_context.stack_arena, 0);

			thread_context.free_stacks.reserve(allocator, 16);

			thread_context.allocator = allocator;
			thread_context.main_context = fiber_context_create(allocator, nullptr, nullptr, 0);
			thread_context.main_coro.state = CoroState::Running;
			thread_context.main_coro.context = thread_context.main_context;
			thread_context.current_coro = &thread_context.main_coro;
		}
	}

	void coro_shutdown_thread_context(void) {
		if (thread_context.main_coro.state != CoroState{}) {
			if (thread_context.main_context) {
				fiber_context_destroy(thread_context.allocator, thread_context.main_context);
			}

			thread_context.free_stacks.destroy(thread_context.allocator);
			arena_destroy(&thread_context.stack_arena);
		}
	}

	Coro* coro_create(CoroFn function, void* arg) {
		if (!function) {
			return nullptr;
		}

		Coro* coro = static_cast<Coro*>(coro_malloc(sizeof(Coro)));
		if (!coro) {
			return nullptr;
		}

		memset(coro, 0, sizeof(Coro));

		void* stack_ptr = coro_alloc_stack_pointer();
		if (!stack_ptr) {
			coro_free(coro);
			return nullptr;
		}

		coro->context = fiber_context_create(thread_context.allocator, coro_main, stack_ptr, EDGE_FIBER_STACK_SIZE);
		if (!coro->context) {
			coro_free_stack_pointer(stack_ptr);
			coro_free(coro);
			return nullptr;
		}

		coro->state = CoroState::Suspended;
		coro->caller = nullptr;
		coro->func = function;
		coro->user_data = arg;

		return coro;
	}

	void coro_destroy(Coro* coro) {
		if (!coro) {
			return;
		}

		if (coro->context) {
			void* stack_ptr = fiber_get_stack_ptr(coro->context);
			if (stack_ptr) {
				coro_free_stack_pointer(stack_ptr);
			}

			fiber_context_destroy(thread_context.allocator, coro->context);
		}

		coro_free(coro);
	}

	bool coro_resume(Coro* coro) {
		if (!coro || coro->state != CoroState::Suspended) {
			return false;
		}

		Coro* caller = coro_current();
		coro->caller = caller;

		caller->state = CoroState::Suspended;
		coro->state = CoroState::Running;

		Coro* volatile target_coro = coro;
		thread_context.current_coro = target_coro;

		fiber_context_switch(caller->context, coro->context);

		thread_context.current_coro = caller;
		caller->state = CoroState::Running;

		return coro->state != CoroState::Finished;
	}

	void coro_yield(void) {
		CoroThreadContext* ctx = &thread_context;

		Coro* coro = ctx->current_coro;
		if (!coro || coro == &ctx->main_coro || coro->state == CoroState::Finished) {
			return;
		}

		Coro* caller = coro->caller;
		if (!caller) {
			return;
		}

		coro->state = CoroState::Suspended;
		caller->state = CoroState::Running;

		fiber_context_switch(coro->context, caller->context);

		coro->state = CoroState::Running;
	}

	Coro* coro_current(void) {
		return thread_context.current_coro;
	}

	CoroState coro_state(Coro* coro) {
		return coro ? coro->state : CoroState::Finished;
	}

	bool coro_alive(Coro* coro) {
		return coro && coro->state != CoroState::Finished;
	}
}