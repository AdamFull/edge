#ifndef EDGE_BITARRAY_H
#define EDGE_BITARRAY_H

#include "stddef.hpp"

namespace edge {
	template<usize BitCount>
	struct BitArray {
		u8 m_data[(BitCount + 7) / 8];
	};

	template<usize BitCount>
	void bitarray_set(BitArray<BitCount>* arr, usize index) {
		arr->m_data[index / 8] |= (1 << (index % 8));
	}

	template<usize BitCount>
	void bitarray_clear(BitArray<BitCount>* arr, usize index) {
		arr->m_data[index / 8] &= ~(1 << (index % 8));
	}

	template<usize BitCount>
	void bitarray_toggle(BitArray<BitCount>* arr, usize index) {
		arr->m_data[index / 8] ^= (1 << (index % 8));
	}

	template<usize BitCount>
	bool bitarray_get(const BitArray<BitCount>* arr, usize index) {
		return (arr->m_data[index / 8] & (1 << (index % 8))) != 0;
	}

	template<usize BitCount>
	void bitarray_put(BitArray<BitCount>* arr, usize index, bool value) {
		if (value) {
			bitarray_set(arr, index);
		}
		else {
			bitarray_clear(arr, index);
		}
	}

	template<usize BitCount>
	void bitarray_clear_all(BitArray<BitCount>* arr) {
		memset(arr->m_data, 0, sizeof(arr->m_data));
	}

	template<usize BitCount>
	void bitarray_set_all(BitArray<BitCount>* arr) {
		memset(arr->m_data, 0xFF, sizeof(arr->m_data));
	}

	template<usize BitCount>
	i32 bitarray_count_set(const BitArray<BitCount>* arr) {
		i32 count = 0;

		for (i32 i = 0; i < sizeof(arr->m_data); i++) {
			u8 byte = arr->m_data[i];
			while (byte) {
				byte &= byte - 1;
				count++;
			}
		}

		return count;
	}

	template<usize BitCount>
	i32 bitarray_find_first_set(const BitArray<BitCount>* arr) {
		for (i32 i = 0; i < sizeof(arr->m_data); i++) {
			if (arr->m_data[i] != 0) {
				for (i32 bit = 0; bit < 8; bit++) {
					i32 index = i * 8 + bit;
					if (index >= BitCount) {
						return -1;
					}
					if (arr->m_data[i] & (1 << bit)) {
						return index;
					}
				}
			}
		}

		return -1;
	}

	template<usize BitCount>
	bool bitarray_any_set(const BitArray<BitCount>* arr) {
		for (i32 i = 0; i < sizeof(arr->m_data); i++) {
			if (arr->m_data[i] != 0) {
				return true;
			}
		}
		return false;
	}

	template<usize BitCount>
	bool bitarray_all_clear(const BitArray<BitCount>* arr) {
		return !bitarray_any_set(arr);
	}
}

#endif