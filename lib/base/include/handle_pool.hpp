#ifndef EDGE_HANDLE_POOL_H
#define EDGE_HANDLE_POOL_H

#include "array.hpp"

namespace edge {
    struct Allocator;

#ifdef EDGE_HANDLE_USE_32BIT
    using Handle = u32;
    using HandleVersion = u16;
    constexpr u32 HANDLE_INDEX_BITS = 20;
    constexpr u32 HANDLE_VERSION_BITS = 12;
    constexpr Handle HANDLE_INVALID = ~0u;
#else
    using Handle = u64;
    using HandleVersion = u32;
    constexpr u32 HANDLE_INDEX_BITS = 32;
    constexpr u32 HANDLE_VERSION_BITS = 32;
    constexpr Handle HANDLE_INVALID = ~0ull;
#endif

    constexpr u64 HANDLE_INDEX_MASK = (1ull << HANDLE_INDEX_BITS) - 1;
    constexpr u64 HANDLE_VERSION_MASK = (1ull << HANDLE_VERSION_BITS) - 1;
    constexpr u32 HANDLE_MAX_CAPACITY = static_cast<u32>(HANDLE_INDEX_MASK);

    template<TrivialType T>
    struct HandlePool {
        T* m_data = nullptr;
        HandleVersion* m_versions = nullptr;
        Array<u32> m_free_indices;
        u32 m_capacity = 0ull;
        u32 m_count = 0ull;
        const Allocator* m_allocator = nullptr;
    };

    /**
     * Create a handle from index and version
     */
    inline Handle handle_make(u32 index, u32 version) {
        return ((static_cast<Handle>(index & HANDLE_INDEX_MASK) << HANDLE_VERSION_BITS) |
            (static_cast<Handle>(version & HANDLE_VERSION_MASK)));
    }

    /**
     * Extract index from handle
     */
    inline u32 handle_get_index(Handle handle) {
        return static_cast<u32>((handle >> HANDLE_VERSION_BITS) & HANDLE_INDEX_MASK);
    }

    /**
     * Extract version from handle
     */
    inline HandleVersion handle_get_version(Handle handle) {
        return static_cast<HandleVersion>(handle & HANDLE_VERSION_MASK);
    }

    template<TrivialType T>
    struct HandlePoolIterator {
        HandlePool<T>* m_pool;
        u32 m_current_index;

        struct Entry {
            Handle handle;
            T* element;
        };

        HandlePoolIterator& operator++() {
            if (m_pool) {
                m_current_index++;
                while (m_current_index < m_pool->m_capacity) {
                    Handle handle = handle_make(m_current_index, m_pool->m_versions[m_current_index]);
                    if (handle_pool_is_valid(m_pool, handle)) {
                        break;
                    }
                    m_current_index++;
                }
            }
            return *this;
        }

        Entry operator*() const {
            Handle handle = handle_make(m_current_index, m_pool->m_versions[m_current_index]);
            return { handle, &m_pool->m_data[m_current_index] };
        }

        bool operator!=(const HandlePoolIterator& other) const {
            return m_current_index != other.m_current_index || m_pool != other.m_pool;
        }

        bool operator==(const HandlePoolIterator& other) const {
            return m_current_index == other.m_current_index && m_pool == other.m_pool;
        }
    };

    template<TrivialType T>
    struct HandlePoolConstIterator {
        const HandlePool<T>* m_pool;
        u32 m_current_index;

        struct Entry {
            Handle handle;
            const T* element;
        };

        HandlePoolConstIterator& operator++() {
            if (m_pool) {
                m_current_index++;
                while (m_current_index < m_pool->m_capacity) {
                    Handle handle = handle_make(m_current_index, m_pool->m_versions[m_current_index]);
                    if (handle_pool_is_valid(m_pool, handle)) {
                        break;
                    }
                    m_current_index++;
                }
            }
            return *this;
        }

        Entry operator*() const {
            Handle handle = handle_make(m_current_index, m_pool->m_versions[m_current_index]);
            return { handle, &m_pool->m_data[m_current_index] };
        }

        bool operator!=(const HandlePoolConstIterator& other) const {
            return m_current_index != other.m_current_index || m_pool != other.m_pool;
        }

        bool operator==(const HandlePoolConstIterator& other) const {
            return m_current_index == other.m_current_index && m_pool == other.m_pool;
        }
    };

