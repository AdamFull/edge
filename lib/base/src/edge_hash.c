#include "edge_hash.h"
#include "edge_platform_detect.h"

#include <string.h>

#if defined(EDGE_ARCH_X64)
#define EDGE_HASH_X86
#if defined(__SSE4_2__)
#define EDGE_HASH_HAS_CRC32
#include <nmmintrin.h>
#endif
#elif defined(EDGE_ARCH_AARCH64)
#if defined(__ARM_FEATURE_CRC32)
#define EDGE_HASH_HAS_CRC32
#include <arm_acle.h>
#endif
#endif

static inline u32 edge_rotl32(u32 x, i32 r) {
    return (x << r) | (x >> (32 - r));
}

static inline u64 edge_rotl64(u64 x, i32 r) {
    return (x << r) | (x >> (64 - r));
}

static inline u32 edge_read32(const void* ptr) {
    u32 result;
    memcpy(&result, ptr, sizeof(u32));
    return result;
}

static inline u64 edge_read64(const void* ptr) {
    u64 result;
    memcpy(&result, ptr, sizeof(u64));
    return result;
}


static u32 crc32_table[256];
static bool crc32_table_initialized = false;

static void crc32_init_table(void) {
    for (u32 i = 0; i < 256; i++) {
        u32 crc = i;
        for (u32 j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
        crc32_table[i] = crc;
    }
    crc32_table_initialized = true;
}

u32 edge_hash_crc32(const void* data, size_t size) {
    if (!crc32_table_initialized) {
        crc32_init_table();
    }

    const u8* bytes = (const u8*)data;
    u32 crc = 0xFFFFFFFF;

#if defined(EDGE_HASH_HAS_CRC32) && defined(EDGE_ARCH_X64)
     while (size >= 8) {
         crc = (u32)_mm_crc32_u64(crc, edge_read64(bytes));
         bytes += 8;
         size -= 8;
     }
     if (size >= 4) {
         crc = _mm_crc32_u32(crc, edge_read32(bytes));
         bytes += 4;
         size -= 4;
     }
     while (size > 0) {
         crc = _mm_crc32_u8(crc, *bytes++);
         size--;
     }
#elif defined(EDGE_HASH_HAS_CRC32) && defined(EDGE_ARCH_AARCH64)
     while (size >= 8) {
         crc = __crc32d(crc, edge_read64(bytes));
         bytes += 8;
         size -= 8;
     }
     if (size >= 4) {
         crc = __crc32w(crc, edge_read32(bytes));
         bytes += 4;
         size -= 4;
     }
     while (size > 0) {
         crc = __crc32b(crc, *bytes++);
         size--;
     }
#else
    for (size_t i = 0; i < size; i++) {
        crc = (crc >> 8) ^ crc32_table[(crc ^ bytes[i]) & 0xFF];
    }
#endif

    return ~crc;
}

#define XXH_PRIME32_1 0x9E3779B1U
#define XXH_PRIME32_2 0x85EBCA77U
#define XXH_PRIME32_3 0xC2B2AE3DU
#define XXH_PRIME32_4 0x27D4EB2FU
#define XXH_PRIME32_5 0x165667B1U

u32 edge_hash_xxh32(const void* data, size_t size, u32 seed) {
    const u8* bytes = (const u8*)data;
    const u8* end = bytes + size;
    u32 h32;

    if (size >= 16) {
        const u8* limit = end - 16;
        u32 v1 = seed + XXH_PRIME32_1 + XXH_PRIME32_2;
        u32 v2 = seed + XXH_PRIME32_2;
        u32 v3 = seed + 0;
        u32 v4 = seed - XXH_PRIME32_1;

        do {
            v1 += edge_read32(bytes) * XXH_PRIME32_2;
            v1 = edge_rotl32(v1, 13);
            v1 *= XXH_PRIME32_1;
            bytes += 4;

            v2 += edge_read32(bytes) * XXH_PRIME32_2;
            v2 = edge_rotl32(v2, 13);
            v2 *= XXH_PRIME32_1;
            bytes += 4;

            v3 += edge_read32(bytes) * XXH_PRIME32_2;
            v3 = edge_rotl32(v3, 13);
            v3 *= XXH_PRIME32_1;
            bytes += 4;

            v4 += edge_read32(bytes) * XXH_PRIME32_2;
            v4 = edge_rotl32(v4, 13);
            v4 *= XXH_PRIME32_1;
            bytes += 4;
        } while (bytes <= limit);

        h32 = edge_rotl32(v1, 1) + edge_rotl32(v2, 7) + edge_rotl32(v3, 12) + edge_rotl32(v4, 18);
    }
    else {
        h32 = seed + XXH_PRIME32_5;
    }

    h32 += (u32)size;

    while (bytes + 4 <= end) {
        h32 += edge_read32(bytes) * XXH_PRIME32_3;
        h32 = edge_rotl32(h32, 17) * XXH_PRIME32_4;
        bytes += 4;
    }

    while (bytes < end) {
        h32 += (*bytes) * XXH_PRIME32_5;
        h32 = edge_rotl32(h32, 11) * XXH_PRIME32_1;
        bytes++;
    }

    h32 ^= h32 >> 15;
    h32 *= XXH_PRIME32_2;
    h32 ^= h32 >> 13;
    h32 *= XXH_PRIME32_3;
    h32 ^= h32 >> 16;

    return h32;
}

#define XXH_PRIME64_1 0x9E3779B185EBCA87ULL
#define XXH_PRIME64_2 0xC2B2AE3D27D4EB4FULL
#define XXH_PRIME64_3 0x165667B19E3779F9ULL
#define XXH_PRIME64_4 0x85EBCA77C2B2AE63ULL
#define XXH_PRIME64_5 0x27D4EB2F165667C5ULL

u64 edge_hash_xxh64(const void* data, size_t size, u64 seed) {
    const u8* bytes = (const u8*)data;
    const u8* end = bytes + size;
    u64 h64;

    if (size >= 32) {
        const u8* limit = end - 32;
        u64 v1 = seed + XXH_PRIME64_1 + XXH_PRIME64_2;
        u64 v2 = seed + XXH_PRIME64_2;
        u64 v3 = seed + 0;
        u64 v4 = seed - XXH_PRIME64_1;

        do {
            v1 += edge_read64(bytes) * XXH_PRIME64_2;
            v1 = edge_rotl64(v1, 31);
            v1 *= XXH_PRIME64_1;
            bytes += 8;

            v2 += edge_read64(bytes) * XXH_PRIME64_2;
            v2 = edge_rotl64(v2, 31);
            v2 *= XXH_PRIME64_1;
            bytes += 8;

            v3 += edge_read64(bytes) * XXH_PRIME64_2;
            v3 = edge_rotl64(v3, 31);
            v3 *= XXH_PRIME64_1;
            bytes += 8;

            v4 += edge_read64(bytes) * XXH_PRIME64_2;
            v4 = edge_rotl64(v4, 31);
            v4 *= XXH_PRIME64_1;
            bytes += 8;
        } while (bytes <= limit);

        h64 = edge_rotl64(v1, 1) + edge_rotl64(v2, 7) + edge_rotl64(v3, 12) + edge_rotl64(v4, 18);

        v1 *= XXH_PRIME64_2; v1 = edge_rotl64(v1, 31); v1 *= XXH_PRIME64_1;
        h64 ^= v1; h64 = h64 * XXH_PRIME64_1 + XXH_PRIME64_4;

        v2 *= XXH_PRIME64_2; v2 = edge_rotl64(v2, 31); v2 *= XXH_PRIME64_1;
        h64 ^= v2; h64 = h64 * XXH_PRIME64_1 + XXH_PRIME64_4;

        v3 *= XXH_PRIME64_2; v3 = edge_rotl64(v3, 31); v3 *= XXH_PRIME64_1;
        h64 ^= v3; h64 = h64 * XXH_PRIME64_1 + XXH_PRIME64_4;

        v4 *= XXH_PRIME64_2; v4 = edge_rotl64(v4, 31); v4 *= XXH_PRIME64_1;
        h64 ^= v4; h64 = h64 * XXH_PRIME64_1 + XXH_PRIME64_4;
    }
    else {
        h64 = seed + XXH_PRIME64_5;
    }

    h64 += (u64)size;

    while (bytes + 8 <= end) {
        u64 k1 = edge_read64(bytes);
        k1 *= XXH_PRIME64_2;
        k1 = edge_rotl64(k1, 31);
        k1 *= XXH_PRIME64_1;
        h64 ^= k1;
        h64 = edge_rotl64(h64, 27) * XXH_PRIME64_1 + XXH_PRIME64_4;
        bytes += 8;
    }

    if (bytes + 4 <= end) {
        h64 ^= (u64)(edge_read32(bytes)) * XXH_PRIME64_1;
        h64 = edge_rotl64(h64, 23) * XXH_PRIME64_2 + XXH_PRIME64_3;
        bytes += 4;
    }

    while (bytes < end) {
        h64 ^= (*bytes) * XXH_PRIME64_5;
        h64 = edge_rotl64(h64, 11) * XXH_PRIME64_1;
        bytes++;
    }

    h64 ^= h64 >> 33;
    h64 *= XXH_PRIME64_2;
    h64 ^= h64 >> 29;
    h64 *= XXH_PRIME64_3;
    h64 ^= h64 >> 32;

    return h64;
}

static inline u32 murmur3_fmix32(u32 h) {
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}

u32 edge_hash_murmur3_32(const void* data, size_t size, u32 seed) {
    const u8* bytes = (const u8*)data;
    const size_t nblocks = size / 4;

    u32 h1 = seed;
    const u32 c1 = 0xcc9e2d51;
    const u32 c2 = 0x1b873593;

    /* Body */
    const u32* blocks = (const u32*)bytes;
    for (size_t i = 0; i < nblocks; i++) {
        u32 k1 = edge_read32(blocks + i);

        k1 *= c1;
        k1 = edge_rotl32(k1, 15);
        k1 *= c2;

        h1 ^= k1;
        h1 = edge_rotl32(h1, 13);
        h1 = h1 * 5 + 0xe6546b64;
    }

    /* Tail */
    const u8* tail = bytes + nblocks * 4;
    u32 k1 = 0;

    switch (size & 3) {
    case 3: k1 ^= tail[2] << 16; /* fallthrough */
    case 2: k1 ^= tail[1] << 8;  /* fallthrough */
    case 1: k1 ^= tail[0];
        k1 *= c1;
        k1 = edge_rotl32(k1, 15);
        k1 *= c2;
        h1 ^= k1;
    }

    /* Finalization */
    h1 ^= (u32)size;
    h1 = murmur3_fmix32(h1);

    return h1;
}

static inline u64 murmur3_fmix64(u64 k) {
    k ^= k >> 33;
    k *= 0xff51afd7ed558ccdULL;
    k ^= k >> 33;
    k *= 0xc4ceb9fe1a85ec53ULL;
    k ^= k >> 33;
    return k;
}

edge_hash128_t edge_hash_murmur3_128(const void* data, size_t size, u32 seed) {
    const u8* bytes = (const u8*)data;
    const size_t nblocks = size / 16;

    u64 h1 = seed;
    u64 h2 = seed;

    const u64 c1 = 0x87c37b91114253d5ULL;
    const u64 c2 = 0x4cf5ad432745937fULL;

    /* Body */
    const u64* blocks = (const u64*)bytes;
    for (size_t i = 0; i < nblocks; i++) {
        u64 k1 = edge_read64(blocks + i * 2);
        u64 k2 = edge_read64(blocks + i * 2 + 1);

        k1 *= c1; k1 = edge_rotl64(k1, 31); k1 *= c2; h1 ^= k1;
        h1 = edge_rotl64(h1, 27); h1 += h2; h1 = h1 * 5 + 0x52dce729;

        k2 *= c2; k2 = edge_rotl64(k2, 33); k2 *= c1; h2 ^= k2;
        h2 = edge_rotl64(h2, 31); h2 += h1; h2 = h2 * 5 + 0x38495ab5;
    }

    /* Tail */
    const u8* tail = bytes + nblocks * 16;
    u64 k1 = 0;
    u64 k2 = 0;

    switch (size & 15) {
    case 15: k2 ^= ((u64)tail[14]) << 48; /* fallthrough */
    case 14: k2 ^= ((u64)tail[13]) << 40; /* fallthrough */
    case 13: k2 ^= ((u64)tail[12]) << 32; /* fallthrough */
    case 12: k2 ^= ((u64)tail[11]) << 24; /* fallthrough */
    case 11: k2 ^= ((u64)tail[10]) << 16; /* fallthrough */
    case 10: k2 ^= ((u64)tail[9]) << 8;   /* fallthrough */
    case  9: k2 ^= ((u64)tail[8]) << 0;
        k2 *= c2; k2 = edge_rotl64(k2, 33); k2 *= c1; h2 ^= k2;
        /* fallthrough */
    case  8: k1 ^= ((u64)tail[7]) << 56; /* fallthrough */
    case  7: k1 ^= ((u64)tail[6]) << 48; /* fallthrough */
    case  6: k1 ^= ((u64)tail[5]) << 40; /* fallthrough */
    case  5: k1 ^= ((u64)tail[4]) << 32; /* fallthrough */
    case  4: k1 ^= ((u64)tail[3]) << 24; /* fallthrough */
    case  3: k1 ^= ((u64)tail[2]) << 16; /* fallthrough */
    case  2: k1 ^= ((u64)tail[1]) << 8;  /* fallthrough */
    case  1: k1 ^= ((u64)tail[0]) << 0;
        k1 *= c1; k1 = edge_rotl64(k1, 31); k1 *= c2; h1 ^= k1;
    }

    /* Finalization */
    h1 ^= size; h2 ^= size;
    h1 += h2;
    h2 += h1;
    h1 = murmur3_fmix64(h1);
    h2 = murmur3_fmix64(h2);
    h1 += h2;
    h2 += h1;

    edge_hash128_t result = { h1, h2 };
    return result;
}

// https://gist.github.com/badboy/6267743
u32 edge_hash_int32(u32 value) {
    value = (value ^ 61) ^ (value >> 16);
    value = value + (value << 3);
    value = value ^ (value >> 4);
    value = value * 0x27d4eb2d;
    value = value ^ (value >> 15);
    return value;
}

u64 edge_hash_int64(u64 value) {
    value = (~value) + (value << 21);
    value = value ^ (value >> 24);
    value = (value + (value << 3)) + (value << 8);
    value = value ^ (value >> 14);
    value = (value + (value << 2)) + (value << 4);
    value = value ^ (value >> 28);
    value = value + (value << 31);
    return value;
}

u32 edge_hash_string_32(const char* str) {
    return edge_hash_xxh32(str, strlen(str), 0u);
}

u64 edge_hash_string_64(const char* str) {
    return edge_hash_xxh64(str, strlen(str), 0ull);
}

size_t edge_hash_pointer(const void* ptr) {
#if __UINTPTR_MAX__ == __UINT64_MAX__
    return edge_hash_int64((u64)(uintptr_t)ptr);
#else
    return edge_hash_int32((u32)(uintptr_t)ptr);
#endif
}

// https://stackoverflow.com/questions/2590677/how-do-i-combine-hash-values-in-c0x
size_t edge_hash_combine(size_t hash1, size_t hash2) {
#if __UINTPTR_MAX__ == __UINT64_MAX__
    hash1 ^= hash2 + 0x9e3779b97f4a7c15ULL + (hash1 << 6) + (hash1 >> 2);
#else
    hash1 ^= hash2 + 0x9e3779b9 + (hash1 << 6) + (hash1 >> 2);
#endif
    return hash1;
}

size_t edge_hash128_to_size(edge_hash128_t hash) {
#if __UINTPTR_MAX__ == __UINT64_MAX__
    return hash.low ^ hash.high;
#else
    return (size_t)(hash.low ^ hash.high);
#endif
}