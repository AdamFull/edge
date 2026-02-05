#ifndef EDGE_RANDOM_ACCESS_ITERATOR_H
#define EDGE_RANDOM_ACCESS_ITERATOR_H

#include "stddef.hpp"
#include <iterator>

namespace edge {
template <typename T> class random_access_iterator {
public:
  using iterator_category = std::random_access_iterator_tag;
  using value_type = T;
  using difference_type = isize;
  using pointer = T *;
  using reference = T &;

  constexpr random_access_iterator() noexcept : m_ptr(nullptr) {}
  constexpr explicit random_access_iterator(T *p) noexcept : m_ptr(p) {}

  constexpr reference operator*() const { return *m_ptr; }
  constexpr pointer operator->() const { return m_ptr; }
  constexpr random_access_iterator &operator++() {
    ++m_ptr;
    return *this;
  }
  constexpr random_access_iterator operator++(int) {
    auto tmp = *this;
    ++m_ptr;
    return tmp;
  }
  constexpr random_access_iterator &operator--() {
    --m_ptr;
    return *this;
  }
  constexpr random_access_iterator operator--(int) {
    auto tmp = *this;
    --m_ptr;
    return tmp;
  }
  constexpr random_access_iterator operator+(difference_type n) const {
    return random_access_iterator(m_ptr + n);
  }
  constexpr random_access_iterator operator-(difference_type n) const {
    return random_access_iterator(m_ptr - n);
  }
  constexpr difference_type operator-(const random_access_iterator &o) const {
    return m_ptr - o.m_ptr;
  }
  constexpr reference operator[](difference_type n) const { return m_ptr[n]; }

  constexpr bool operator==(const random_access_iterator &o) const {
    return m_ptr == o.m_ptr;
  }
  constexpr bool operator!=(const random_access_iterator &o) const {
    return m_ptr != o.m_ptr;
  }
  constexpr bool operator<(const random_access_iterator &o) const {
    return m_ptr < o.m_ptr;
  }
  constexpr bool operator<=(const random_access_iterator &o) const {
    return m_ptr <= o.m_ptr;
  }
  constexpr bool operator>(const random_access_iterator &o) const {
    return m_ptr > o.m_ptr;
  }
  constexpr bool operator>=(const random_access_iterator &o) const {
    return m_ptr >= o.m_ptr;
  }

private:
  T *m_ptr;
};

template <typename T> class const_random_access_iterator {
public:
  using iterator_category = std::random_access_iterator_tag;
  using value_type = T;
  using difference_type = isize;
  using pointer = const T *;
  using reference = const T &;

  constexpr const_random_access_iterator() noexcept : m_ptr(nullptr) {}
  constexpr explicit const_random_access_iterator(const T *p) noexcept
      : m_ptr(p) {}

  constexpr reference operator*() const { return *m_ptr; }
  constexpr pointer operator->() const { return m_ptr; }
  constexpr const_random_access_iterator &operator++() {
    ++m_ptr;
    return *this;
  }
  constexpr const_random_access_iterator operator++(int) {
    auto tmp = *this;
    ++m_ptr;
    return tmp;
  }
  constexpr const_random_access_iterator &operator--() {
    --m_ptr;
    return *this;
  }
  constexpr const_random_access_iterator operator--(int) {
    auto tmp = *this;
    --m_ptr;
    return tmp;
  }
  constexpr const_random_access_iterator operator+(difference_type n) const {
    return const_random_access_iterator(m_ptr + n);
  }
  constexpr const_random_access_iterator operator-(difference_type n) const {
    return const_random_access_iterator(m_ptr - n);
  }
  constexpr difference_type
  operator-(const const_random_access_iterator &o) const {
    return m_ptr - o.m_ptr;
  }
  constexpr reference operator[](difference_type n) const { return m_ptr[n]; }

  constexpr bool operator==(const const_random_access_iterator &o) const {
    return m_ptr == o.m_ptr;
  }
  constexpr bool operator!=(const const_random_access_iterator &o) const {
    return m_ptr != o.m_ptr;
  }
  constexpr bool operator<(const const_random_access_iterator &o) const {
    return m_ptr < o.m_ptr;
  }
  constexpr bool operator<=(const const_random_access_iterator &o) const {
    return m_ptr <= o.m_ptr;
  }
  constexpr bool operator>(const const_random_access_iterator &o) const {
    return m_ptr > o.m_ptr;
  }
  constexpr bool operator>=(const const_random_access_iterator &o) const {
    return m_ptr >= o.m_ptr;
  }

private:
  const T *m_ptr;
};

template <typename T>
using reverse_random_access_iterator =
    std::reverse_iterator<random_access_iterator<T>>;
template <typename T>
using const_reverse_random_access_iterator =
    std::reverse_iterator<const_random_access_iterator<T>>;
} // namespace edge

#define EDGE_DECLARE_RANDOM_ACCESS_ITERATOR(Type, Data, Size)                  \
  constexpr random_access_iterator<Type> begin() noexcept {                    \
    return random_access_iterator<Type>(Data);                                 \
  }                                                                            \
  constexpr random_access_iterator<Type> end() noexcept {                      \
    return random_access_iterator<Type>(Data + Size);                          \
  }                                                                            \
  constexpr const_random_access_iterator<Type> begin() const noexcept {        \
    return const_random_access_iterator<Type>(Data);                           \
  }                                                                            \
  constexpr const_random_access_iterator<Type> end() const noexcept {          \
    return const_random_access_iterator<Type>(Data + Size);                    \
  }                                                                            \
  constexpr const_random_access_iterator<Type> cbegin() const noexcept {       \
    return const_random_access_iterator<Type>(Data);                           \
  }                                                                            \
  constexpr const_random_access_iterator<Type> cend() const noexcept {         \
    return const_random_access_iterator<Type>(Data + Size);                    \
  }                                                                            \
  constexpr reverse_random_access_iterator<Type> rbegin() noexcept {           \
    return reverse_random_access_iterator<Type>(end());                        \
  }                                                                            \
  constexpr reverse_random_access_iterator<Type> rend() noexcept {             \
    return reverse_random_access_iterator<Type>(begin());                      \
  }                                                                            \
  constexpr const_reverse_random_access_iterator<Type> rbegin()                \
      const noexcept {                                                         \
    return const_reverse_random_access_iterator<Type>(end());                  \
  }                                                                            \
  constexpr const_reverse_random_access_iterator<Type> rend() const noexcept { \
    return const_reverse_random_access_iterator<Type>(begin());                \
  }

#endif