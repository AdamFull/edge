#ifndef EDGE_STRING_H
#define EDGE_STRING_H

#include "allocator.hpp"
#include "hash.hpp"

#include <cstring>
#include <cstdlib>

namespace edge {
	namespace detail {
		constexpr usize STRING_DEFAULT_CAPACITY = 16;

		namespace utf8 {
			inline constexpr bool is_continuation_byte(char8_t c) noexcept {
				return (c & 0xC0) == 0x80;
			}

			inline constexpr usize sequence_length(u8 first_byte) noexcept {
				if ((first_byte & 0x80) == 0x00) return 1;      // 0xxxxxxx
				if ((first_byte & 0xE0) == 0xC0) return 2;      // 110xxxxx
				if ((first_byte & 0xF0) == 0xE0) return 3;      // 1110xxxx
				if ((first_byte & 0xF8) == 0xF0) return 4;      // 11110xxx
				return 0;
			}

			inline constexpr bool is_valid_codepoint(char32_t cp) noexcept {
				return cp <= 0x10FFFF && (cp < 0xD800 || cp > 0xDFFF);
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

			inline constexpr usize char_byte_count(char8_t fb) noexcept {
				auto uc = static_cast<u8>(fb);
				if ((uc & 0x80) == 0) return 1;
				if ((uc & 0xE0) == 0xC0) return 2;
				if ((uc & 0xF0) == 0xE0) return 3;
				if ((uc & 0xF8) == 0xF0) return 4;
				return 0;
			}

			inline bool decode_char(const char8_t* utf8, usize remaining, char32_t& cp, usize& readed) noexcept {
				if (remaining == 0) {
					return false;
				}

				char8_t uc0 = utf8[0];
				usize len = char_byte_count(uc0);

				switch (len)
				{
				case 1: {
					readed = 1;
					cp = static_cast<char32_t>(uc0);
					return true;
				}
				case 2: {
					char8_t uc1 = utf8[1];
					if (!is_continuation_byte(uc1)) {
						return false;
					}
					readed = 2;
					cp = static_cast<char32_t>(((uc0 & 0x1F) << 6) | (uc1 & 0x3F));
					return true;
				}
				case 3: {
					char8_t uc1 = utf8[1];
					char8_t uc2 = utf8[2];
					if (!is_continuation_byte(uc1) || !is_continuation_byte(uc2)) {
						return false;
					}
					readed = 3;
					cp = static_cast<char32_t>(((uc0 & 0x0F) << 12) | ((uc1 & 0x3F) << 6) | (uc2 & 0x3F));
					return true;
				}
				case 4: {
					char8_t uc1 = utf8[1];
					char8_t uc2 = utf8[2];
					char8_t uc3 = utf8[3];
					if (!is_continuation_byte(uc1) || !is_continuation_byte(uc2) || !is_continuation_byte(uc3)) {
						return false;
					}
					readed = 4;
					cp = static_cast<char32_t>(((uc0 & 0x07) << 18) | ((uc1 & 0x3F) << 12) | ((uc2 & 0x3F) << 6) | (uc3 & 0x3F));
					return true;
				}
				default: {
					readed = 1;
					cp = 0xFFFD;
					return false;
				}
				}
			}

			inline bool encode_char(char32_t cp, char8_t* out, usize& len) noexcept {
				if (!is_valid_codepoint(cp)) {
					return false;
				}

				if (cp <= 0x7F) {
					// 1-byte sequence: 0xxxxxxx
					out[0] = static_cast<char8_t>(cp);
					len = 1;
				}
				else if (cp <= 0x7FF) {
					// 2-byte sequence: 110xxxxx 10xxxxxx
					out[0] = static_cast<char8_t>(0xC0 | (cp >> 6));
					out[1] = static_cast<char8_t>(0x80 | (cp & 0x3F));
					len = 2;
				}
				else if (cp <= 0xFFFF) {
					// 3-byte sequence: 1110xxxx 10xxxxxx 10xxxxxx
					out[0] = static_cast<char8_t>(0xE0 | (cp >> 12));
					out[1] = static_cast<char8_t>(0x80 | ((cp >> 6) & 0x3F));
					out[2] = static_cast<char8_t>(0x80 | (cp & 0x3F));
					len = 3;
				}
				else if (cp <= 0x10FFFF) {
					// 4-byte sequence: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
					out[0] = static_cast<char8_t>(0xF0 | (cp >> 18));
					out[1] = static_cast<char8_t>(0x80 | ((cp >> 12) & 0x3F));
					out[2] = static_cast<char8_t>(0x80 | ((cp >> 6) & 0x3F));
					out[3] = static_cast<char8_t>(0x80 | (cp & 0x3F));
					len = 4;
				}
				else {
					len = 0;
					return false;
				}

				return true;
			}

			inline bool encode_char(char16_t cp, char8_t* out, usize& len) noexcept {
				if (is_high_surrogate(cp) || is_low_surrogate(cp)) {
					return false;
				}

				return encode_char(static_cast<char32_t>(cp), out, len);
			}

			inline bool encode_char(char16_t cp_high, char16_t cp_low, char8_t* out, usize& len) noexcept {
				if (is_high_surrogate_invalid(cp_high) || is_low_surrogate_invalid(cp_low)) {
					return false;
				}

				char32_t cp = static_cast<char32_t>(0x10000u + ((cp_high - 0xD800u) << 10) + (cp_low - 0xDC00u));
				return encode_char(cp, out, len);
			}

			inline bool validate_utf8(const char8_t* data, usize length) noexcept {
				usize pos = 0;
				while (pos < length) {
					char32_t cp = 0;
					usize cp_size = 0;
					if (!decode_char(data + pos, length - pos, cp, cp_size)) {
						return false;
					}
					pos += cp_size;
				}
				return true;
			}
		}
	}

