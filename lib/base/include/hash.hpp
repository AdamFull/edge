#ifndef EDGE_HASH_H
#define EDGE_HASH_H

#include "stddef.hpp"

namespace edge {
	struct Hash128 {
		u64 low;
		u64 high;
	};

	u32 hash_crc32(const void* data, usize size);

	u32 hash_xxh32(const void* data, usize size, u32 seed = 0);
	u64 hash_xxh64(const void* data, usize size, u64 seed = 0);

	u32 hash_murmur3_32(const void* data, usize size, u32 seed = 0);
	Hash128 hash_murmur3_128(const void* data, usize size, u32 seed = 0);

	constexpr inline u32 hash_int32(u32 value) {
		value = (value ^ 61) ^ (value >> 16);
		value = value + (value << 3);
		value = value ^ (value >> 4);
		value = value * 0x27d4eb2d;
		value = value ^ (value >> 15);
		return value;
	}

	constexpr inline u64 hash_int64(u64 value) {
		value = (~value) + (value << 21);
		value = value ^ (value >> 24);
		value = (value + (value << 3)) + (value << 8);
		value = value ^ (value >> 14);
		value = (value + (value << 2)) + (value << 4);
		value = value ^ (value >> 28);
		value = value + (value << 31);
		return value;
	}

	u32 hash_string_32(const char* str);
	u64 hash_string_64(const char* str);
	usize hash_pointer(const void* ptr);

	constexpr inline usize hash_combine(usize hash1, usize hash2) {
		if constexpr (sizeof(usize) == 8) {
			hash1 ^= hash2 + 0x9e3779b97f4a7c15ULL + (hash1 << 6) + (hash1 >> 2);
		}
		else {
			hash1 ^= hash2 + 0x9e3779b9 + (hash1 << 6) + (hash1 >> 2);
		}
		return hash1;
	}

	constexpr inline usize hash128_to_size(Hash128 hash) {
		if constexpr (sizeof(usize) == 8) {
			return hash.low ^ hash.high;
		}
		else {
			return static_cast<usize>((hash.low ^ hash.high) & 0xFFFFFFFF);
		}
	}
}

#endif