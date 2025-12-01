#ifndef EDGE_HANDLE_POOL_H
#define EDGE_HANDLE_POOL_H

#include "edge_base.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

	typedef struct edge_allocator edge_allocator_t;
	typedef struct edge_vector edge_vector_t;

#ifdef EDGE_HANDLE_USE_32BIT
    typedef u32 edge_handle_t;
    typedef u16 edge_ver_t;
#define EDGE_HANDLE_INDEX_BITS 20
#define EDGE_HANDLE_VERSION_BITS 12
#define EDGE_HANDLE_INVALID ((edge_handle_t)(~0u))
#else
    typedef u64 edge_handle_t;
    typedef u32 edge_ver_t;
#define EDGE_HANDLE_INDEX_BITS 32
#define EDGE_HANDLE_VERSION_BITS 32
#define EDGE_HANDLE_INVALID ((edge_handle_t)(~0ull))
#endif

#define EDGE_HANDLE_INDEX_MASK ((1ull << EDGE_HANDLE_INDEX_BITS) - 1)
#define EDGE_HANDLE_VERSION_MASK ((1ull << EDGE_HANDLE_VERSION_BITS) - 1)
#define EDGE_HANDLE_MAX_CAPACITY ((u32)EDGE_HANDLE_INDEX_MASK)

    typedef bool (*edge_handle_visitor_fn)(edge_handle_t handle, void* element, void* user_data);

    typedef struct edge_handle_pool {
        void* data;
        edge_ver_t* versions;
        edge_vector_t* free_indices;
        size_t element_size;
        u32 capacity;
        u32 count;
        const edge_allocator_t* allocator;
    } edge_handle_pool_t;

    /**
     * Create a handle pool
     *
     * @param allocator Memory allocator to use
     * @param element_size Size of each element in bytes
     * @param capacity Maximum number of handles (must be <= EDGE_HANDLE_MAX_CAPACITY)
     * @return Initialized pool or NULL on failure
     */
    edge_handle_pool_t* edge_handle_pool_create(const edge_allocator_t* allocator, size_t element_size, u32 capacity);

    /**
     * Destroy handle pool and free all resources
     */
    void edge_handle_pool_destroy(edge_handle_pool_t* pool);

    /**
     * Allocate a new unique handle
     * The element data is zeroed initially
     *
     * @param pool Handle pool
     * @return Valid handle or EDGE_HANDLE_INVALID if pool is full
     */
    edge_handle_t edge_handle_pool_allocate(edge_handle_pool_t* pool);

    /**
     * Allocate a new unique handle with initial data
     *
     * @param pool Handle pool
     * @param element Initial data to copy into the slot
     * @return Valid handle or EDGE_HANDLE_INVALID if pool is full
     */
    edge_handle_t edge_handle_pool_allocate_with_data(edge_handle_pool_t* pool, const void* element);

    /**
     * Free a handle and return it to the pool
     * Version is incremented to invalidate old handles
     *
     * @param pool Handle pool
     * @param handle Handle to free
     * @return true if successful, false if handle is invalid or already freed
     */
    bool edge_handle_pool_free(edge_handle_pool_t* pool, edge_handle_t handle);

    /**
     * Get element by handle with validation
     *
     * @param pool Handle pool
     * @param handle Handle to look up
     * @return Pointer to element or NULL if handle is invalid/stale
     */
    void* edge_handle_pool_get(edge_handle_pool_t* pool, edge_handle_t handle);

    /**
     * Get element by handle with validation (const version)
     */
    const void* edge_handle_pool_get_const(const edge_handle_pool_t* pool, edge_handle_t handle);

    /**
     * Set element data by handle
     *
     * @param pool Handle pool
     * @param handle Handle to update
     * @param element Data to copy into the slot
     * @return true if successful, false if handle is invalid
     */
    bool edge_handle_pool_set(edge_handle_pool_t* pool, edge_handle_t handle, const void* element);

    /**
     * Check if a handle is valid
     *
     * @param pool Handle pool
     * @param handle Handle to validate
     * @return true if handle is valid and active
     */
    bool edge_handle_pool_is_valid(const edge_handle_pool_t* pool, edge_handle_t handle);

    /**
     * Get number of active handles
     */
    u32 edge_handle_pool_count(const edge_handle_pool_t* pool);

    /**
     * Get total pool capacity
     */
    u32 edge_handle_pool_capacity(const edge_handle_pool_t* pool);

    /**
     * Get element size
     */
    size_t edge_handle_pool_element_size(const edge_handle_pool_t* pool);

    /**
     * Check if pool is full
     */
    bool edge_handle_pool_is_full(const edge_handle_pool_t* pool);

    /**
     * Check if pool is empty
     */
    bool edge_handle_pool_is_empty(const edge_handle_pool_t* pool);

    /**
     * Clear all handles (frees all resources)
     */
    void edge_handle_pool_clear(edge_handle_pool_t* pool);

    /**
     * Iterate over all active (valid) handles in the pool
     * Calls the visitor function for each active handle
     *
     * @param pool Handle pool
     * @param visitor Callback function to invoke for each active handle
     * @param user_data User data to pass to the visitor function
     * @return Number of handles visited
     */
    u32 edge_handle_pool_foreach(edge_handle_pool_t* pool, edge_handle_visitor_fn visitor, void* user_data);

    /**
     * Iterate over all active (valid) handles in the pool (const version)
     * Calls the visitor function for each active handle
     *
     * @param pool Handle pool (const)
     * @param visitor Callback function to invoke for each active handle
     * @param user_data User data to pass to the visitor function
     * @return Number of handles visited
     */
    u32 edge_handle_pool_foreach_const(const edge_handle_pool_t* pool, edge_handle_visitor_fn visitor, void* user_data);

    /**
     * Create a handle from index and version
     */
    static inline edge_handle_t edge_handle_make(u32 index, u32 version) {
        return ((edge_handle_t)(index & EDGE_HANDLE_INDEX_MASK) << EDGE_HANDLE_VERSION_BITS) |
            ((edge_handle_t)(version & EDGE_HANDLE_VERSION_MASK));
    }

    /**
     * Extract index from handle
     */
    static inline u32 edge_handle_get_index(edge_handle_t handle) {
        return (u32)((handle >> EDGE_HANDLE_VERSION_BITS) & EDGE_HANDLE_INDEX_MASK);
    }

    /**
     * Extract version from handle
     */
    static inline edge_ver_t edge_handle_get_version(edge_handle_t handle) {
        return (edge_ver_t)(handle & EDGE_HANDLE_VERSION_MASK);
    }
#ifdef __cplusplus
}
#endif

#endif
