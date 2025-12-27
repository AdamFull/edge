#ifndef EDGE_STRING_H
#define EDGE_STRING_H

#include "allocator.hpp"
#include "hash.hpp"

#include <cstring>
#include <cstdlib>

namespace edge {
	namespace detail {
		constexpr usize STRING_DEFAULT_CAPACITY = 16;

		namespace unicode {
			inline constexpr bool is_continuation_byte(char8_t c) noexcept {
				return (static_cast<char8_t>(c) & 0xC0) == 0x80;
			}

			inline constexpr usize char_byte_count(char8_t fb) noexcept {
				auto uc = static_cast<unsigned char>(fb);
				if ((uc & 0x80) == 0) { 
					return 1; 
				}
				if ((uc & 0xE0) == 0xC0) { 
					return 2; 
				}
				if ((uc & 0xF0) == 0xE0) {
					return 3;
				}
				if ((uc & 0xF8) == 0xF0) { 
					return 4; 
				}
				return 0;
			}

			inline constexpr bool is_surrogate(char32_t cp) noexcept {
				return cp >= 0xD800u && cp <= 0xDFFFu;
			}

			inline constexpr bool is_high_surrogate(char16_t cp) noexcept {
				return cp >= 0xD800u && cp <= 0xDBFFu;
			}

			inline constexpr bool is_high_surrogate_invalid(char16_t cp) noexcept {
				return cp < 0xD800u || cp > 0xDBFFu;
			}

			inline constexpr bool is_low_surrogate(char16_t cp) noexcept {
				return cp >= 0xDC00u && cp <= 0xDFFFu;
			}

			inline constexpr bool is_low_surrogate_invalid(char16_t cp) noexcept {
				return cp < 0xDC00u || cp > 0xDFFF;
			}
		}
	}

	struct String {
		char8_t* m_data = nullptr;
		usize m_length = 0ull;
		usize m_capacity = 0ull;

		bool create(NotNull<const Allocator*> alloc, usize initial_capacity = 0) {
			if (initial_capacity < detail::STRING_DEFAULT_CAPACITY) {
				initial_capacity = detail::STRING_DEFAULT_CAPACITY;
			}

			m_data = alloc->allocate_array<char8_t>(initial_capacity);
			if (!m_data) {
				return false;
			}

			m_data[0] = u8'\0';
			m_length = 0;
			m_capacity = initial_capacity;

			return true;
		}

		template<typename CharT>
			requires std::same_as<CharT, char> || std::same_as<CharT, char8_t>
		bool create_from(NotNull<const Allocator*> alloc, const CharT* cstr) {
			if (!cstr) {
				return create(alloc, detail::STRING_DEFAULT_CAPACITY);
			}

			usize len = strlen((const char*)cstr);
			if (!create(alloc, len + 1)) {
				return false;
			}

			memcpy(m_data, cstr, len + 1);
			m_length = len;

			return true;
		}

		template<typename CharT>
			requires std::same_as<CharT, char> || std::same_as<CharT, char8_t>
		bool create_from_buffer(NotNull<const Allocator*> alloc, const CharT* buffer, usize length) {
			if (!buffer) {
				return create(alloc, detail::STRING_DEFAULT_CAPACITY);
			}

			if (!create(alloc, length + 1)) {
				return false;
			}

			memcpy(m_data, buffer, length);
			m_data[length] = u8'\0';
			m_length = length;

			return true;
		}

		void destroy(NotNull<const Allocator*> alloc) {
			if (m_data) {
				alloc->deallocate_array(m_data, m_capacity);
			}
		}

		inline void clear() {
			m_length = 0;
			if (m_data) {
				m_data[0] = u8'\0';
			}
		}

		bool reserve(NotNull<const Allocator*> alloc, usize capacity) {
			if (capacity == 0) {
				capacity = detail::STRING_DEFAULT_CAPACITY;
			}
			
			if (capacity <= m_capacity) {
				return true;
			}

			char8_t* new_data = (char8_t*)alloc->realloc(m_data, capacity, 1);
			if (!new_data) {
				return false;
			}

			m_data = new_data;
			m_capacity = capacity;

			return true;
		}

