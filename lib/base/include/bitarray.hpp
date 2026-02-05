#ifndef EDGE_BITARRAY_H
#define EDGE_BITARRAY_H

#include "stddef.hpp"

namespace edge {
template <usize BitCount> struct BitArray {
  static constexpr usize bit_count() { return BitCount; }

  static constexpr usize byte_count() { return BitCount / 8; }

  constexpr void set(const usize index) { m_data[index / 8] |= (1 << (index % 8)); }

  constexpr void clear(const usize index) {
    m_data[index / 8] &= ~(1 << (index % 8));
  }

  constexpr void toggle(const usize index) {
    m_data[index / 8] ^= (1 << (index % 8));
  }

  constexpr bool get(const usize index) const {
    return (m_data[index / 8] & (1 << (index % 8))) != 0;
  }

  constexpr void put(const usize index, const bool value) {
    value ? set(index) : clear(index);
    if (value) {
      set(index);
    } else {
      clear(index);
    }
  }

  constexpr void clear_all() { memset(m_data, 0, sizeof(m_data)); }

  constexpr void set_all() { memset(m_data, 0xFF, sizeof(m_data)); }

  constexpr usize count_set() const {
    usize count = 0;

    for (usize i = 0; i < sizeof(m_data); i++) {
      u8 byte = m_data[i];
      while (byte) {
        byte &= byte - 1;
        count++;
      }
    }

    return count;
  }

  constexpr usize find_first_set() const {
    for (usize i = 0; i < sizeof(m_data); i++) {
      if (m_data[i] != 0) {
        for (usize bit = 0; bit < 8; bit++) {
          const usize index = i * 8 + bit;
          if (index >= BitCount) {
            return -1;
          }
          if (m_data[i] & (1 << bit)) {
            return index;
          }
        }
      }
    }

    return -1;
  }

  constexpr bool any_set() const {
    for (usize i = 0; i < sizeof(m_data); i++) {
      if (m_data[i] != 0) {
        return true;
      }
    }
    return false;
  }

  constexpr bool all_clear() const { return !any_set(); }

private:
  u8 m_data[(BitCount + 7) / 8];
};
} // namespace edge

#endif