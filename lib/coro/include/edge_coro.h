#ifndef EDGE_CORO_H
#define EDGE_CORO_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

	/* Forward declarations */
	typedef struct edge_allocator edge_allocator_t;
	typedef struct edge_coro edge_coro_t;

	/* Coroutine function signature */
	typedef void (*edge_coro_fn)(void* arg);

	/* Coroutine status */
	typedef enum {
		EDGE_CORO_STATE_READY,      /* Ready to run */
		EDGE_CORO_STATE_RUNNING,    /* Currently executing */
		EDGE_CORO_STATE_SUSPENDED,  /* Suspended, waiting to resume */
		EDGE_CORO_STATE_FINISHED   /* Execution completed */
	} edge_coro_status_t;

	/* Create a new coroutine */
	edge_coro_t* edge_coro_create(edge_allocator_t* allocator, edge_coro_fn function, void* arg, size_t stack_size);

	/* Destroy a coroutine */
	void edge_coro_destroy(edge_coro_t* coro);

	/* Resume a suspended coroutine */
	bool edge_coro_resume(edge_coro_t* coro);

	/* Yield from current coroutine back to caller */
	void edge_coro_yield(void);

	/* Get current running coroutine */
	edge_coro_t* edge_coro_current(void);

	/* Get coroutine status */
	edge_coro_status_t edge_coro_status(edge_coro_t* coro);

	/* Check if coroutine is alive (can be resumed) */
	bool edge_coro_alive(edge_coro_t* coro);

#ifdef __cplusplus
}
#endif

#endif /* EDGE_CORO_H */