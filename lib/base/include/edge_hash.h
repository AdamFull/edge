#ifndef EDGE_HASH_H
#define EDGE_HASH_H

#include "edge_base.h"

#ifdef __cplusplus
extern "C" {
#endif

	typedef struct edge_hash128 {
		u64 low;
		u64 high;
	} edge_hash128_t;

	u32 edge_hash_crc32(const void* data, size_t size);

	u32 edge_hash_xxh32(const void* data, size_t size, u32 seed);
	u64 edge_hash_xxh64(const void* data, size_t size, u64 seed);

	u32 edge_hash_murmur3_32(const void* data, size_t size, u32 seed);
	edge_hash128_t edge_hash_murmur3_128(const void* data, size_t size, u32 seed);

	u32 edge_hash_int32(u32 value);
	u64 edge_hash_int64(u64 value);

	u32 edge_hash_string_32(const char* str);
	u64 edge_hash_string_64(const char* str);

	size_t edge_hash_pointer(const void* ptr);
	size_t edge_hash_combine(size_t hash1, size_t hash2);

	size_t edge_hash128_to_size(edge_hash128_t hash);

#ifdef __cplusplus
}
#endif

#endif