#ifndef EDGE_BITARRAY_H
#define EDGE_BITARRAY_H

#include "stddef.hpp"

namespace edge {
	template<usize BitCount>
	struct BitArray {
		u8 m_data[(BitCount + 7) / 8];

		constexpr void set(usize index) noexcept {
			m_data[index / 8] |= (1 << (index % 8));
		}

		constexpr void clear(usize index) noexcept {
			m_data[index / 8] &= ~(1 << (index % 8));
		}

		constexpr void toggle(usize index) noexcept {
			m_data[index / 8] ^= (1 << (index % 8));
		}

		constexpr bool get(usize index) const noexcept {
			return (m_data[index / 8] & (1 << (index % 8))) != 0;
		}

		constexpr void put(usize index, bool value) noexcept {
			value ? set(index) : clear(index);
			if (value) {
				set(index);
			}
			else {
				clear(index);
			}
		}

		constexpr void clear_all() noexcept {
			memset(m_data, 0, sizeof(m_data));
		}

		constexpr void set_all() noexcept {
			memset(m_data, 0xFF, sizeof(m_data));
		}

		constexpr i32 count_set() const noexcept {
			i32 count = 0;

			for (i32 i = 0; i < sizeof(m_data); i++) {
				u8 byte = m_data[i];
				while (byte) {
					byte &= byte - 1;
					count++;
				}
			}

			return count;
		}

		constexpr i32 find_first_set() const noexcept {
			for (i32 i = 0; i < sizeof(m_data); i++) {
				if (m_data[i] != 0) {
					for (i32 bit = 0; bit < 8; bit++) {
						i32 index = i * 8 + bit;
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

		constexpr bool any_set() const noexcept {
			for (i32 i = 0; i < sizeof(m_data); i++) {
				if (m_data[i] != 0) {
					return true;
				}
			}
			return false;
		}

		constexpr bool all_clear() const noexcept {
			return !any_set();
		}
	};
}

#endif