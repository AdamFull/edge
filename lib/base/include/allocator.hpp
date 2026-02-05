#ifndef EDGE_ALLOCATOR_H
#define EDGE_ALLOCATOR_H

#include "stddef.hpp"

#include <atomic>
#include <new>

namespace edge {
namespace detail {
struct AllocatorStats {
  std::atomic_size_t alloc_bytes;
  std::atomic_size_t free_bytes;
};

struct AllocationHeader {
  usize size;
};

inline void *aligned_malloc(usize size, usize alignment) {
  if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
    return nullptr;
  }

  if (alignment < sizeof(void *)) {
    alignment = sizeof(void *);
  }

  void *ptr = nullptr;
#if defined(EDGE_HAS_WINDOWS_API)
  ptr = _aligned_malloc(size, alignment);
#elif defined(EDGE_PLATFORM_POSIX)
  if (posix_memalign(&ptr, alignment, size) != 0) {
    ptr = nullptr;
  }
#else
  usize total_size = size + alignment + sizeof(void *);

  void *raw_ptr = malloc(total_size);
  if (!raw_ptr) {
    return nullptr;
  }

  uintptr_t raw_addr = reinterpret_cast<uintptr_t>(raw_ptr);
  uintptr_t aligned_addr =
      (raw_addr + sizeof(void *) + alignment - 1) & ~(alignment - 1);

  ptr = reinterpret_cast<void *>(aligned_addr);
  void **stored_ptr = reinterpret_cast<void **>(aligned_addr) - 1;
  *stored_ptr = raw_ptr;
#endif

  return ptr;
}

inline void aligned_free(void *ptr) {
  if (!ptr) {
    return;
  }

#if defined(EDGE_HAS_WINDOWS_API)
  _aligned_free(ptr);
#elif defined(EDGE_PLATFORM_POSIX)
  free(ptr);
#else
  void **stored_ptr = reinterpret_cast<void **>(ptr) - 1;
  void *raw_ptr = *stored_ptr;
  free(raw_ptr);
#endif
}

inline void *aligned_realloc(void *ptr, const usize new_size,
                             const usize alignment) {
  if (!ptr) {
    return aligned_malloc(new_size, alignment);
  }

  if (new_size == 0) {
    aligned_free(ptr);
    return nullptr;
  }

#if defined(EDGE_HAS_WINDOWS_API)
  return _aligned_realloc(ptr, new_size, alignment);
#else
  void *new_ptr = aligned_malloc(new_size, alignment);
  if (!new_ptr) {
    return nullptr;
  }

  memcpy(new_ptr, ptr, new_size);
  aligned_free(ptr);
  return new_ptr;
#endif
}

inline void *tracked_malloc(const usize size, const usize alignment, void *user_data) {
  if (size == 0) {
    return nullptr;
  }

  constexpr usize header_size = sizeof(AllocationHeader);
  constexpr usize back_ptr_size = sizeof(void *);

  constexpr usize min_offset = header_size + back_ptr_size;
  const usize user_data_offset = (min_offset + alignment - 1) & ~(alignment - 1);

  const usize total_size = user_data_offset + size;

  const usize alloc_alignment = alignment > alignof(AllocationHeader)
                              ? alignment
                              : alignof(AllocationHeader);
  void *raw_ptr = aligned_malloc(total_size, alloc_alignment);
  if (!raw_ptr) {
    return nullptr;
  }

  const auto header = static_cast<AllocationHeader *>(raw_ptr);
  header->size = size;

  const auto back_ptr =
      reinterpret_cast<void **>(static_cast<char *>(raw_ptr) + user_data_offset -
                                            sizeof(void *));
  *back_ptr = raw_ptr;

  const auto stats = static_cast<AllocatorStats *>(user_data);
  stats->alloc_bytes.fetch_add(size);

  return static_cast<char *>(raw_ptr) + user_data_offset;
}

inline void tracked_free(void *ptr, void *user_data) {
  if (!ptr) {
    return;
  }

  void **back_ptr = static_cast<void **>(ptr) - 1;
  const auto header = static_cast<AllocationHeader *>(*back_ptr);

  const auto stats = static_cast<AllocatorStats *>(user_data);
  stats->free_bytes.fetch_add(header->size);

  aligned_free(header);
}

inline void *tracked_realloc(void *ptr, const usize size, const usize alignment,
                             void *user_data) {
  if (!ptr) {
    return tracked_malloc(size, alignment, user_data);
  }

  if (size == 0) {
    tracked_free(ptr, user_data);
    return nullptr;
  }

  void **back_ptr = static_cast<void **>(ptr) - 1;
  const auto old_header = static_cast<AllocationHeader *>(*back_ptr);
  const usize old_size = old_header->size;

  void *new_ptr = tracked_malloc(size, alignment, user_data);
  if (!new_ptr) {
    return nullptr;
  }

  const usize copy_size = old_size < size ? old_size : size;
  memcpy(new_ptr, ptr, copy_size);

  tracked_free(ptr, user_data);

  return new_ptr;
}
} // namespace detail