    /**
     * Create a handle pool
     *
     * @param alloc Memory allocator to use
     * @param pool Pointer to pool to initialize
     * @param capacity Maximum number of handles (must be <= HANDLE_MAX_CAPACITY)
     * @return true on success, false on failure
     */
    template<TrivialType T>
    bool handle_pool_create(const Allocator* alloc, HandlePool<T>* pool, u32 capacity) {
        if (!alloc || !pool || capacity == 0 || capacity > HANDLE_MAX_CAPACITY) {
            return false;
        }

        pool->m_data = allocate_array<T>(alloc, capacity);
        if (!pool->m_data) {
            return false;
        }

        pool->m_versions = allocate_array<HandleVersion>(alloc, capacity);
        if (!pool->m_versions) {
            deallocate_array(alloc, pool->m_data, pool->m_capacity);
            return false;
        }

        if (!pool->m_free_indices.reserve(alloc, capacity)) {
            deallocate_array(alloc, pool->m_versions, pool->m_capacity);
            deallocate_array(alloc, pool->m_data, pool->m_capacity);;
            return false;
        }

        pool->m_capacity = capacity;
        pool->m_count = 0;
        pool->m_allocator = alloc;

        // Initialize free indices in reverse order (so 0 is allocated first)
        for (u32 i = 0; i < capacity; i++) {
            u32 index = capacity - 1 - i;
            pool->m_free_indices.push_back(alloc, index);
        }

        return true;
    }

    /**
     * Destroy handle pool and free all resources
     */
    template<TrivialType T>
    void handle_pool_destroy(HandlePool<T>* pool) {
        if (!pool) {
            return;
        }

        pool->m_free_indices.destroy(pool->m_allocator);

        if (pool->m_versions) {
            deallocate_array(pool->m_allocator, pool->m_versions, pool->m_capacity);
        }

        if (pool->m_data) {
            deallocate_array(pool->m_allocator, pool->m_data, pool->m_capacity);
        }
    }

    /**
     * Allocate a new unique handle
     * The element data is zeroed initially
     *
     * @param pool Handle pool
     * @return Valid handle or HANDLE_INVALID if pool is full
     */
    template<TrivialType T>
    Handle handle_pool_allocate(HandlePool<T>* pool) {
        if (!pool || pool->m_free_indices.empty()) {
            return HANDLE_INVALID;
        }

        u32 index;
        if (!pool->m_free_indices.pop_back(&index)) {
            return HANDLE_INVALID;
        }

        memset(&pool->m_data[index], 0, sizeof(T));

        HandleVersion version = pool->m_versions[index];
        pool->m_count++;

        return handle_make(index, version);
    }

    /**
     * Allocate a new unique handle with initial data
     *
     * @param pool Handle pool
     * @param element Initial data to copy into the slot
     * @return Valid handle or HANDLE_INVALID if pool is full
     */
    template<TrivialType T>
    Handle handle_pool_allocate_with_data(HandlePool<T>* pool, const T& element) {
        if (!pool || pool->m_free_indices.empty()) {
            return HANDLE_INVALID;
        }

        u32 index;
        if (!pool->m_free_indices.pop_back(&index)) {
            return HANDLE_INVALID;
        }

        memcpy(&pool->m_data[index], &element, sizeof(T));

        HandleVersion version = pool->m_versions[index];
        pool->m_count++;

        return handle_make(index, version);
    }

    /**
     * Free a handle and return it to the pool
     * Version is incremented to invalidate old handles
     *
     * @param pool Handle pool
     * @param handle Handle to free
     * @return true if successful, false if handle is invalid or already freed
     */
    template<TrivialType T>
    bool handle_pool_free(HandlePool<T>* pool, Handle handle) {
        if (!pool || handle == HANDLE_INVALID) {
            return false;
        }

        u32 index = handle_get_index(handle);
        HandleVersion version = handle_get_version(handle);

        if (index >= pool->m_capacity) {
            return false;
        }

        if (pool->m_versions[index] != version) {
            return false;
        }

        // Increment version (wrapping around within version mask)
        pool->m_versions[index] = (pool->m_versions[index] + 1) & HANDLE_VERSION_MASK;

        // Clear the element data
        memset(&pool->m_data[index], 0, sizeof(T));

        pool->m_free_indices.push_back(pool->m_allocator, index);
        pool->m_count--;

        return true;
    }

    /**
     * Get element by handle with validation
     *
     * @param pool Handle pool
     * @param handle Handle to look up
     * @return Pointer to element or nullptr if handle is invalid/stale
     */
    template<TrivialType T>
    T* handle_pool_get(HandlePool<T>* pool, Handle handle) {
        if (!pool || handle == HANDLE_INVALID) {
            return nullptr;
        }

        u32 index = handle_get_index(handle);
        HandleVersion version = handle_get_version(handle);

        if (index >= pool->m_capacity) {
            return nullptr;
        }

        if (pool->m_versions[index] != version) {
            return nullptr;
        }

        return &pool->m_data[index];
    }

