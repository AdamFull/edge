#ifndef EDGE_FIBER_H
#define EDGE_FIBER_H

#include "allocator.hpp"

#ifndef EDGE_FIBER_STACK_SIZE
#if defined(TSAN_ENABLED)
#define EDGE_FIBER_TSAN_STACK_SIZE (256 * 1024)
#define EDGE_FIBER_SMALL_STACK_SIZE ((16 * 1024) + EDGE_FIBER_TSAN_STACK_SIZE)
#define EDGE_FIBER_MEDIUM_STACK_SIZE ((64 * 1024) + EDGE_FIBER_TSAN_STACK_SIZE)
#define EDGE_FIBER_BIG_STACK_SIZE ((256 * 1024) + EDGE_FIBER_TSAN_STACK_SIZE)

#define EDGE_FIBER_STACK_SIZE EDGE_FIBER_MEDIUM_STACK_SIZE
#else
#define EDGE_FIBER_SMALL_STACK_SIZE (16 * 1024)
#define EDGE_FIBER_MEDIUM_STACK_SIZE (64 * 1024)
#define EDGE_FIBER_BIG_STACK_SIZE (256 * 1024)

#define EDGE_FIBER_STACK_SIZE EDGE_FIBER_MEDIUM_STACK_SIZE
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

	FiberContext* fiber_context_create(NotNull<const Allocator*> allocator, FiberEntryFn entry, void* stack_ptr, usize stack_size);
	void fiber_context_destroy(NotNull<const Allocator*> allocator, FiberContext* context);

	void* fiber_get_stack_ptr(FiberContext* ctx);
	usize fiber_get_stack_size(FiberContext* ctx);

	bool fiber_context_switch(FiberContext* from, FiberContext* to);
}

#endif /* EDGE_FIBER_H */