using malloc_fn = void *(*)(usize size, usize alignment, void *user_data);
using free_fn = void (*)(void *ptr, void *user_data);
using realloc_fn = void *(*)(void *ptr, usize size, usize alignment,
                             void *user_data);

struct Allocator {
  malloc_fn m_malloc;
  free_fn m_free;
  realloc_fn m_realloc;
  void *user_data;

  static Allocator create(const malloc_fn malloc_pfn, const free_fn free_pfn,
                          const realloc_fn realloc_pfn, void *user_data) {
    return {malloc_pfn, free_pfn, realloc_pfn, user_data};
  }

  static Allocator create_default() {
    return create(
        [](const usize size, const usize alignment, void *) {
          return detail::aligned_malloc(size, alignment);
        },
        [](void *ptr, void *) { detail::aligned_free(ptr); },
        [](void *ptr, const usize size, const usize alignment, void *) {
          return detail::aligned_realloc(ptr, size, alignment);
        },
        nullptr);
  }

  static Allocator create_tracking() {
    static detail::AllocatorStats stats = {};
    return create(detail::tracked_malloc, detail::tracked_free,
                  detail::tracked_realloc, &stats);
  }

  usize get_net() const {
    const auto stats = static_cast<detail::AllocatorStats *>(user_data);
    if (!stats) {
      return ~0ull;
    }

    return stats->alloc_bytes.load() - stats->free_bytes.load();
  }

  void *malloc(const usize size,
               const usize alignment = alignof(max_align_t)) const {
    if (!m_malloc) {
      return nullptr;
    }
    return m_malloc(size, alignment, user_data);
  }

  void free(void *ptr) const {
    if (!m_free) {
      return;
    }
    return m_free(ptr, user_data);
  }

  void *realloc(void *ptr, const usize size,
                const usize alignment = alignof(max_align_t)) const {
    if (!m_realloc) {
      return nullptr;
    }
    return m_realloc(ptr, size, alignment, user_data);
  }

  char *strdup(const char *str) const {
    if (!str) {
      return nullptr;
    }

    const usize len = strlen(str);
    const auto copy = static_cast<char *>(malloc(len + 1));
    if (copy) {
      memcpy(copy, str, len + 1);
    }
    return copy;
  }

  char *strndup(const char *str, const usize n) const {
    if (!str) {
      return nullptr;
    }

    usize len = strlen(str);
    if (n < len)
      len = n;

    const auto copy = static_cast<char *>(malloc(len + 1));
    if (copy) {
      memcpy(copy, str, len);
      copy[len] = '\0';
    }
    return copy;
  }

  template <typename T, typename... Args> T *allocate(Args &&...args) const {
    T *ptr = static_cast<T *>(malloc(sizeof(T), alignof(T)));
    if (!ptr) {
      return nullptr;
    }

    // if constexpr (!std::is_constructible_v<T, Args...>) {
    new (ptr) T(std::forward<Args>(args)...);
    //}

    return ptr;
  }

  template <typename T>
  T *allocate_array(const usize count,
                    const usize alignment = alignof(T)) const {
    if (count == 0) {
      return nullptr;
    }

    T *ptr = static_cast<T *>(malloc(sizeof(T) * count, alignment));
    if (!ptr) {
      return nullptr;
    }

    // if constexpr (!std::is_constructable_v<T>) {
    for (usize i = 0; i < count; ++i) {
      new (&ptr[i]) T();
    }
    //}

    return ptr;
  }

  template <typename T> void deallocate(T *ptr) const {
    if (!ptr) {
      return;
    }

    if constexpr (!std::is_same_v<T, void>) {
      ptr->~T();
    }

    free(ptr);
  }

  template <typename T> void deallocate_array(T *ptr, const usize count) const {
    if (!ptr) {
      return;
    }

    if constexpr (!std::is_same_v<T, void>) {
      for (usize i = count; i > 0; --i) {
        ptr[i - 1].~T();
      }
    }

    free(ptr);
  }
};
} // namespace edge

#endif