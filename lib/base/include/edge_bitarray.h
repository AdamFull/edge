#ifndef EDGE_BITARRAY_H
#define EDGE_BITARRAY_H

#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#define EDGE_BITARRAY_SIZE(n) (((n) + 7) / 8)

#ifdef __cplusplus
extern "C" {
#endif

	static inline void edge_bitarray_set(uint8_t* data, size_t index) {
		data[index / 8] |= (1 << (index % 8));
	}

	static inline void edge_bitarray_clear(uint8_t* data, size_t index) {
		data[index / 8] &= ~(1 << (index % 8));
	}

	static inline void edge_bitarray_toggle(uint8_t* data, size_t index) {
		data[index / 8] ^= (1 << (index % 8));
	}

	static inline bool edge_bitarray_get(const uint8_t* data, size_t index) {
		return (data[index / 8] & (1 << (index % 8))) != 0;
	}

	static inline void edge_bitarray_put(uint8_t* data, size_t index, bool value) {
		if (value) {
			edge_bitarray_set(data, index);
		}
		else {
			edge_bitarray_clear(data, index);
		}
	}

	static inline void edge_bitarray_clear_all(uint8_t* data, size_t num_bytes) {
		memset(data, 0, num_bytes);
	}

	static inline void edge_bitarray_set_all(uint8_t* data, size_t num_bytes) {
		memset(data, 0xFF, num_bytes);
	}

	static inline int edge_bitarray_count_set(const uint8_t* data, size_t num_bytes) {
		int count = 0;

		for (int i = 0; i < num_bytes; i++) {
			uint8_t byte = data[i];
			// Brian Kernighan's algorithm
			while (byte) {
				byte &= byte - 1;
				count++;
			}
		}

		return count;
	}

	static inline int edge_bitarray_find_first_set(const uint8_t* data, size_t num_bits) {
		int num_bytes = EDGE_BITARRAY_SIZE(num_bits);

		for (int i = 0; i < num_bytes; i++) {
			if (data[i] != 0) {
				// Found a byte with set bits
				for (int bit = 0; bit < 8; bit++) {
					int index = i * 8 + bit;
					if (index >= num_bits) return -1;
					if (data[i] & (1 << bit)) {
						return index;
					}
				}
			}
		}

		return -1;
	}

	static inline bool edge_bitarray_any_set(const uint8_t* data, size_t num_bytes) {
		for (int i = 0; i < num_bytes; i++) {
			if (data[i] != 0) return true;
		}
		return false;
	}

	static inline bool edge_bitarray_all_clear(const uint8_t* data, size_t num_bytes) {
		return !edge_bitarray_any_set(data, num_bytes);
	}

#ifdef __cplusplus
}
#endif

#endif // EDGE_BITARRAY_H
