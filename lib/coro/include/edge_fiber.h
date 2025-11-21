#ifndef EDGE_FIBER_H
#define EDGE_FIBER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifndef EDGE_FIBER_STACK_SIZE
#if TSAN_ENABLED
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

#ifdef __cplusplus
extern "C" {
#endif

	typedef struct edge_allocator edge_allocator_t;
	typedef struct edge_fiber_context edge_fiber_context_t;

	typedef void (*edge_fiber_entry_fn)();

	edge_fiber_context_t* edge_fiber_context_create(edge_allocator_t* allocator, edge_fiber_entry_fn entry, void* stack_ptr, size_t stack_size);
	void edge_fiber_context_destroy(edge_allocator_t* allocator, edge_fiber_context_t* context);

	void* edge_fiber_get_stack_ptr(edge_fiber_context_t* ctx);
	size_t edge_fiber_get_stack_size(edge_fiber_context_t* ctx);

	bool edge_fiber_context_switch(edge_fiber_context_t* from, edge_fiber_context_t* to);

#ifdef __cplusplus
}
#endif

#endif /* EDGE_FIBER_H */