	struct String {
		char8_t* m_data = nullptr;
		usize m_length = 0ull;
		usize m_capacity = 0ull;

		bool from_raw(NotNull<const Allocator*> alloc, const char* cstr, usize len) noexcept {
			if (!cstr) {
				return allocate(alloc, len + 1);
			}

			memcpy(m_data, cstr, len);
			m_data[len] = u8'\0';
			m_length = len;

			return true;
		}

		bool from_utf8(NotNull<const Allocator*> alloc, const char8_t* cstr, usize len) noexcept {
			if (!cstr) {
				return allocate(alloc, detail::STRING_DEFAULT_CAPACITY);
			}

			if (!detail::utf8::validate_utf8(cstr, len)) {
				return false;
			}

			m_data = (char8_t*)alloc->malloc(len + 1, alignof(char8_t));
			if (!m_data) {
				return false;
			}

			memcpy(m_data, cstr, len);
			m_data[len] = u8'\0';
			m_length = len;

			return true;
		}
	
		bool from_utf16(NotNull<const Allocator*> alloc, const char16_t* str, usize len) noexcept {
			if (!str) {
				return allocate(alloc, detail::STRING_DEFAULT_CAPACITY);
			}

			usize capacity = len * 4;
			m_data = (char8_t*)alloc->malloc(capacity + 1, alignof(char8_t));
			if (!m_data) {
				return false;
			}
			m_capacity = capacity;

			return append(alloc, str, len);
		}

		template<usize N>
		bool from_utf16(NotNull<const Allocator*> alloc, const char16_t(&str)[N]) noexcept {
			return from_utf16(alloc, str, N - 1);
		}

		bool from_utf32(NotNull<const Allocator*> alloc, const char32_t* str, usize len) noexcept {
			if (!str) {
				return allocate(alloc, detail::STRING_DEFAULT_CAPACITY);
			}

			usize capacity = len * 4;
			m_data = (char8_t*)alloc->malloc(capacity + 1, alignof(char8_t));
			if (!m_data) {
				return false;
			}
			m_capacity = capacity;

			return append(alloc, str, len);
		}

