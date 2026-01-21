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
    struct HandlePool;

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
                    if (m_pool->is_valid(handle)) {
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

    template<TrivialType T>
    struct HandlePool {
        T* m_data = nullptr;
        HandleVersion* m_versions = nullptr;
        Array<u32> m_free_indices;
        u32 m_capacity = 0ull;
        u32 m_count = 0ull;

        bool create(NotNull<const Allocator*> alloc, u32 capacity) {
            if (capacity == 0 || capacity > HANDLE_MAX_CAPACITY) {
                return false;
            }

            m_data = alloc->allocate_array<T>(capacity);
            if (!m_data) {
                return false;
            }

            m_versions = alloc->allocate_array<HandleVersion>(capacity);
            if (!m_versions) {
                alloc->deallocate_array(m_data, m_capacity);
                return false;
            }

            if (!m_free_indices.reserve(alloc, capacity)) {
                alloc->deallocate_array(m_versions, m_capacity);
                alloc->deallocate_array(m_data, m_capacity);;
                return false;
            }

            m_capacity = capacity;
            m_count = 0;

            // Initialize free indices in reverse order (so 0 is allocated first)
            for (u32 i = 0; i < capacity; i++) {
                u32 index = capacity - 1 - i;
                m_free_indices.push_back(alloc, index);
            }

            return true;
        }

        void destroy(NotNull<const Allocator*> alloc) {
            m_free_indices.destroy(alloc);

            if (m_versions) {
                alloc->deallocate_array(m_versions, m_capacity);
            }

            if (m_data) {
                alloc->deallocate_array(m_data, m_capacity);
            }
        }

        Handle allocate() {
            if (m_free_indices.empty()) {
                return HANDLE_INVALID;
            }

            u32 index;
            if (!m_free_indices.pop_back(&index)) {
                return HANDLE_INVALID;
            }

            memset(&m_data[index], 0, sizeof(T));

            HandleVersion version = m_versions[index];
            m_count++;

            return handle_make(index, version);
        }

        Handle allocate_with_data(const T& element) {
            if (m_free_indices.empty()) {
                return HANDLE_INVALID;
            }

            u32 index;
            if (!m_free_indices.pop_back(&index)) {
                return HANDLE_INVALID;
            }

            memcpy(&m_data[index], &element, sizeof(T));

            HandleVersion version = m_versions[index];
            m_count++;

            return handle_make(index, version);
        }

        bool free(NotNull<const Allocator*> alloc, Handle handle) {
            if (handle == HANDLE_INVALID) {
                return false;
            }

            u32 index = handle_get_index(handle);
            HandleVersion version = handle_get_version(handle);

            if (index >= m_capacity) {
                return false;
            }

            if (m_versions[index] != version) {
                return false;
            }

            // Increment version (wrapping around within version mask)
            m_versions[index] = (m_versions[index] + 1) & HANDLE_VERSION_MASK;

            // Clear the element data
            memset(&m_data[index], 0, sizeof(T));

            m_free_indices.push_back(alloc, index);
            m_count--;

            return true;
        }

        T* get(Handle handle) {
            if (handle == HANDLE_INVALID) {
                return nullptr;
            }

            u32 index = handle_get_index(handle);
            HandleVersion version = handle_get_version(handle);

            if (index >= m_capacity) {
                return nullptr;
            }

            if (m_versions[index] != version) {
                return nullptr;
            }

            return &m_data[index];
        }

        const T* get(Handle handle) const {
            if (handle == HANDLE_INVALID) {
                return nullptr;
            }

            u32 index = handle_get_index(handle);
            HandleVersion version = handle_get_version(handle);

            if (index >= m_capacity) {
                return nullptr;
            }

            if (m_versions[index] != version) {
                return nullptr;
            }

            return &m_data[index];
        }

        bool set(Handle handle, const T& element) {
            if (handle == HANDLE_INVALID) {
                return false;
            }

            u32 index = handle_get_index(handle);
            HandleVersion version = handle_get_version(handle);

            if (index >= m_capacity) {
                return false;
            }

            if (m_versions[index] != version) {
                return false;
            }

            memcpy(&m_data[index], &element, sizeof(T));
            return true;
        }

        bool is_valid(Handle handle) const {
            if (handle == HANDLE_INVALID) {
                return false;
            }

            u32 index = handle_get_index(handle);
            HandleVersion version = handle_get_version(handle);

            if (index >= m_capacity) {
                return false;
            }

            return m_versions[index] == version;
        }

        bool is_full() const {
            return m_free_indices.empty();
        }

        bool is_empty() const {
            return m_count == 0;
        }

        void clear(NotNull<const Allocator*> alloc) {
            m_free_indices.clear();

            // Re-initialize free indices and increment versions
            for (u32 i = 0; i < m_capacity; i++) {
                u32 index = m_capacity - 1 - i;
                m_free_indices.push_back(alloc, index);

                m_versions[i] = (m_versions[i] + 1) & HANDLE_VERSION_MASK;

                memset(&m_data[i], 0, sizeof(T));
            }

            m_count = 0;
        }
    };

    /**
     * Get iterator to first active handle
     */
    template<TrivialType T>
    HandlePoolIterator<T> begin(HandlePool<T>& pool) {
        u32 index = 0;
        while (index < pool.m_capacity) {
            Handle handle = handle_make(index, pool.m_versions[index]);
            if (pool.is_valid(handle)) {
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
            if (pool.is_valid(handle)) {
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
