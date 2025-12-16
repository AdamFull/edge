#ifndef EDGE_FREE_INDEX_LIST_H
#define EDGE_FREE_INDEX_LIST_H

#include "allocator.hpp"

namespace edge {
	struct FreeIndexList {
		u32* m_indices = nullptr;
		u32 m_capacity = 0ull;
		u32 m_count = 0ull;
		const Allocator* m_allocator = nullptr;
	};

	/**
	 * Create a free list with fixed capacity
	 * Initially all indices [0, capacity-1] are available
	 *
	 * @param alloc Memory allocator to use
	 * @param list Pointer to list to initialize
	 * @param capacity Number of indices to manage
	 * @return true on success, false on failure
	 */
	inline bool free_index_list_create(const Allocator* alloc, FreeIndexList* list, u32 capacity) {
		if (!alloc || !list || capacity == 0) {
			return false;
		}

		list->m_indices = allocate_array<u32>(alloc, capacity);
		if (!list->m_indices) {
			return false;
		}

		list->m_capacity = capacity;
		list->m_count = capacity;
		list->m_allocator = alloc;

		// Initialize with all indices available in reverse order
		// So index 0 is allocated first
		for (u32 i = 0; i < capacity; i++) {
			list->m_indices[i] = capacity - 1 - i;
		}

		return true;
	}

	/**
	 * Destroy free list and free memory
	 */
	inline void free_index_list_destroy(FreeIndexList* list) {
		if (!list) {
			return;
		}

		if (list->m_indices) {
			deallocate_array(list->m_allocator, list->m_indices, list->m_capacity);
		}
	}

	/**
	 * Allocate (pop) a free index
	 *
	 * @param list Free list
	 * @param out_index Pointer to store the allocated index
	 * @return true if successful, false if no free indices available
	 */
	inline bool free_index_list_allocate(FreeIndexList* list, u32* out_index) {
		if (!list || !out_index || list->m_count == 0) {
			return false;
		}

		*out_index = list->m_indices[--list->m_count];
		return true;
	}

	/**
	 * Free (return) an index back to the free list
	 * Note: Does not check for double-free
	 *
	 * @param list Free list
	 * @param index Index to return
	 * @return true if successful, false if list is full or invalid
	 */
	inline bool free_index_list_free(FreeIndexList* list, u32 index) {
		if (!list || list->m_count >= list->m_capacity) {
			return false;
		}

		if (index >= list->m_capacity) {
			return false;
		}

		list->m_indices[list->m_count++] = index;
		return true;
	}

	/**
	 * Get number of available free indices
	 */
	inline u32 free_index_list_available(const FreeIndexList* list) {
		return list ? list->m_count : 0;
	}

	/**
	 * Get total capacity
	 */
	inline u32 free_index_list_capacity(const FreeIndexList* list) {
		return list ? list->m_capacity : 0;
	}

	/**
	 * Check if any free indices are available
	 */
	inline bool free_index_list_has_available(const FreeIndexList* list) {
		return list && list->m_count > 0;
	}

	/**
	 * Check if free list is full (all indices are free)
	 */
	inline bool free_index_list_is_full(const FreeIndexList* list) {
		return list && list->m_count == list->m_capacity;
	}

	/**
	 * Check if free list is empty (no free indices available)
	 */
	inline bool free_index_list_is_empty(const FreeIndexList* list) {
		return !list || list->m_count == 0;
	}

	/**
	 * Reset the free list to initial state (all indices available)
	 */
	inline void free_index_list_reset(FreeIndexList* list) {
		if (!list) {
			return;
		}

		list->m_count = list->m_capacity;

		for (u32 i = 0; i < list->m_capacity; i++) {
			list->m_indices[i] = list->m_capacity - 1 - i;
		}
	}

	/**
	 * Clear the free list (marks all indices as allocated)
	 */
	inline void free_index_list_clear(FreeIndexList* list) {
		if (!list) {
			return;
		}

		list->m_count = 0;
	}
}

#endif