		template<usize N>
		bool from_utf32(NotNull<const Allocator*> alloc, const char32_t(&str)[N]) noexcept {
			return from_utf32(alloc, str, N - 1);
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

		template<typename T, usize N>
			requires std::is_same_v<T, char8_t> || std::is_same_v<T, char16_t> || std::is_same_v<T, char32_t>
		bool append(NotNull<const Allocator*> alloc, const T(&str)[N]) noexcept {
			return append(alloc, str, N - 1);
		}

		bool append(NotNull<const Allocator*> alloc, const char8_t* text) noexcept {
			return append(alloc, text, strlen((const char*)text));
		}

		bool append(NotNull<const Allocator*> alloc, const char8_t* buffer, usize length) noexcept {
			if (!buffer || length == 0) {
				return false;
			}

			if (!grow(alloc, length + 1)) {
				return false;
			}

			memcpy(m_data + m_length, buffer, length);
			m_length += length;
			m_data[m_length] = u8'\0';

			return true;
		}

		template<typename T>
			requires std::is_same_v<T, char16_t> || std::is_same_v<T, char32_t>
		bool append(NotNull<const Allocator*> alloc, const T* buffer, usize len) {
			if (!buffer || len == 0) {
				return false;
			}

			char8_t* data = (char8_t*)alloc->malloc((len * 4) + 1, alignof(char8_t));
			if (!data) {
				return false;
			}

			usize pos = 0;

			if constexpr (std::is_same_v<T, char16_t>) {
				for (usize i = 0; i < len; ++i) {
					usize bytes_count = 0;

					char16_t c = buffer[i];
					if (detail::utf8::is_high_surrogate(c)) {
						// Check for incomplete surrogate pair
						if (i + 1 >= len) {
							alloc->free(data);
							return false;
						}

						char16_t low = buffer[++i];
						if (!detail::utf8::encode_char(c, low, data + pos, bytes_count)) {
							alloc->free(data);
							return false;
						}
					}
					else {
						if (!detail::utf8::encode_char(c, data + pos, bytes_count)) {
							alloc->free(data);
							return false;
						}
					}
					pos += bytes_count;
				}
			}
			else if constexpr (std::is_same_v<T, char32_t>) {
				for (usize i = 0; i < len; ++i) {
					usize bytes_count = 0;
					if (!detail::utf8::encode_char(buffer[i], data + pos, bytes_count)) {
						alloc->free(data);
						return false;
					}
					pos += bytes_count;
				}
			}

			if (!grow(alloc, pos + 1)) {
				return false;
			}

			memcpy(m_data + m_length, data, pos);
			m_length += pos;
			m_data[m_length] = u8'\0';

			alloc->free(data);
			return true;
		}

		bool append(NotNull<const Allocator*> alloc, char8_t c) noexcept {
			if (!grow(alloc, 2)) {
				return false;
			}

			m_data[m_length] = c;
			m_length++;
			m_data[m_length] = u8'\0';
			return true;
		}

		template<typename T>
			requires std::is_same_v<T, char16_t> || std::is_same_v<T, char32_t>
		bool append(NotNull<const Allocator*> alloc, T cp) noexcept {
			usize byte_len = 0;
			char8_t buf[4];
			if (!detail::utf8::encode_char(cp, buf, byte_len)) {
				return false;
			}

			if (!grow(alloc, byte_len)) {
				return false;
			}

			for (usize i = 0; i < byte_len; ++i) {
				m_data[m_length + i] = buf[i];
			}

			m_length += byte_len;
			m_data[m_length] = u8'\0';

			return true;
		}

		bool append(NotNull<const Allocator*> alloc, char16_t cp_high, char16_t cp_low) noexcept {
			usize byte_len = 0;
			char8_t buf[4];
			if (!detail::utf8::encode_char(cp_high, cp_low, buf, byte_len)) {
				return false;
			}

			if (!grow(alloc, byte_len)) {
				return false;
			}

			for (usize i = 0; i < byte_len; ++i) {
				m_data[m_length + i] = buf[i];
			}

			m_length += byte_len;
			m_data[m_length] = u8'\0';

			return true;
		}

		// TODO: Not needed for now, but in future may be having insert for other encodings will be cool to haves
		bool insert(NotNull<const Allocator*> alloc, usize pos, const char8_t* text) {
			if (!text || pos > m_length) {
				return false;
			}

			usize text_len = strlen((const char*)text);
			if (text_len == 0) {
				return true;
			}

			if (!grow(alloc, text_len + 1)) {
				return false;
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
			return dest.from_utf8(alloc, m_data, m_length);
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
		bool allocate(NotNull<const Allocator*> alloc, usize capacity) noexcept {
			if (capacity < detail::STRING_DEFAULT_CAPACITY) {
				capacity = detail::STRING_DEFAULT_CAPACITY;
			}

			m_data = alloc->allocate_array<char8_t>(capacity);
			if (!m_data) {
				return false;
			}

			m_data[0] = u8'\0';
			m_length = 0;
			m_capacity = capacity;

			return true;
		}

		bool grow(NotNull<const Allocator*> alloc, usize size) noexcept {
			usize required = m_length + size;
			if (required > m_capacity) {
				usize new_capacity = m_capacity * 2;
				while (new_capacity < required) {
					new_capacity *= 2;
				}
				if (!reserve(alloc, new_capacity)) {
					return false;
				}
			}
			return true;
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