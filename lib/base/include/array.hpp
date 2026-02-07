#ifndef EDGE_ARRAY_H
#define EDGE_ARRAY_H

#include "allocator.hpp"
#include "random_access_iterator.hpp"

#include <cstdlib>
#include <cstring>
#include <type_traits>
#include <initializer_list>

namespace edge {
template <typename T> struct Array {
  EDGE_DECLARE_CONTAINER_HEADER(T)

  void destroy(const NotNull<const Allocator *> alloc) {
    destroy_elements();
    if (m_data) {
      alloc->free(m_data);
    }
    m_data = nullptr;
    m_size = 0;
    m_capacity = 0;
  }

  void clear() {
    destroy_elements();
    m_size = 0;
  }

  bool reserve(const NotNull<const Allocator *> alloc,
               const size_type new_cap) {
    if (new_cap <= m_capacity) {
      return true;
    }
    return grow_to(alloc, new_cap);
  }

  bool resize(const NotNull<const Allocator *> alloc,
              const size_type new_size) {
    if (new_size > m_capacity) {
      if (!grow_to(alloc, new_size)) {
        return false;
      }
    }

    if (new_size > m_size) {
      if constexpr (std::is_trivially_constructible_v<T> &&
                    std::is_trivially_destructible_v<T>) {
        memset(&m_data[m_size], 0, sizeof(T) * (new_size - m_size));
      } else {
        for (size_type i = m_size; i < new_size; ++i) {
          new (&m_data[i]) T();
        }
      }
    } else if (new_size < m_size) {
      if constexpr (!std::is_trivially_destructible_v<T>) {
        for (size_type i = new_size; i < m_size; ++i) {
          m_data[i].~T();
        }
      }
    }
    m_size = new_size;
    return true;
  }

  constexpr reference operator[](size_type index) {
    assert(index < m_size && "Array::operator[]: index out of bounds");
    return m_data[index];
  }

  constexpr const_reference operator[](size_type index) const {
    assert(index < m_size && "Array::operator[]: index out of bounds");
    return m_data[index];
  }

  constexpr reference front() {
    assert(m_size > 0 && "Array::front(): array is empty");
    return m_data[0];
  }

  constexpr const_reference front() const {
    assert(m_size > 0 && "Array::front(): array is empty");
    return m_data[0];
  }

  constexpr reference back() {
    assert(m_size > 0 && "Array::back(): array is empty");
    return m_data[m_size - 1];
  }

  constexpr const_reference back() const {
    assert(m_size > 0 && "Array::back(): array is empty");
    return m_data[m_size - 1];
  }

  constexpr size_type size() const noexcept { return m_size; }

  constexpr size_type capacity() const noexcept { return m_capacity; }

  constexpr bool empty() const noexcept { return m_size == 0; }

  bool push_back(const NotNull<const Allocator *> alloc, const_reference value) {
    if (m_size == m_capacity) {
      if (!grow_to(alloc, m_capacity == 0 ? 16 : m_capacity * 2)) {
        return false;
      }
    }
    new (&m_data[m_size++]) T(value);
    return true;
  }

  bool push_back(const_reference value) {
    const bool is_full = m_size >= m_capacity;
    assert(!is_full && "Array is already full.");
    if (is_full) {
      return false;
    }
    new (&m_data[m_size++]) T(value);
    return true;
  }

  bool push_back(const NotNull<const Allocator *> alloc, T &&value) {
    if (m_size == m_capacity) {
      if (!grow_to(alloc, m_capacity == 0 ? 16 : m_capacity * 2)) {
        return false;
      }
    }
    new (&m_data[m_size++]) T(std::move(value));
    return true;
  }

  bool push_back(T &&value) {
    const bool is_full = m_size >= m_capacity;
    assert(!is_full && "Array is already full.");
    if (is_full) {
      return false;
    }
    new (&m_data[m_size++]) T(std::move(value));
    return true;
  }

  template <typename... Args>
  bool emplace_back(const NotNull<const Allocator *> alloc, Args &&...args) {
    if (m_size == m_capacity) {
      if (!grow_to(alloc, m_capacity == 0 ? 16 : m_capacity * 2)) {
        return false;
      }
    }
    new (&m_data[m_size++]) T(std::forward<Args>(args)...);
    return true;
  }

  bool pop_back(pointer out_element = nullptr) {
    assert(m_size > 0 && "Array::pop_back: array is empty");
    if (m_size == 0) {
      return false;
    }

    --m_size;
    if (out_element) {
      if constexpr (std::is_move_assignable_v<T> &&
                    !std::is_trivially_copyable_v<T>) {
        *out_element = std::move(m_data[m_size]);
      } else {
        *out_element = m_data[m_size];
      }
    }

    if constexpr (!std::is_trivially_destructible_v<T>) {
      m_data[m_size].~T();
    }

    return true;
  }

  bool insert(const NotNull<const Allocator *> alloc, size_type index,
              const_reference value) {
    assert(index <= m_size && "Array::insert: index out of bounds");
    if (index > m_size) {
      return false;
    }

    if (m_size == m_capacity) {
      if (!grow_to(alloc, m_capacity == 0 ? 16 : m_capacity * 2)) {
        return false;
      }
    }

    if constexpr (std::is_trivially_copyable_v<T>) {
      if (index < m_size) {
        memmove(&m_data[index + 1], &m_data[index],
                sizeof(T) * (m_size - index));
      }
      m_data[index] = value;
    } else {
      if (index < m_size) {
        new (&m_data[m_size]) T(std::move(m_data[m_size - 1]));
        for (usize i = m_size - 1; i > index; --i) {
          m_data[i] = std::move(m_data[i - 1]);
        }
        m_data[index] = value;
      } else {
        new (&m_data[index]) T(value);
      }
    }
    ++m_size;

    return true;
  }

  bool remove(size_type index, pointer out_element = nullptr) {
    assert(index < m_size && "Array::erase: index out of bounds");
    if (index >= m_size) {
      return false;
    }

    if (out_element) {
      if constexpr (std::is_move_assignable_v<T> &&
                    !std::is_trivially_copyable_v<T>) {
        *out_element = std::move(m_data[index]);
      } else {
        *out_element = m_data[index];
      }
    }

    if constexpr (std::is_trivially_copyable_v<T>) {
      if (index < m_size - 1) {
        memmove(&m_data[index], &m_data[index + 1],
                sizeof(T) * (m_size - index - 1));
      }
    } else {
      for (size_type i = index; i < m_size - 1; ++i) {
        m_data[i] = std::move(m_data[i + 1]);
      }
      m_data[m_size - 1].~T();
    }
    --m_size;

    return true;
  }

  constexpr pointer data() noexcept { return m_data; }

  constexpr const_pointer data() const noexcept { return m_data; }

  EDGE_DECLARE_RANDOM_ACCESS_ITERATOR(T, m_data, m_size)

private:
  pointer m_data = nullptr;
  size_type m_size = 0;
  size_type m_capacity = 0;

  void destroy_elements() {
    if constexpr (!std::is_trivially_destructible_v<T>) {
      for (size_type i = m_size; i > 0; --i) {
        m_data[i - 1].~T();
      }
    }
  }

  bool grow_to(const NotNull<const Allocator *> alloc,
               const size_type new_cap) {
    auto new_data =
        static_cast<pointer>(alloc->malloc(sizeof(T) * new_cap, alignof(T)));
    assert(new_data && "Array: allocation failed during grow");
    if (!new_data) {
      return false;
    }

    move_elements(new_data, m_data, m_size);

    if (m_data) {
      alloc->free(m_data);
    }
    m_data = new_data;
    m_capacity = new_cap;
    return true;
  }

  static void move_elements(pointer dst, pointer src, const size_type count) {
    if (!src || count == 0) {
      return;
    }

    if constexpr (std::is_trivially_copyable_v<T>) {
      memcpy(dst, src, sizeof(T) * count);
    } else {
      for (size_type i = 0; i < count; ++i) {
        new (&dst[i]) T(std::move(src[i]));
        src[i].~T();
      }
    }
  }
};

template <typename T, usize N> class StaticArray {
  static_assert(N > 0, "StaticArray: N must be greater than 0");

public:
  EDGE_DECLARE_CONTAINER_HEADER(T)

  constexpr StaticArray() : m_data{} {}

  constexpr StaticArray(std::initializer_list<T> init) : m_data{} {
    static_assert(
        std::is_copy_assignable_v<T>,
        "StaticArray initializer_list constructor requires copy-assignable T");
    assert(init.size() <= N &&
           "StaticArray: initializer_list has more elements than N");
    usize i = 0;
    for (auto &v : init) {
      m_data[i++] = v;
    }
  }

  constexpr StaticArray(const StaticArray &) = default;
  constexpr StaticArray &operator=(const StaticArray &) = default;
  constexpr StaticArray(StaticArray &&) = default;
  constexpr StaticArray &operator=(StaticArray &&) = default;

  constexpr reference operator[](usize index) {
    assert(index < N && "StaticArray::operator[]: index out of bounds");
    return m_data[index];
  }

  constexpr const_reference operator[](usize index) const {
    assert(index < N && "StaticArray::operator[]: index out of bounds");
    return m_data[index];
  }

  constexpr reference at(usize index) {
    assert(index < N && "StaticArray::at: index out of bounds");
    return m_data[index];
  }

  constexpr const_reference at(usize index) const {
    assert(index < N && "StaticArray::at: index out of bounds");
    return m_data[index];
  }

  constexpr reference front() { return m_data[0]; }
  constexpr const_reference front() const { return m_data[0]; }

  constexpr reference back() { return m_data[N - 1]; }
  constexpr const_reference back()  const { return m_data[N - 1]; }

  constexpr usize size() const noexcept { return N; }
  constexpr bool empty() const noexcept { return N == 0; }
  constexpr usize capacity() const noexcept { return N; }

  constexpr pointer data() noexcept { return m_data; }
  constexpr const_pointer data() const noexcept { return m_data; }

  EDGE_DECLARE_RANDOM_ACCESS_ITERATOR(T, m_data, N)

  constexpr void fill(const T& val) {
    for (usize i = 0; i < N; ++i) {
      m_data[i] = val;
    }
  }

  constexpr void swap(StaticArray& other) noexcept(noexcept(std::swap(m_data[0], other.m_data[0]))) {
    for (usize i = 0; i < N; ++i) {
      T tmp = std::move(m_data[i]);
      m_data[i] = std::move(other.m_data[i]);
      other.m_data[i] = std::move(tmp);
    }
  }

  constexpr bool operator==(const StaticArray& other) const {
    for (usize i = 0; i < N; ++i) {
      if (m_data[i] != other.m_data[i]) return false;
    }
    return true;
  }

  constexpr bool operator!=(const StaticArray& other) const { return !(*this == other); }
private:
  T m_data[N];
};
} // namespace edge

#endif