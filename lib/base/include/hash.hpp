#ifndef EDGE_HASH_H
#define EDGE_HASH_H

#include "stddef.hpp"

#include <functional>

namespace edge {
	template<typename T>
	struct Hash {
		usize operator()(const T& obj) const noexcept {
			(void)obj;
			static_assert(sizeof(T) == 0, "Hash specialization not found for this type. Please provide a Hash<T> specialization.");
			return 0;
		}
	};

	template<std::integral T>
	struct Hash<T> {
		usize operator()(const T& value) const noexcept {
			if constexpr (sizeof(T) == 1) {
				return static_cast<usize>(value) * 0x9e3779b9u;
			}
			else if constexpr (sizeof(T) == 2) {
				u32 v = static_cast<u32>(value);
				v *= 0x9e3779b9u;
				return v ^ (v >> 16);
			}
			else if constexpr (sizeof(T) <= 4) {
				u32 v = static_cast<u32>(value);
				v = (v ^ 61) ^ (v >> 16);
				v = v + (v << 3);
				v = v ^ (v >> 4);
				v = v * 0x27d4eb2d;
				v = v ^ (v >> 15);
				return v;
			}
			else {
				u64 v = static_cast<u64>(value);
				v = (~v) + (v << 21);
				v = v ^ (v >> 24);
				v = (v + (v << 3)) + (v << 8);
				v = v ^ (v >> 14);
				v = (v + (v << 2)) + (v << 4);
				v = v ^ (v >> 28);
				v = v + (v << 31);
				return v;
			}
		}
	};

	struct Hash128 {
		u64 low;
		u64 high;
	};

	u32 hash_crc32(const void* data, usize size);

	u32 hash_xxh32(const void* data, usize size, u32 seed = 0);
	u64 hash_xxh64(const void* data, usize size, u64 seed = 0);

	u32 hash_murmur3_32(const void* data, usize size, u32 seed = 0);
	Hash128 hash_murmur3_128(const void* data, usize size, u32 seed = 0);

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