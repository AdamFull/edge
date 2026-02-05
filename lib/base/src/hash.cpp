#include "hash.hpp"

#include <bit>

#if defined(EDGE_ARCH_X64)
#define EDGE_HASH_X86
#if defined(EDGE_HAS_SSE4_2)
#define EDGE_HASH_HAS_CRC32
#include <nmmintrin.h>
#endif
#elif defined(EDGE_ARCH_AARCH64)
#if defined(__ARM_FEATURE_CRC32)
#define EDGE_HASH_HAS_CRC32
#include <arm_acle.h>
#endif
#endif

namespace edge {
namespace detail {
static u32 read32(const void *ptr) {
  u32 result;
  memcpy(&result, ptr, sizeof(u32));
  return result;
}

static u64 read64(const void *ptr) {
  u64 result;
  memcpy(&result, ptr, sizeof(u64));
  return result;
}

static u32 g_crc32_table[256];
static bool g_crc32_table_initialized = false;

static void crc32_init_table() {
  for (u32 i = 0; i < 256; i++) {
    u32 crc = i;
    for (u32 j = 0; j < 8; j++) {
      crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
    }
    g_crc32_table[i] = crc;
  }
  g_crc32_table_initialized = true;
}
} // namespace detail

u32 hash_crc32(const void *data, usize size) {
  if (!detail::g_crc32_table_initialized) {
    detail::crc32_init_table();
  }

  auto bytes = static_cast<const u8 *>(data);
  u32 crc = 0xFFFFFFFF;

#if defined(EDGE_HASH_HAS_CRC32) && defined(EDGE_HASH_X86)
  while (size >= 8) {
    crc = static_cast<u32>(_mm_crc32_u64(crc, detail::read64(bytes)));
    bytes += 8;
    size -= 8;
  }
  if (size >= 4) {
    crc = _mm_crc32_u32(crc, detail::read32(bytes));
    bytes += 4;
    size -= 4;
  }
  while (size > 0) {
    crc = _mm_crc32_u8(crc, *bytes++);
    size--;
  }
#elif defined(EDGE_HASH_HAS_CRC32) && defined(EDGE_HASH_AARCH64)
  while (size >= 8) {
    crc = __crc32d(crc, detail::read64(bytes));
    bytes += 8;
    size -= 8;
  }
  if (size >= 4) {
    crc = __crc32w(crc, detail::read32(bytes));
    bytes += 4;
    size -= 4;
  }
  while (size > 0) {
    crc = __crc32b(crc, *bytes++);
    size--;
  }
#else
  for (usize i = 0; i < size; i++) {
    crc = (crc >> 8) ^ detail::g_crc32_table[(crc ^ bytes[i]) & 0xFF];
  }
#endif

  return ~crc;
}

usize hash_pointer(const void *ptr) {
  if constexpr (sizeof(usize) == 8) {
    return Hash<u64>{}(reinterpret_cast<u64>(ptr));
  } else {
    return Hash<u32>{}(static_cast<u32>(reinterpret_cast<uintptr_t>(ptr)));
  }
}
} // namespace edge