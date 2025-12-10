#ifndef EDGE_STRING_H
#define EDGE_STRING_H

#include "allocator.hpp"

#include <cstring>
#include <cstdlib>

namespace edge {
	template<Character CharT>
	struct BasicString {
		CharT* m_data;
		usize m_length;
		usize m_capacity;
		const Allocator* m_allocator;
	};

	using String = BasicString<char>;
	using WString = BasicString<wchar_t>;
	using U8String = BasicString<char8_t>;
	using U16String = BasicString<char16_t>;
	using U32String = BasicString<char32_t>;

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
				// Simple implementation for other character types
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
	inline bool string_create(const Allocator* alloc, BasicString<CharT>* str, usize initial_capacity = 0) {
		if (!alloc || !str) {
			return false;
		}

		if (initial_capacity < detail::STRING_DEFAULT_CAPACITY) {
			initial_capacity = detail::STRING_DEFAULT_CAPACITY;
		}

		str->m_data = allocate<CharT>(alloc, initial_capacity);
		if (!str->m_data) {
			return false;
		}

		str->m_data[0] = CharT{ 0 };
		str->m_length = 0;
		str->m_capacity = initial_capacity;
		str->m_allocator = alloc;

		return true;
	}

	template<Character CharT>
	inline bool string_create_from(const Allocator* alloc, BasicString<CharT>* str, const CharT* cstr) {
		if (!cstr) {
			return string_create(alloc, str, detail::STRING_DEFAULT_CAPACITY);
		}

		usize len = detail::char_strlen(cstr);
		if (!string_create(alloc, str, len + 1)) {
			return false;
		}

		memcpy(str->m_data, cstr, (len + 1) * sizeof(CharT));
		str->m_length = len;

		return true;
	}

	template<Character CharT>
	inline bool string_create_from_buffer(const Allocator* alloc, BasicString<CharT>* str, const CharT* buffer, usize length) {
		if (!buffer) {
			return string_create(alloc, str, detail::STRING_DEFAULT_CAPACITY);
		}

		if (!string_create(alloc, str, length + 1)) {
			return false;
		}

		memcpy(str->m_data, buffer, length * sizeof(CharT));
		str->m_data[length] = CharT{ 0 };
		str->m_length = length;

		return true;
	}

	template<Character CharT>
	inline void string_destroy(BasicString<CharT>* str) {
		if (!str) {
			return;
		}

		if (str->m_data) {
			deallocate(str->m_allocator, str->m_data);
		}
	}

	template<Character CharT>
	inline void string_clear(BasicString<CharT>* str) {
		if (!str) {
			return;
		}

		str->m_length = 0;
		if (str->m_data) {
			str->m_data[0] = CharT{ 0 };
		}
	}

	template<Character CharT>
	inline bool string_reserve(BasicString<CharT>* str, usize capacity) {
		if (!str) {
			return false;
		}

		if (capacity <= str->m_capacity) {
			return true;
		}

		CharT* new_data = reallocate(str->m_allocator, str->m_data, capacity);
		if (!new_data) {
			return false;
		}

		str->m_data = new_data;
		str->m_capacity = capacity;

		return true;
	}

	template<Character CharT>
	inline bool string_append(BasicString<CharT>* str, const CharT* text) {
		if (!str || !text) {
			return false;
		}

		usize text_len = detail::char_strlen(text);
		if (text_len == 0) {
			return true;
		}

		usize required = str->m_length + text_len + 1;
		if (required > str->m_capacity) {
			usize new_capacity = str->m_capacity * 2;
			while (new_capacity < required) {
				new_capacity *= 2;
			}
			if (!string_reserve(str, new_capacity)) {
				return false;
			}
		}

		memcpy(str->m_data + str->m_length, text, (text_len + 1) * sizeof(CharT));
		str->m_length += text_len;

		return true;
	}

	template<Character CharT>
	inline bool string_append_buffer(BasicString<CharT>* str, const CharT* buffer, usize length) {
		if (!str || !buffer || length == 0) {
			return false;
		}

		usize required = str->m_length + length + 1;
		if (required > str->m_capacity) {
			usize new_capacity = str->m_capacity * 2;
			while (new_capacity < required) {
				new_capacity *= 2;
			}
			if (!string_reserve(str, new_capacity)) {
				return false;
			}
		}

		memcpy(str->m_data + str->m_length, buffer, length * sizeof(CharT));
		str->m_length += length;
		str->m_data[str->m_length] = CharT{ 0 };

		return true;
	}

