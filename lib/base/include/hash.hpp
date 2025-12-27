#ifndef EDGE_HASH_H
#define EDGE_HASH_H

#include "stddef.hpp"

#include <functional>

namespace edge {
	inline u32 hash_fnv1a32(const void* key, u32 len) {
		const char* data = (char*)key;
		u32 hash = 0x811c9dc5;
		u32 prime = 0x1000193;

		for (usize i = 0; i < len; ++i) {
			u8 value = data[i];
			hash = hash ^ value;
			hash *= prime;
		}

		return hash;
	}

	inline u64 hash_fnv1a64(const void* key, u64 len) {
		const char* data = (char*)key;
		u64 hash = 0xcbf29ce484222325;
		u64 prime = 0x100000001b3;

		for (usize i = 0; i < len; ++i) {
			u8 value = data[i];
			hash = hash ^ value;
			hash *= prime;
		}

		return hash;
	}

	u32 hash_crc32(const void* data, usize size);
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
}

#endif