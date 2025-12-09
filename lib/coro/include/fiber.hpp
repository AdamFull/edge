#ifndef EDGE_FIBER_H
#define EDGE_FIBER_H

#include "stddef.hpp"
#include "allocator.hpp"

#ifndef EDGE_FIBER_STACK_SIZE
#if defined(TSAN_ENABLED)
#define EDGE_FIBER_STACK_SIZE ((512 + 64) * 1024)
#else
#define EDGE_FIBER_STACK_SIZE (64 * 1024)
#endif
#endif

#ifndef EDGE_FIBER_STACK_ALIGN
#define EDGE_FIBER_STACK_ALIGN 16
#endif

#ifndef EDGE_FIBER_CACHE_LINE_SIZE
#define EDGE_FIBER_CACHE_LINE_SIZE 64
#endif

namespace edge {
	struct FiberContext;

	using FiberEntryFn = void(*)();

	FiberContext* fiber_context_create(const Allocator* allocator, FiberEntryFn entry, void* stack_ptr, usize stack_size);
	void fiber_context_destroy(const Allocator* allocator, FiberContext* context);

	void* fiber_get_stack_ptr(FiberContext* ctx);
	usize fiber_get_stack_size(FiberContext* ctx);

	bool fiber_context_switch(FiberContext* from, FiberContext* to);
}

#endif /* EDGE_FIBER_H */
