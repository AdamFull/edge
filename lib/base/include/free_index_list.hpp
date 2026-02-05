#ifndef EDGE_FREE_INDEX_LIST_H
#define EDGE_FREE_INDEX_LIST_H

#include "allocator.hpp"

namespace edge {
struct FreeIndexList {
  u32 *m_indices = nullptr;
  u32 m_capacity = 0ull;
  u32 m_count = 0ull;

  bool create(const NotNull<const Allocator *> alloc, const u32 capacity) {
    if (capacity == 0) {
      return false;
    }

    m_indices = alloc->allocate_array<u32>(capacity);
    if (!m_indices) {
      return false;
    }

    m_capacity = capacity;
    m_count = capacity;

    // Initialize with all indices available in reverse order
    // So index 0 is allocated first
    for (u32 i = 0; i < capacity; i++) {
      m_indices[i] = capacity - 1 - i;
    }

    return true;
  }

  void destroy(const NotNull<const Allocator *> alloc) const {
    if (m_indices) {
      alloc->deallocate_array(m_indices, m_capacity);
    }
  }

  bool allocate(u32 *out_index) {
    if (!out_index || m_count == 0) {
      return false;
    }

    *out_index = m_indices[--m_count];
    return true;
  }

  bool free(const u32 index) {
    if (m_count >= m_capacity) {
      return false;
    }

    if (index >= m_capacity) {
      return false;
    }

    m_indices[m_count++] = index;
    return true;
  }

  bool has_available() const { return m_count > 0; }

  bool full() const { return m_count == m_capacity; }

  bool empty() const { return m_count == 0; }

  void reset() {
    m_count = m_capacity;

    for (u32 i = 0; i < m_capacity; i++) {
      m_indices[i] = m_capacity - 1 - i;
    }
  }

  void clear() { m_count = 0; }
};
} // namespace edge

#endif