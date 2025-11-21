#ifndef EDGE_CORO_H
#define EDGE_CORO_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

	typedef struct edge_allocator edge_allocator_t;

	/* Forward declarations */
	typedef struct edge_coro edge_coro_t;

	/* Coroutine function signature */
	typedef void (*edge_coro_fn)(void* arg);

	/* Coroutine status */
	typedef enum {
		EDGE_CORO_STATE_RUNNING = 1,    /* Currently executing */
		EDGE_CORO_STATE_SUSPENDED = 2,  /* Suspended, waiting to resume */
		EDGE_CORO_STATE_FINISHED = 3   /* Execution completed */
	} edge_coro_state_t;

	/**
	 * @brief Create a global context for the current execution thread
	 * @param allocator Memory allocator
	 */
	void edge_coro_init_thread_context(edge_allocator_t* allocator);

	/**
	 * @brief Clear the global context for the current execution thread
	 */
	void edge_coro_shutdown_thread_context(void);

	/**
	 * @brief Create a new coroutine
	 * @param function coroutine callback
	 * @param payload pointer to data passed in to coroutine
	 * @return Coroutine handle
	 */
	edge_coro_t* edge_coro_create(edge_coro_fn function, void* payload);

	/**
	 * @brief Destroy a coroutine
	 * @param coro coroutine handle
	 */
	void edge_coro_destroy(edge_coro_t* coro);

	/**
	 * @brief Resume a suspended coroutine
	 * @param coro coroutine handle
	 * @return Resume result
	 */
	bool edge_coro_resume(edge_coro_t* coro);

	/**
	 * @brief Yield from current coroutine back to caller
	 */
	void edge_coro_yield(void);

	/**
	 * @brief Get current running coroutine
	 * @return Coroutine handle
	 */
	edge_coro_t* edge_coro_current(void);

	/**
	 * @brief Get coroutine state
	 * @param coro coroutine handle
	 * @return Coroutine state
	 */
	edge_coro_state_t edge_coro_state(edge_coro_t* coro);

	/**
	 * @brief Check if coroutine is alive (can be resumed)
	 * @param coro coroutine handle
	 * @return Is alive
	 */
	bool edge_coro_alive(edge_coro_t* coro);

#ifdef __cplusplus
}
#endif

#endif /* EDGE_CORO_H */
