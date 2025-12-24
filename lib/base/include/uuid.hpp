#ifndef EDGE_UUID_H
#define EDGE_UUID_H

#include "hash.hpp"
#include "random.hpp"
#include "platform_detect.hpp"

namespace edge {
	struct alignas(16) UUID {
		union {
			u8 bytes[16];
			u32 dwords[4];
			u64 qwords[2];
		};

		constexpr UUID() : bytes{} {}
		constexpr UUID(u64 high, u64 low) : qwords{ low, high } {}
		constexpr UUID(u32 d0, u32 d1, u32 d2, u32 d3) : dwords{ d0, d1, d2, d3 } {}

		EDGE_FORCE_INLINE bool is_null() const noexcept {
			return qwords[0] == 0 && qwords[1] == 0;
		}

		EDGE_FORCE_INLINE void set_null() noexcept {
			qwords[0] = 0;
			qwords[1] = 0;
		}

		constexpr u8 version() const noexcept {
			return (bytes[6] >> 4) & 0x0F;
		}

		constexpr u8 variant() const noexcept {
			return (bytes[8] >> 6) & 0x03;
		}
	};

	template<>
	struct Hash<UUID> {
		EDGE_FORCE_INLINE usize operator()(const UUID& uuid) const noexcept {
			return static_cast<usize>(uuid.qwords[0] ^ uuid.qwords[1]);
		}
	};

	EDGE_FORCE_INLINE bool operator==(const UUID& lhs, const UUID& rhs) noexcept {
		return lhs.qwords[0] == rhs.qwords[0] && lhs.qwords[1] == rhs.qwords[1];
	}

	template<RngAlgorithm Algorithm>
	UUID uuid_v4_generate(Rng<Algorithm>& rng) noexcept {
		UUID uuid;
		rng.gen_bytes(uuid.bytes, 16);
		uuid.bytes[6] = (uuid.bytes[6] & 0x0F) | 0x40;
		uuid.bytes[8] = (uuid.bytes[8] & 0x3F) | 0x80;
		return uuid;
	}

	UUID uuid_v4_parse(const char* str) noexcept;
	usize uuid_to_string(const UUID& uuid, char* buffer, usize buffer_size) noexcept;
	usize uuid_to_compact_string(const UUID& uuid, char* buffer, usize buffer_size) noexcept;
}

#endif