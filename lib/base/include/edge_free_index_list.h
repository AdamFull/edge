#ifndef EDGE_FREE_INDEX_LIST_H
#define EDGE_FREE_INDEX_LIST_H

#include "edge_base.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

	typedef struct edge_allocator edge_allocator_t;
    typedef struct edge_free_list edge_free_list_t;

	/**
	 * Create a free list with fixed capacity
	 * Initially all indices [0, capacity-1] are available
	 *
	 * @param allocator Memory allocator to use
	 * @param capacity Number of indices to manage
	 * @return Initialized free list or NULL on failure
	 */
	edge_free_list_t* edge_free_list_create(const edge_allocator_t* allocator, u32 capacity);

	/**
	 * Destroy free list and free memory
	 */
	void edge_free_list_destroy(edge_free_list_t* list);

	/**
	 * Allocate (pop) a free index
	 *
	 * @param list Free list
	 * @param out_index Pointer to store the allocated index
	 * @return true if successful, false if no free indices available
	 */
	bool edge_free_list_allocate(edge_free_list_t* list, u32* out_index);

	/**
	 * Free (return) an index back to the free list
	 * Note: Does not check for double-free
	 *
	 * @param list Free list
	 * @param index Index to return
	 * @return true if successful, false if list is full or invalid
	 */
	bool edge_free_list_free(edge_free_list_t* list, u32 index);

	/**
	 * Get number of available free indices
	 */
	u32 edge_free_list_available(const edge_free_list_t* list);

	/**
	 * Get total capacity
	 */
	u32 edge_free_list_capacity(const edge_free_list_t* list);

	/**
	 * Check if any free indices are available
	 */
	bool edge_free_list_has_available(const edge_free_list_t* list);

	/**
	 * Check if free list is full (all indices are free)
	 */
	bool edge_free_list_is_full(const edge_free_list_t* list);

	/**
	 * Check if free list is empty (no free indices available)
	 */
	bool edge_free_list_is_empty(const edge_free_list_t* list);

	/**
	 * Reset the free list to initial state (all indices available)
	 */
	void edge_free_list_reset(edge_free_list_t* list);

	/**
	 * Clear the free list (marks all indices as allocated)
	 */
	void edge_free_list_clear(edge_free_list_t* list);

#ifdef __cplusplus
}
#endif

#endif