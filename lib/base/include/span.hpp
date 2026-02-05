#ifndef EDGE_SPAN_H
#define EDGE_SPAN_H

#include "random_access_iterator.hpp"

#include <cstring>
#include <type_traits>

namespace edge {
template <typename T> struct Array;

template <TrivialType T, typename StorageProvider> struct Buffer;

template <TrivialType T> struct Span {
  EDGE_DECLARE_CONTAINER_HEADER(T)

  constexpr Span() = default;

  constexpr Span(const_pointer data, const size_type size) : m_data(data), m_size(size) {}

  constexpr Span(pointer begin, pointer end)
      : m_data(begin), m_size(static_cast<size_type>(end - begin)) {
    assert(end >= begin);
  }

  template <size_type N>
  constexpr Span(T (&arr)[N]) : m_data(arr), m_size(N) {}

  constexpr Span(Array<T> &arr) noexcept
      : m_data(arr.data()), m_size(arr.size()) {}

  constexpr Span(const Array<T> &arr) noexcept
    requires std::is_const_v<T>
      : m_data(arr.data()), m_size(arr.size()) {}

  template <typename StorageProvider>
  constexpr Span(Buffer<T, StorageProvider> &arr) noexcept
      : m_data(arr.data()), m_size(arr.size()) {}

  template <typename StorageProvider>
  constexpr Span(const Buffer<T, StorageProvider> &arr) noexcept
    requires std::is_const_v<T>
      : m_data(arr.data()), m_size(arr.size()) {}

  [[nodiscard]] constexpr size_type size() const { return m_size; }

  [[nodiscard]] constexpr size_type size_bytes() const { return m_size * sizeof(T); }

  [[nodiscard]] constexpr bool empty() const { return m_size == 0; }

  constexpr reference operator[](size_type index) {
    assert(index < m_size && "Span::operator[]: index out of bounds");
    return m_data[index];
  }

  constexpr const_reference operator[](size_type index) const {
    assert(index < m_size && "Span::operator[]: index out of bounds");
    return m_data[index];
  }

  constexpr reference front() {
    assert(m_size > 0 && "Span::front(): span is empty");
    return m_data[0];
  }

  constexpr const_reference front() const {
    assert(m_size > 0 && "Span::front(): span is empty");
    return m_data[0];
  }

  constexpr reference back() {
    assert(m_size > 0 && "Span::back(): span is empty");
    return m_data[m_size - 1];
  }

  constexpr const_reference back() const {
    assert(m_size > 0 && "Span::back(): span is empty");
    return m_data[m_size - 1];
  }

  constexpr pointer data() noexcept { return m_data; }

  constexpr const_pointer data() const noexcept { return m_data; }

  EDGE_DECLARE_RANDOM_ACCESS_ITERATOR(T, m_data, m_size)

  Span subspan(size_type offset, const size_type count) const {
    if (offset >= m_size) {
      return Span();
    }

    size_type actual_count = count;
    if (offset + count > m_size) {
      actual_count = m_size - offset;
    }

    return Span(m_data + offset, actual_count);
  }

  Span subspan(const size_type offset) const { return subspan(offset, m_size); }

  Span first(size_type count) const {
    if (count > m_size) {
      count = m_size;
    }
    return Span(m_data, count);
  }

  Span last(size_type count) const {
    if (count > m_size) {
      count = m_size;
    }
    return Span(m_data + (m_size - count), count);
  }

  void copy_to(pointer dest) const {
    if (m_data && dest && m_size > 0) {
      memcpy(dest, m_data, sizeof(T) * m_size);
    }
  }

  void copy_from(const_pointer src, const size_type count) {
    if (m_data && src && count > 0) {
      const size_type actual_count = count > m_size ? m_size : count;
      memcpy(m_data, src, sizeof(T) * actual_count);
    }
  }

private:
  pointer m_data = nullptr;
  size_type m_size = 0;
};
} // namespace edge
#endif