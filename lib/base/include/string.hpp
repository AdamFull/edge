#ifndef EDGE_STRING_H
#define EDGE_STRING_H

#include "allocator.hpp"

#include <cstring>
#include <cstdlib>

namespace edge {
	namespace detail {
		constexpr usize STRING_DEFAULT_CAPACITY = 16;

		template<Character CharT>
		inline usize char_strlen(const CharT* str) {
			if constexpr (std::same_as<CharT, char>) {
				return strlen(str);
			}
			else if constexpr (std::same_as<CharT, wchar_t>) {
				return wcslen(str);
			}
			else {
				usize len = 0;
				while (str[len] != CharT{ 0 }) {
					len++;
				}
				return len;
			}
		}

		template<Character CharT>
		inline i32 char_strcmp(const CharT* s1, const CharT* s2) {
			if constexpr (std::same_as<CharT, char>) {
				return strcmp(s1, s2);
			}
			else if constexpr (std::same_as<CharT, wchar_t>) {
				return wcscmp(s1, s2);
			}
			else {
				while (*s1 && (*s1 == *s2)) {
					s1++;
					s2++;
				}
				return static_cast<i32>(*s1) - static_cast<i32>(*s2);
			}
		}

		template<Character CharT>
		inline const CharT* char_strstr(const CharT* haystack, const CharT* needle) {
			if constexpr (std::same_as<CharT, char>) {
				return strstr(haystack, needle);
			}
			else if constexpr (std::same_as<CharT, wchar_t>) {
				return wcsstr(haystack, needle);
			}
			else {
				if (!*needle) return haystack;
				const CharT* p1 = haystack;
				while (*p1) {
					const CharT* p1_begin = p1;
					const CharT* p2 = needle;
					while (*p1 && *p2 && *p1 == *p2) {
						p1++;
						p2++;
					}
					if (!*p2) return p1_begin;
					p1 = p1_begin + 1;
				}
				return nullptr;
			}
		}
	}

	template<Character CharT>
	struct BasicString {
		CharT* m_data = nullptr;
		usize m_length = 0ull;
		usize m_capacity = 0ull;

		bool create(NotNull<const Allocator*> alloc, usize initial_capacity = 0) {
			if (initial_capacity < detail::STRING_DEFAULT_CAPACITY) {
				initial_capacity = detail::STRING_DEFAULT_CAPACITY;
			}

			m_data = alloc->allocate_array<CharT>(initial_capacity);
			if (!m_data) {
				return false;
			}

			m_data[0] = CharT{ 0 };
			m_length = 0;
			m_capacity = initial_capacity;

			return true;
		}

		bool create_from(NotNull<const Allocator*> alloc, const CharT* cstr) {
			if (!cstr) {
				return create(alloc, detail::STRING_DEFAULT_CAPACITY);
			}

			usize len = detail::char_strlen(cstr);
			if (!create(alloc, len + 1)) {
				return false;
			}

			memcpy(m_data, cstr, (len + 1) * sizeof(CharT));
			m_length = len;

			return true;
		}

		bool create_from_buffer(NotNull<const Allocator*> alloc, const CharT* buffer, usize length) {
			if (!buffer) {
				return create(alloc, detail::STRING_DEFAULT_CAPACITY);
			}

			if (!create(alloc, length + 1)) {
				return false;
			}

			memcpy(m_data, buffer, length * sizeof(CharT));
			m_data[length] = CharT{ 0 };
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
				m_data[0] = CharT{ 0 };
			}
		}

		bool reserve(NotNull<const Allocator*> alloc, usize capacity) {
			if (capacity <= m_capacity) {
				return true;
			}

			CharT* new_data = (CharT*)alloc->realloc(m_data, capacity);
			if (!new_data) {
				return false;
			}

			m_data = new_data;
			m_capacity = capacity;

			return true;
		}

		bool append(NotNull<const Allocator*> alloc, const CharT* text) {
			if (!text) {
				return false;
			}

			usize text_len = detail::char_strlen(text);
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

			memcpy(m_data + m_length, text, (text_len + 1) * sizeof(CharT));
			m_length += text_len;

			return true;
		}