		bool append(NotNull<const Allocator*> alloc, const char8_t* text) {
			if (!text) {
				return false;
			}

			usize text_len = strlen((const char*)text);
			if (text_len == 0) {
				return true;
			}

			usize required = m_length + text_len + 1;
			if (required > m_capacity) {
				usize new_capacity = m_capacity * 2;
				while (new_capacity < required) {
					new_capacity *= 2;
				}
				if (!reserve(alloc, new_capacity)) {
					return false;
				}
			}

			memcpy(m_data + m_length, text, text_len + 1);
			m_length += text_len;

			return true;
		}

		bool append(NotNull<const Allocator*> alloc, const char8_t* buffer, usize length) {
			if (!buffer || length == 0) {
				return false;
			}

			usize required = m_length + length + 1;
			if (required > m_capacity) {
				usize new_capacity = m_capacity * 2;
				while (new_capacity < required) {
					new_capacity *= 2;
				}
				if (!reserve(alloc, new_capacity)) {
					return false;
				}
			}

			memcpy(m_data + m_length, buffer, length);
			m_length += length;
			m_data[m_length] = u8'\0';

			return true;
		}

		bool append(NotNull<const Allocator*> alloc, char8_t c) noexcept {
			usize required = m_length + 2;
			if (required > m_capacity) {
				if (!reserve(alloc, m_capacity * 2)) {
					return false;
				}
			}

			m_data[m_length] = c;
			m_length++;
			m_data[m_length] = u8'\0';
			return true;
		}

		bool append(NotNull<const Allocator*> alloc, char16_t cp) noexcept {
			usize required = m_length + 2;
			if (required > m_capacity) {
				if (!reserve(alloc, m_capacity * 2)) {
					return false;
				}
			}

			return encode_u16_utf8(alloc, cp);
		}

		bool append(NotNull<const Allocator*> alloc, char16_t cp_high, char16_t cp_low) noexcept {
			usize required = m_length + 2;
			if (required > m_capacity) {
				if (!reserve(alloc, m_capacity * 2)) {
					return false;
				}
			}

			return encode_u16_utf8(alloc, cp_high, cp_low);
		}

		bool append(NotNull<const Allocator*> alloc, char32_t cp) noexcept {
			usize required = m_length + 2;
			if (required > m_capacity) {
				if (!reserve(alloc, m_capacity * 2)) {
					return false;
				}
			}

			return encode_u32_utf8(alloc, cp);
		}

		bool insert(NotNull<const Allocator*> alloc, usize pos, const char8_t* text) {
			if (!text || pos > m_length) {
				return false;
			}

			usize text_len = strlen((const char*)text);
			if (text_len == 0) {
				return true;
			}

			usize required = m_length + text_len + 1;
			if (required > m_capacity) {
				usize new_capacity = m_capacity * 2;
				while (new_capacity < required) {
					new_capacity *= 2;
				}
				if (!reserve(alloc, new_capacity)) {
					return false;
				}
			}

			memmove(m_data + pos + text_len, m_data + pos, m_length - pos + 1);

			memcpy(m_data + pos, text, text_len);
			m_length += text_len;

			return true;
		}

		bool remove(usize pos, usize length) {
			if (pos >= m_length) {
				return false;
			}

			if (pos + length > m_length) {
				length = m_length - pos;
			}

			memmove(m_data + pos, m_data + pos + length, m_length - pos - length + 1);
			m_length -= length;

			return true;
		}

		bool empty() const noexcept {
			return m_length == 0;
		}

		template<typename CharT>
			requires std::same_as<CharT, char> || std::same_as<CharT, char8_t>
		i32 compare(const CharT* other) {
			if (!m_data) {
				return other ? -1 : 0;
			}

			if (!other) {
				return 1;
			}

			return strcmp((const char*)m_data, (const char*)other);
		}

		i32 compare(const String str2) {
			if (!m_data) {
				return (str2.m_data) ? -1 : 0;
			}

			if (!str2.m_data) {
				return 1;
			}

			return strcmp((const char*)m_data, (const char*)str2.m_data);
		}

