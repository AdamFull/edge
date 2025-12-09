#include "uuid.hpp"
#include "random.hpp"

#include <cstdio>
#include <cstring>

namespace edge {
	inline u8 hex_to_nibble(char c) {
		if (c >= '0' && c <= '9') {
			return (u8)(c - '0');
		}
		else if (c >= 'a' && c <= 'f') {
			return (u8)(c - 'a' + 10);
		}
		else if (c >= 'A' && c <= 'F') {
			return (u8)(c - 'A' + 10);
		}
		return 255;
	}

	void uuid_v4_create(Rng* rng, UUID* uuid) {
		if (!rng || !uuid) {
			if (uuid) {
				memset(uuid->m_bytes, 0, 16);
			}
			return;
		}

		rng_bytes(rng, uuid->m_bytes, 16);

		// Set version to 4 (random)
		uuid->m_bytes[6] = (uuid->m_bytes[6] & 0x0F) | 0x40;
		// Set variant to RFC 4122
		uuid->m_bytes[8] = (uuid->m_bytes[8] & 0x3F) | 0x80;
	}

	bool uuid_parse(const char* str, UUID* out_uuid) {
		if (!str || !out_uuid) {
			return false;
		}

		usize len = strlen(str);
		i32 hyphen_count = 0;
		for (usize i = 0; i < len; i++) {
			if (str[i] == '-') {
				hyphen_count++;
			}
		}

		// Format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx (36 chars, 4 hyphens)
		if (hyphen_count == 4 && len == 36) {
			const char* p = str;

			for (i32 i = 0; i < 16; i++) {
				// Skip hyphens at positions 8, 13, 18, 23
				if (i == 4 || i == 6 || i == 8 || i == 10) {
					if (*p != '-') return false;
					p++;
				}

				u8 high = hex_to_nibble(*p++);
				u8 low = hex_to_nibble(*p++);

				if (high == 255 || low == 255) return false;

				out_uuid->m_bytes[i] = (high << 4) | low;
			}

			return true;
		}
		// Format: xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx (32 chars, no hyphens)
		else if (hyphen_count == 0 && len == 32) {
			const char* p = str;

			for (i32 i = 0; i < 16; i++) {
				u8 high = hex_to_nibble(*p++);
				u8 low = hex_to_nibble(*p++);

				if (high == 255 || low == 255) return false;

				out_uuid->m_bytes[i] = (high << 4) | low;
			}

			return true;
		}

		return false;
	}

	i32 uuid_to_string(const UUID* uuid, char* out_str, usize buffer_size) {
		if (!uuid || !out_str || buffer_size < 37) {
			return -1;
		}

		return snprintf(out_str, buffer_size,
			"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
			uuid->m_bytes[0], uuid->m_bytes[1], uuid->m_bytes[2], uuid->m_bytes[3],
			uuid->m_bytes[4], uuid->m_bytes[5],
			uuid->m_bytes[6], uuid->m_bytes[7],
			uuid->m_bytes[8], uuid->m_bytes[9],
			uuid->m_bytes[10], uuid->m_bytes[11], uuid->m_bytes[12],
			uuid->m_bytes[13], uuid->m_bytes[14], uuid->m_bytes[15]);
	}

	bool uuid_is_nil(const UUID* uuid) {
		if (!uuid) {
			return true;
		}

		for (i32 i = 0; i < 16; i++) {
			if (uuid->m_bytes[i] != 0) {
				return false;
			}
		}

		return true;
	}

	bool uuid_is_valid_v4(const UUID* uuid) {
		if (!uuid) {
			return false;
		}

		// Check version is 4
		if ((uuid->m_bytes[6] & 0xF0) != 0x40) {
			return false;
		}

		// Check variant is RFC 4122
		if ((uuid->m_bytes[8] & 0xC0) != 0x80) {
			return false;
		}

		return true;
	}

	u8 uuid_version(const UUID* uuid) {
		if (!uuid) {
			return 0;
		}

		return (uuid->m_bytes[6] >> 4) & 0x0F;
	}
}