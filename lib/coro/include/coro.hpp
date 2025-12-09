#ifndef EDGE_CORO_HPP
#define EDGE_CORO_HPP

#include "stddef.hpp"
#include "allocator.hpp"

namespace edge {
	struct Coro;

	using CoroFn = void(*)(void* arg);

	enum class CoroState {
		Running = 1,
		Suspended = 2,
		Finished = 3
	};

	void coro_init_thread_context(const Allocator* allocator);
	void coro_shutdown_thread_context(void);

	Coro* coro_create(CoroFn function, void* payload);
	void coro_destroy(Coro* coro);

	bool coro_resume(Coro* coro);
	void coro_yield(void);

	Coro* coro_current(void);
	CoroState coro_state(Coro* coro);
	bool coro_alive(Coro* coro);
}

#endif