    /**
     * Get element by handle with validation (const version)
     */
    template<TrivialType T>
    const T* handle_pool_get(const HandlePool<T>* pool, Handle handle) {
        if (!pool || handle == HANDLE_INVALID) {
            return nullptr;
        }

        u32 index = handle_get_index(handle);
        HandleVersion version = handle_get_version(handle);

        if (index >= pool->m_capacity) {
            return nullptr;
        }

        if (pool->m_versions[index] != version) {
            return nullptr;
        }

        return &pool->m_data[index];
    }

    /**
     * Set element data by handle
     *
     * @param pool Handle pool
     * @param handle Handle to update
     * @param element Data to copy into the slot
     * @return true if successful, false if handle is invalid
     */
    template<TrivialType T>
    bool handle_pool_set(HandlePool<T>* pool, Handle handle, const T& element) {
        if (!pool || handle == HANDLE_INVALID) {
            return false;
        }

        u32 index = handle_get_index(handle);
        HandleVersion version = handle_get_version(handle);

        if (index >= pool->m_capacity) {
            return false;
        }

        if (pool->m_versions[index] != version) {
            return false;
        }

        memcpy(&pool->m_data[index], &element, sizeof(T));
        return true;
    }

    /**
     * Check if a handle is valid
     *
     * @param pool Handle pool
     * @param handle Handle to validate
     * @return true if handle is valid and active
     */
    template<TrivialType T>
    bool handle_pool_is_valid(const HandlePool<T>* pool, Handle handle) {
        if (!pool || handle == HANDLE_INVALID) {
            return false;
        }

        u32 index = handle_get_index(handle);
        HandleVersion version = handle_get_version(handle);

        if (index >= pool->m_capacity) {
            return false;
        }

        return pool->m_versions[index] == version;
    }

    /**
     * Get number of active handles
     */
    template<TrivialType T>
    u32 handle_pool_count(const HandlePool<T>* pool) {
        return pool ? pool->m_count : 0;
    }

    /**
     * Get total pool capacity
     */
    template<TrivialType T>
    u32 handle_pool_capacity(const HandlePool<T>* pool) {
        return pool ? pool->m_capacity : 0;
    }

    /**
     * Check if pool is full
     */
    template<TrivialType T>
    bool handle_pool_is_full(const HandlePool<T>* pool) {
        return pool ? pool->m_free_indices.empty() : true;
    }

    /**
     * Check if pool is empty
     */
    template<TrivialType T>
    bool handle_pool_is_empty(const HandlePool<T>* pool) {
        return pool ? (pool->m_count == 0) : true;
    }

    /**
     * Clear all handles (frees all resources and increments versions)
     */
    template<TrivialType T>
    void handle_pool_clear(HandlePool<T>* pool) {
        if (!pool) {
            return;
        }

        array_clear(&pool->m_free_indices);

        // Re-initialize free indices and increment versions
        for (u32 i = 0; i < pool->m_capacity; i++) {
            u32 index = pool->m_capacity - 1 - i;
            array_push_back(&pool->m_free_indices, index);

            pool->m_versions[i] = (pool->m_versions[i] + 1) & HANDLE_VERSION_MASK;

            memset(&pool->m_data[i], 0, sizeof(T));
        }

        pool->m_count = 0;
    }

    /**
     * Get iterator to first active handle
     */
    template<TrivialType T>
    HandlePoolIterator<T> begin(HandlePool<T>& pool) {
        u32 index = 0;
        while (index < pool.m_capacity) {
            Handle handle = handle_make(index, pool.m_versions[index]);
            if (handle_pool_is_valid(&pool, handle)) {
                break;
            }
            index++;
        }
        return { &pool, index };
    }

    /**
     * Get iterator past the last active handle
     */
    template<TrivialType T>
    HandlePoolIterator<T> end(HandlePool<T>& pool) {
        return { &pool, pool.m_capacity };
    }

    /**
     * Get const iterator to first active handle
     */
    template<TrivialType T>
    HandlePoolConstIterator<T> begin(const HandlePool<T>& pool) {
        u32 index = 0;
        while (index < pool.m_capacity) {
            Handle handle = handle_make(index, pool.m_versions[index]);
            if (handle_pool_is_valid(&pool, handle)) {
                break;
            }
            index++;
        }
        return { &pool, index };
    }

    /**
     * Get const iterator past the last active handle
     */
    template<TrivialType T>
    HandlePoolConstIterator<T> end(const HandlePool<T>& pool) {
        return { &pool, pool.m_capacity };
    }
}

#endif
