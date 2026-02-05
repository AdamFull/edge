#include "uuid.hpp"

namespace edge {
namespace detail {
static constexpr i8 hex_to_value_lut[256] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,  4,  5,  6,  7,  8,
    9,  -1, -1, -1, -1, -1, -1, -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1};

static constexpr char value_to_hex_lut[16] = {'0', '1', '2', '3', '4', '5',
                                              '6', '7', '8', '9', 'a', 'b',
                                              'c', 'd', 'e', 'f'};

static bool is_hex_char(const char c) {
  return hex_to_value_lut[static_cast<u8>(c)] != -1;
}

static u8 hex_to_byte(const char high, const char low) {
  const i8 h = hex_to_value_lut[static_cast<u8>(high)];
  const i8 l = hex_to_value_lut[static_cast<u8>(low)];
  if (h < 0 || l < 0) {
    return 0;
  }
  return static_cast<u8>((h << 4) | l);
}

static void byte_to_hex(const u8 byte, char *out) {
  out[0] = value_to_hex_lut[(byte >> 4) & 0x0F];
  out[1] = value_to_hex_lut[byte & 0x0F];
}
} // namespace detail

UUID uuid_v4_parse(const char *str) {
  if (!str) {
    return {};
  }

  const usize len = strlen(str);
  const bool has_hyphens = (len == 36);

  if (const bool no_hyphens = (len == 32); !has_hyphens && !no_hyphens) {
    return {};
  }

  UUID result;
  usize byte_idx = 0;
  usize str_idx = 0;

  constexpr usize hyphen_positions[] = {8, 13, 18, 23};
  usize hyphen_idx = 0;

  while (byte_idx < 16 && str_idx < len) {
    if (has_hyphens && hyphen_idx < 4 &&
        str_idx == hyphen_positions[hyphen_idx]) {
      if (str[str_idx] != '-') {
        return {};
      }
      str_idx++;
      hyphen_idx++;
      continue;
    }

    if (str_idx + 1 >= len) {
      return {};
    }

    if (!detail::is_hex_char(str[str_idx]) ||
        !detail::is_hex_char(str[str_idx + 1])) {
      return {};
    }

    result.bytes[byte_idx] =
        detail::hex_to_byte(str[str_idx], str[str_idx + 1]);
    byte_idx++;
    str_idx += 2;
  }

  if (byte_idx != 16) {
    return {};
  }

  return result;
}

usize uuid_to_string(const UUID &uuid, char *buffer, const usize buffer_size) {
  if (!buffer || buffer_size < 37) {
    return 0;
  }

  usize pos = 0;

  for (usize i = 0; i < 4; i++) {
    detail::byte_to_hex(uuid.bytes[i], &buffer[pos]);
    pos += 2;
  }
  buffer[pos++] = '-';

  for (usize i = 4; i < 6; i++) {
    detail::byte_to_hex(uuid.bytes[i], &buffer[pos]);
    pos += 2;
  }
  buffer[pos++] = '-';

  for (usize i = 6; i < 8; i++) {
    detail::byte_to_hex(uuid.bytes[i], &buffer[pos]);
    pos += 2;
  }
  buffer[pos++] = '-';

  for (usize i = 8; i < 10; i++) {
    detail::byte_to_hex(uuid.bytes[i], &buffer[pos]);
    pos += 2;
  }
  buffer[pos++] = '-';

  for (usize i = 10; i < 16; i++) {
    detail::byte_to_hex(uuid.bytes[i], &buffer[pos]);
    pos += 2;
  }

  buffer[pos] = '\0';
  return pos;
}

usize uuid_to_compact_string(const UUID &uuid, char *buffer,
                             const usize buffer_size) {
  if (!buffer || buffer_size < 33) {
    return 0;
  }

  usize pos = 0;
  for (usize i = 0; i < 16; i++) {
    detail::byte_to_hex(uuid.bytes[i], &buffer[pos]);
    pos += 2;
  }

  buffer[pos] = '\0';
  return pos;
}
} // namespace edge