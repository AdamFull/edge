#ifndef EDGE_HANDLE_POOL_H
#define EDGE_HANDLE_POOL_H

#include "array.hpp"

namespace edge {
    using HandleRawType = u32;
    using HandleIndexType = u32;
    using HandleVersionType = u16;
    constexpr u32 HANDLE_INDEX_BITS = 20;
    constexpr u32 HANDLE_VERSION_BITS = 12;

    struct Handle {
        u32 version : HANDLE_VERSION_BITS;
        u32 index : HANDLE_INDEX_BITS;

        constexpr Handle() : version(0), index(0) {}
        constexpr explicit Handle(HandleRawType raw)
            : version{ raw & ((1u << HANDLE_VERSION_BITS) - 1) }
            , index{ (raw >> HANDLE_VERSION_BITS) & ((1u << HANDLE_INDEX_BITS) - 1) } {
        }
        constexpr Handle(HandleIndexType idx, HandleVersionType ver)
            : version{ ver & ((1u << HANDLE_VERSION_BITS) - 1) }
            , index{ idx & ((1u << HANDLE_INDEX_BITS) - 1) } {
        }

        constexpr explicit operator HandleIndexType() const {
            return (static_cast<HandleIndexType>(index) << HANDLE_VERSION_BITS) | static_cast<HandleIndexType>(version);
        }

        constexpr bool operator==(const Handle& other) const { return index == other.index && version == other.version; }
        constexpr bool operator!=(const Handle& other) const { return !(*this == other); }

        constexpr bool is_invalid() const {
            return index == ((1u << HANDLE_INDEX_BITS) - 1) && version == ((1u << HANDLE_VERSION_BITS) - 1);
        }
    };

    constexpr Handle HANDLE_INVALID = Handle(~0u);
    constexpr u64 HANDLE_INDEX_MASK = (1ull << HANDLE_INDEX_BITS) - 1;
    constexpr u64 HANDLE_VERSION_MASK = (1ull << HANDLE_VERSION_BITS) - 1;
    constexpr u32 HANDLE_MAX_CAPACITY = static_cast<u32>(HANDLE_INDEX_MASK);

    template<TrivialType T>
    struct HandlePool {
        T* m_data = nullptr;
        HandleVersionType* m_versions = nullptr;
        Array<HandleIndexType> m_free_indices;
        u32 m_capacity = 0ull;
        u32 m_count = 0ull;

        struct Iterator {
            HandlePool<T>* m_pool;
            u32 m_current_index;

            struct Entry {
                Handle handle;
                T* element;
            };

            Iterator& operator++() {
                if (m_pool) {
                    m_current_index++;
                    while (m_current_index < m_pool->m_capacity) {
                        auto handle = Handle{ m_current_index, m_pool->m_versions[m_current_index] };
                        if (m_pool->is_valid(handle)) {
                            break;
                        }
                        m_current_index++;
                    }
                }
                return *this;
            }

            Entry operator*() const {
                auto handle = Handle{ m_current_index, m_pool->m_versions[m_current_index] };
                return { handle, &m_pool->m_data[m_current_index] };
            }

            bool operator!=(const Iterator& other) const { return m_current_index != other.m_current_index || m_pool != other.m_pool; }
            bool operator==(const Iterator& other) const { return m_current_index == other.m_current_index && m_pool == other.m_pool; }
        };

        struct ConstIterator {
            const HandlePool<T>* m_pool;
            HandleIndexType m_current_index;

            struct Entry {
                Handle handle;
                const T* element;
            };

            ConstIterator& operator++() {
                if (m_pool) {
                    m_current_index++;
                    while (m_current_index < m_pool->m_capacity) {
                        auto handle = Handle{ m_current_index, m_pool->m_versions[m_current_index] };
                        if (m_pool->is_valid(handle)) {
                            break;
                        }
                        m_current_index++;
                    }
                }
                return *this;
            }

            Entry operator*() const {
                auto handle = Handle{ m_current_index, m_pool->m_versions[m_current_index] };
                return { handle, &m_pool->m_data[m_current_index] };
            }

