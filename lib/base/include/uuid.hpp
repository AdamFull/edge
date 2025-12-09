#ifndef EDGE_UUID_H
#define EDGE_UUID_H

#include "stddef.hpp"

namespace edge {
	struct Rng;

	struct UUID {
		u8 m_bytes[16];

		bool operator==(const UUID& other) const {
			return memcmp(m_bytes, other.m_bytes, 16) == 0;
		}

		bool operator!=(const UUID& other) const {
			return memcmp(m_bytes, other.m_bytes, 16) != 0;
		}

		bool operator<(const UUID& other) const {
			return memcmp(m_bytes, other.m_bytes, 16) < 0;
		}

		bool operator<=(const UUID& other) const {
			return memcmp(m_bytes, other.m_bytes, 16) <= 0;
		}

		bool operator>(const UUID& other) const {
			return memcmp(m_bytes, other.m_bytes, 16) > 0;
		}

		bool operator>=(const UUID& other) const {
			return memcmp(m_bytes, other.m_bytes, 16) >= 0;
		}
	};

	void uuid_v4_create(Rng* rng, UUID* uuid);
	bool uuid_parse(const char* str, UUID* out_uuid);
	i32 uuid_to_string(const UUID* uuid, char* out_str, usize buffer_size);

	bool uuid_is_nil(const UUID* uuid);
	bool uuid_is_valid_v4(const UUID* uuid);
	u8 uuid_version(const UUID* uuid);
}

#endif 