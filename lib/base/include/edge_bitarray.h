#ifndef EDGE_BITARRAY_H
#define EDGE_BITARRAY_H

#include "edge_base.h"
#include <string.h>

#define EDGE_BITARRAY_SIZE(n) (((n) + 7) / 8)

#ifdef __cplusplus
extern "C" {
#endif

	static inline void edge_bitarray_set(u8* data, size_t index) {
		data[index / 8] |= (1 << (index % 8));
	}

	static inline void edge_bitarray_clear(u8* data, size_t index) {
		data[index / 8] &= ~(1 << (index % 8));
	}

	static inline void edge_bitarray_toggle(u8* data, size_t index) {
		data[index / 8] ^= (1 << (index % 8));
	}

	static inline bool edge_bitarray_get(const u8* data, size_t index) {
		return (data[index / 8] & (1 << (index % 8))) != 0;
	}

	static inline void edge_bitarray_put(u8* data, size_t index, bool value) {
		if (value) {
			edge_bitarray_set(data, index);
		}
		else {
			edge_bitarray_clear(data, index);
		}
	}

	static inline void edge_bitarray_clear_all(u8* data, size_t num_bytes) {
		memset(data, 0, num_bytes);
	}

	static inline void edge_bitarray_set_all(u8* data, size_t num_bytes) {
		memset(data, 0xFF, num_bytes);
	}

	static inline i32 edge_bitarray_count_set(const u8* data, size_t num_bytes) {
		i32 count = 0;

		for (i32 i = 0; i < num_bytes; i++) {
			u8 byte = data[i];
			// Brian Kernighan's algorithm
			while (byte) {
				byte &= byte - 1;
				count++;
			}
		}

		return count;
	}

	static inline i32 edge_bitarray_find_first_set(const u8* data, size_t num_bits) {
		i32 num_bytes = EDGE_BITARRAY_SIZE(num_bits);

		for (i32 i = 0; i < num_bytes; i++) {
			if (data[i] != 0) {
				// Found a byte with set bits
				for (i32 bit = 0; bit < 8; bit++) {
					i32 index = i * 8 + bit;
					if (index >= num_bits) return -1;
					if (data[i] & (1 << bit)) {
						return index;
					}
				}
			}
		}

		return -1;
	}

	static inline bool edge_bitarray_any_set(const u8* data, size_t num_bytes) {
		for (i32 i = 0; i < num_bytes; i++) {
			if (data[i] != 0) return true;
		}
		return false;
	}

	static inline bool edge_bitarray_all_clear(const u8* data, size_t num_bytes) {
		return !edge_bitarray_any_set(data, num_bytes);
	}

#ifdef __cplusplus
}
#endif

#endif // EDGE_BITARRAY_H