            bool operator!=(const ConstIterator& other) const { return m_current_index != other.m_current_index || m_pool != other.m_pool; }
            bool operator==(const ConstIterator& other) const { return m_current_index == other.m_current_index && m_pool == other.m_pool; }
        };

        bool create(NotNull<const Allocator*> alloc, u32 capacity) {
            if (capacity == 0 || capacity > HANDLE_MAX_CAPACITY) {
                return false;
            }

            m_data = alloc->allocate_array<T>(capacity);
            if (!m_data) {
                return false;
            }

            m_versions = alloc->allocate_array<HandleVersionType>(capacity);
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
            for (usize i = 0; i < capacity; i++) {
                usize index = capacity - 1 - i;
                m_free_indices.push_back(alloc, static_cast<HandleIndexType>(index));
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

            HandleIndexType index;
            if (!m_free_indices.pop_back(&index)) {
                return HANDLE_INVALID;
            }

            memset(&m_data[index], 0, sizeof(T));

            auto version = m_versions[index];
            m_count++;

            return Handle{ index, version };
        }

        Handle allocate_with_data(const T& element) {
            if (m_free_indices.empty()) {
                return HANDLE_INVALID;
            }

            HandleIndexType index;
            if (!m_free_indices.pop_back(&index)) {
                return HANDLE_INVALID;
            }

            memcpy(&m_data[index], &element, sizeof(T));

            auto version = m_versions[index];
            m_count++;

            return Handle{ index, version };
        }

        bool free(NotNull<const Allocator*> alloc, Handle handle) {
            if (handle == HANDLE_INVALID) {
                return false;
            }

            if (handle.index >= m_capacity) {
                return false;
            }

            if (m_versions[handle.index] != handle.version) {
                return false;
            }

            // Increment version (wrapping around within version mask)
            m_versions[handle.index] = (m_versions[handle.index] + 1) & HANDLE_VERSION_MASK;

            // Clear the element data
            memset(&m_data[handle.index], 0, sizeof(T));

            m_free_indices.push_back(alloc, handle.index);
            m_count--;

            return true;
        }

        T* get(Handle handle) {
            if (handle == HANDLE_INVALID) {
                return nullptr;
            }

            if (handle.index >= m_capacity) {
                return nullptr;
            }

            if (m_versions[handle.index] != handle.version) {
                return nullptr;
            }

            return &m_data[handle.index];
        }

        const T* get(Handle handle) const {
            if (handle == HANDLE_INVALID) {
                return nullptr;
            }

            if (handle.index >= m_capacity) {
                return nullptr;
            }

            if (m_versions[handle.index] != handle.version) {
                return nullptr;
            }

            return &m_data[handle.index];
        }

        bool set(Handle handle, const T& element) {
            if (handle == HANDLE_INVALID) {
                return false;
            }

            if (handle.index >= m_capacity) {
                return false;
            }

            if (m_versions[handle.index] != handle.version) {
                return false;
            }

            memcpy(&m_data[handle.index], &element, sizeof(T));
            return true;
        }

        bool is_valid(Handle handle) const {
            if (handle == HANDLE_INVALID) {
                return false;
            }

            if (handle.index >= m_capacity) {
                return false;
            }

            return m_versions[handle.index] == handle.version;
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

        Iterator begin() {
            HandleIndexType index = 0;
            while (index < m_capacity) {
                auto handle = Handle{ index, m_versions[index] };
                if (is_valid(handle)) {
                    break;
                }
                index++;
            }
            return { this, index };
        }

        Iterator end() {
            return { this, m_capacity };
        }

        ConstIterator begin() const {
            HandleIndexType index = 0;
            while (index < m_capacity) {
                auto handle = Handle{ index, m_versions[index] };
                if (is_valid(handle)) {
                    break;
                }
                index++;
            }
            return { this, index };
        }

        ConstIterator end() const {
            return { this, m_capacity };
        }
    };
}

#endif