		i32 find(const char8_t* needle) {
			if (!m_data || !needle) {
				return -1;
			}

			const char8_t* found = (char8_t*)strstr((char *const)m_data, (const char* const)needle);
			if (!found) {
				return -1;
			}

			return static_cast<i32>(found - m_data);
		}

		bool duplicate(NotNull<const Allocator*> alloc, String dest) {
			return dest.create_from_buffer(alloc, m_data, m_length);
		}

		char8_t* begin() noexcept {
			return m_data;
		}

		char8_t* end() noexcept {
			return m_data + m_length;
		}

		const char8_t* begin() const noexcept {
			return m_data;
		}

		const char8_t* end() const noexcept {
			return m_data + m_length;
		}

	private:
		bool encode_u32_utf8(NotNull<const Allocator*> alloc, char32_t cp) noexcept {
			if (detail::unicode::is_surrogate(cp)) {
				return false;
			}

			if (cp <= 0x7F) {
				// 1-byte sequence: 0xxxxxxx
				append(alloc, static_cast<char8_t>(cp));
			}
			else if (cp <= 0x7FF) {
				// 2-byte sequence: 110xxxxx 10xxxxxx
				append(alloc, static_cast<char8_t>(0xC0 | (cp >> 6)));
				append(alloc, static_cast<char8_t>(0x80 | (cp & 0x3F)));
			}
			else if (cp <= 0xFFFF) {
				// 3-byte sequence: 1110xxxx 10xxxxxx 10xxxxxx
				append(alloc, static_cast<char8_t>(0xE0 | (cp >> 12)));
				append(alloc, static_cast<char8_t>(0x80 | ((cp >> 6) & 0x3F)));
				append(alloc, static_cast<char8_t>(0x80 | (cp & 0x3F)));
			}
			else if (cp <= 0x10FFFF) {
				// 4-byte sequence: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
				append(alloc, static_cast<char8_t>(0xF0 | (cp >> 18)));
				append(alloc, static_cast<char8_t>(0x80 | ((cp >> 12) & 0x3F)));
				append(alloc, static_cast<char8_t>(0x80 | ((cp >> 6) & 0x3F)));
				append(alloc, static_cast<char8_t>(0x80 | (cp & 0x3F)));
			}
			else {
				return false;
			}

			return true;
		}

		bool encode_u16_utf8(NotNull<const Allocator*> alloc, char16_t cp) noexcept {
			if (detail::unicode::is_high_surrogate(cp) || detail::unicode::is_low_surrogate(cp)) {
				return false;
			}
			return encode_u32_utf8(alloc, static_cast<char32_t>(cp));
		}

		bool encode_u16_utf8(NotNull<const Allocator*> alloc, char16_t cp_high, char16_t cp_low) noexcept {
			if (detail::unicode::is_high_surrogate_invalid(cp_high) || detail::unicode::is_low_surrogate_invalid(cp_low)) {
				return false;
			}

			char32_t cp = static_cast<char32_t>(0x10000u + ((cp_high - 0xD800u) << 10) + (cp_low - 0xDC00u));
			return encode_u32_utf8(alloc, cp);
		}
	};

	inline bool operator==(const String& lhs, const String& rhs) noexcept {
		if (lhs.m_length != rhs.m_length) {
			return false;
		}
		if (lhs.m_data == rhs.m_data) {
			return true;
		}
		if (!lhs.m_data || !rhs.m_data) {
			return false;
		}

		if (lhs.m_length > 2 && rhs.m_length > 2) {
			return lhs.m_data[0] == rhs.m_data[0] && !memcmp(lhs.m_data + 1, rhs.m_data + 1, lhs.m_length - 1);
		}
		return !memcmp(lhs.m_data, rhs.m_data, lhs.m_length);
	}

	template<>
	struct Hash<String> {
		EDGE_FORCE_INLINE usize operator()(const String& string) const noexcept {
#if EDGE_HAS_SSE4_2
			return hash_crc32(string.m_data, string.m_length);
#else
			return hash_fnv1a64(string.m_data, string.m_length);
#endif
		}
	};
}

#endif