	template<Character CharT>
	inline bool string_append_char(BasicString<CharT>* str, CharT c) {
		if (!str) {
			return false;
		}

		usize required = str->m_length + 2;
		if (required > str->m_capacity) {
			if (!string_reserve(str, str->m_capacity * 2)) {
				return false;
			}
		}

		str->m_data[str->m_length] = c;
		str->m_length++;
		str->m_data[str->m_length] = CharT{ 0 };

		return true;
	}

	template<Character CharT>
	inline bool string_append_string(BasicString<CharT>* dest, const BasicString<CharT>* src) {
		if (!dest || !src) {
			return false;
		}
		return string_append_buffer(dest, src->m_data, src->m_length);
	}

	template<Character CharT>
	inline bool string_insert(BasicString<CharT>* str, usize pos, const CharT* text) {
		if (!str || !text || pos > str->m_length) {
			return false;
		}

		usize text_len = detail::char_strlen(text);
		if (text_len == 0) {
			return true;
		}

		usize required = str->m_length + text_len + 1;
		if (required > str->m_capacity) {
			usize new_capacity = str->m_capacity * 2;
			while (new_capacity < required) {
				new_capacity *= 2;
			}
			if (!string_reserve(str, new_capacity)) {
				return false;
			}
		}

		// Move existing data to make room
		memmove(str->m_data + pos + text_len, str->m_data + pos, (str->m_length - pos + 1) * sizeof(CharT));

		// Insert new text
		memcpy(str->m_data + pos, text, text_len * sizeof(CharT));
		str->m_length += text_len;

		return true;
	}

	template<Character CharT>
	inline bool string_remove(BasicString<CharT>* str, usize pos, usize length) {
		if (!str || pos >= str->m_length) {
			return false;
		}

		if (pos + length > str->m_length) {
			length = str->m_length - pos;
		}

		memmove(str->m_data + pos, str->m_data + pos + length, (str->m_length - pos - length + 1) * sizeof(CharT));
		str->m_length -= length;

		return true;
	}

	template<Character CharT>
	inline const CharT* string_cstr(const BasicString<CharT>* str) {
		return str ? str->m_data : nullptr;
	}

	template<Character CharT>
	inline usize string_length(const BasicString<CharT>* str) {
		return str ? str->m_length : 0;
	}

	template<Character CharT>
	inline usize string_capacity(const BasicString<CharT>* str) {
		return str ? str->m_capacity : 0;
	}

	template<Character CharT>
	inline bool string_empty(const BasicString<CharT>* str) {
		return !str || str->m_length == 0;
	}

	template<Character CharT>
	inline i32 string_compare(const BasicString<CharT>* str, const CharT* other) {
		if (!str || !str->m_data) {
			return other ? -1 : 0;
		}

		if (!other) {
			return 1;
		}

		return detail::char_strcmp(str->m_data, other);
	}

	template<Character CharT>
	inline i32 string_compare_string(const BasicString<CharT>* str1, const BasicString<CharT>* str2) {
		if (!str1 || !str1->m_data) {
			return (str2 && str2->m_data) ? -1 : 0;
		}

		if (!str2 || !str2->m_data) {
			return 1;
		}

		return detail::char_strcmp(str1->m_data, str2->m_data);
	}

	template<Character CharT>
	inline i32 string_find(const BasicString<CharT>* str, const CharT* needle) {
		if (!str || !str->m_data || !needle) {
			return -1;
		}

		const CharT* found = detail::char_strstr(str->m_data, needle);
		if (!found) {
			return -1;
		}

		return static_cast<i32>(found - str->m_data);
	}

	template<Character CharT>
	inline bool string_shrink_to_fit(BasicString<CharT>* str) {
		if (!str) {
			return false;
		}

		usize required = str->m_length + 1;
		if (required >= str->m_capacity) {
			return true;
		}

		CharT* new_data = reallocate(str->m_allocator, str->m_data, required);
		if (!new_data) {
			return false;
		}

		str->m_data = new_data;
		str->m_capacity = required;

		return true;
	}

	template<Character CharT>
	inline bool string_duplicate(const BasicString<CharT>* src, BasicString<CharT>* dest) {
		if (!src) {
			return false;
		}
		return string_create_from_buffer(src->m_allocator, dest, src->m_data, src->m_length);
	}

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