		bool append(NotNull<const Allocator*> alloc, const CharT* buffer, usize length) {
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

			memcpy(m_data + m_length, buffer, length * sizeof(CharT));
			m_length += length;
			m_data[m_length] = CharT{ 0 };

			return true;
		}

		bool append(NotNull<const Allocator*> alloc, CharT c) {
			usize required = m_length + 2;
			if (required > m_capacity) {
				if (!reserve(alloc, m_capacity * 2)) {
					return false;
				}
			}

			m_data[m_length] = c;
			m_length++;
			m_data[m_length] = CharT{ 0 };

			return true;
		}

		bool append(NotNull<const Allocator*> alloc, const BasicString<CharT>* str) {
			if (!str) {
				return false;
			}
			return append(alloc, str->m_data, str->m_length);
		}

		bool insert(NotNull<const Allocator*> alloc, usize pos, const CharT* text) {
			if (!text || pos > m_length) {
				return false;
			}

			usize text_len = detail::char_strlen(text);
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

			memmove(m_data + pos + text_len, m_data + pos, (m_length - pos + 1) * sizeof(CharT));

			memcpy(m_data + pos, text, text_len * sizeof(CharT));
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

			memmove(m_data + pos, m_data + pos + length, (m_length - pos - length + 1) * sizeof(CharT));
			m_length -= length;

			return true;
		}

		bool empty() const noexcept {
			return m_length == 0;
		}

		i32 compare(const CharT* other) {
			if (!m_data) {
				return other ? -1 : 0;
			}

			if (!other) {
				return 1;
			}

			return detail::char_strcmp(m_data, other);
		}

		i32 compare(const BasicString<CharT> str2) {
			if (!m_data) {
				return (str2.m_data) ? -1 : 0;
			}

			if (!str2.m_data) {
				return 1;
			}

			return detail::char_strcmp(m_data, str2.m_data);
		}

		i32 find(const CharT* needle) {
			if (!m_data || !needle) {
				return -1;
			}

			const CharT* found = detail::char_strstr(m_data, needle);
			if (!found) {
				return -1;
			}

			return static_cast<i32>(found - m_data);
		}

		bool duplicate(NotNull<const Allocator*> alloc, BasicString<CharT> dest) {
			return dest.create_from_buffer(alloc, m_data, m_length);
		}
	};

	using String = BasicString<char>;
	using WString = BasicString<wchar_t>;
	using U8String = BasicString<char8_t>;
	using U16String = BasicString<char16_t>;
	using U32String = BasicString<char32_t>;

	template<Character CharT>
	struct BasicStringView {
		const CharT* m_data;
		usize m_length;

		constexpr BasicStringView() : m_data(nullptr), m_length(0) {}

		constexpr BasicStringView(const CharT* str)
			: m_data(str), m_length(str ? detail::char_strlen(str) : 0) {
		}

		constexpr BasicStringView(const CharT* str, usize len)
			: m_data(str), m_length(len) {
		}

		constexpr BasicStringView(const BasicString<CharT>& str)
			: m_data(str.m_data), m_length(str.m_length) {
		}
	};

	using StringView = BasicStringView<char>;
	using WStringView = BasicStringView<wchar_t>;
	using U8StringView = BasicStringView<char8_t>;
	using U16StringView = BasicStringView<char16_t>;
	using U32StringView = BasicStringView<char32_t>;

	template<Character CharT>
	inline CharT* begin(BasicString<CharT>& str) {
		return str.m_data;
	}

	template<Character CharT>
	inline CharT* end(BasicString<CharT>& str) {
		return str.m_data + str.m_length;
	}

	template<Character CharT>
	inline const CharT* begin(const BasicString<CharT>& str) {
		return str.m_data;
	}

	template<Character CharT>
	inline const CharT* end(const BasicString<CharT>& str) {
		return str.m_data + str.m_length;
	}
}

#endif