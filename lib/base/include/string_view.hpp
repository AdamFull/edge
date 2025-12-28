#ifndef EDGE_STRING_VIEW_H
#define EDGE_STRING_VIEW_H

#include "string.hpp"

namespace edge {
	template<Character CharT, typename Traits = std::char_traits<CharT>>
	struct StringView {
		using value_type = CharT;
		using pointer = const CharT*;
		using const_pointer = const CharT*;
		using reference = const CharT&;
		using const_reference = const CharT&;
		using const_iterator = const CharT*;
		using iterator = const_iterator;
		using size_type = usize;
		using difference_type = isize;

		constexpr StringView() noexcept
			: m_data{ nullptr }
			, m_length{ 0 } {
		}

		constexpr StringView(const StringView&) noexcept = default;
		constexpr StringView& operator=(const StringView&) noexcept = default;

		constexpr StringView(const CharT* str, usize count) noexcept
			: m_data{ str }
			, m_length{ count } {

		}

		constexpr StringView(const CharT* str) noexcept
			: m_data{ str }
			, m_length{ str ? Traits::length(str) : 0 } {

		}

		template<usize N>
		constexpr StringView(const CharT(&str)[N]) noexcept
			: m_data{ str }
			, m_length{ N - 1 } {
		}

		constexpr const_pointer data() const noexcept {
			return m_data;
		}

		constexpr size_type size() const noexcept {
			return m_length;
		}

		constexpr size_type length() const noexcept {
			return m_length;
		}

		constexpr bool empty() const noexcept {
			return m_length == 0;
		}

		constexpr const_reference operator[](size_type index) const noexcept {
			assert(index < m_length);
			return m_data[index];
		}

		constexpr const_reference front() const noexcept {
			assert(m_length > 0 && "front() called on empty StringView");
			return m_data[0];
		}

		constexpr const_reference back() const noexcept {
			assert(m_length > 0 && "back() called on empty StringView");
			return m_data[m_length - 1];
		}

		constexpr void remove_prefix(size_type count) noexcept {
			assert(m_length >= count && "cannot remove_prefix() larger than StringView size");
			m_data += count;
			m_length -= count;
		}

		constexpr void remove_suffix(size_type count) noexcept {
			assert(m_length >= count && "cannot remove_suffix() larger than StringView size");
			m_length -= count;
		}

		constexpr StringView substr(size_type offset = 0, size_type count = UINT64_MAX) const noexcept {
			assert(offset < m_length && "offset in substr() is too big");

			size_type  actual_count = count;
			if (offset + count > m_length) {
				actual_count = m_length - offset;
			}

			return StringView(m_data + offset, actual_count);
		}

		constexpr i32 compare(StringView other) const noexcept {
			size_type min_len = m_length < other.m_length ? m_length : other.m_length;

			if (m_data == other.m_data && m_length == other.m_length) {
				return 0;
			}

			if (!m_data) {
				return other.m_data ? -1 : 0;
			}

			if (!other.m_data) {
				return 1;
			}

			i32 result = Traits::compare(m_data, other.m_data, min_len);
			if (result != 0) {
				return result;
			}

			if (m_length < other.m_length) {
				return -1;
			}
			if (m_length > other.m_length) {
				return 1;
			}
			return 0;
		}

		constexpr bool starts_with(StringView prefix) const noexcept {
			if (prefix.m_length > m_length) {
				return false;
			}
			return Traits::compare(m_data, prefix.m_data, prefix.m_length) == 0;
		}

		constexpr bool starts_with(CharT c) const noexcept {
			return !empty() && Traits::eq(m_data[0], c);
		}

		constexpr bool ends_with(StringView suffix) const noexcept {
			if (suffix.m_length > m_length) {
				return false;
			}
			return Traits::compare(m_data + (m_length - suffix.m_length), suffix.m_data, suffix.m_length) == 0;
		}

		constexpr bool ends_with(CharT c) const noexcept {
			return !empty() && Traits::eq(m_data[m_length - 1], c);
		}

		constexpr bool contains(StringView needle) const noexcept {
			return find(needle) != SIZE_MAX;
		}

		constexpr bool contains(CharT c) const noexcept {
			return find(c) != SIZE_MAX;
		}

		constexpr size_type find(StringView needle, size_type pos = 0) const noexcept {
			if (needle.m_length == 0) {
				return pos <= m_length ? pos : SIZE_MAX;
			}

			if (needle.m_length > m_length || pos > m_length - needle.m_length) {
				return SIZE_MAX;
			}

			for (size_type i = pos; i <= m_length - needle.m_length; ++i) {
				if (Traits::compare(m_data + i, needle.m_data, needle.m_length) == 0) {
					return i;
				}
			}

			return SIZE_MAX;
		}

		constexpr size_type find(CharT c, size_type pos = 0) const noexcept {
			if (pos >= m_length) {
				return SIZE_MAX;
			}

			const_pointer result = Traits::find(m_data + pos, m_length - pos, c);
			return result ? static_cast<size_type>(result - m_data) : SIZE_MAX;
		}

		constexpr size_type rfind(StringView needle, size_type pos = SIZE_MAX) const noexcept {
			if (needle.m_length == 0) {
				return m_length < pos ? m_length : pos;
			}

			if (needle.m_length > m_length) {
				return SIZE_MAX;
			}

			size_type last_possible = m_length - needle.m_length;
			if (pos > last_possible) {
				pos = last_possible;
			}

			for (size_type i = pos + 1; i > 0; --i) {
				size_type idx = i - 1;
				if (Traits::compare(m_data + idx, needle.m_data, needle.m_length) == 0) {
					return idx;
				}
			}

			return SIZE_MAX;
		}

		constexpr size_type rfind(CharT c, size_type pos = SIZE_MAX) const noexcept {
			if (m_length == 0) {
				return SIZE_MAX;
			}

			if (pos >= m_length) {
				pos = m_length - 1;
			}

			for (size_type i = pos + 1; i > 0; --i) {
				if (Traits::eq(m_data[i - 1], c)) {
					return i - 1;
				}
			}

			return SIZE_MAX;
		}

		constexpr const_iterator begin() const noexcept {
			return m_data;
		}

		constexpr const_iterator end() const noexcept {
			return m_data + m_length;
		}

		const CharT* m_data = nullptr;
		usize m_length = 0;
	};

	template<Character CharT, typename Traits>
	constexpr bool operator==(StringView<CharT, Traits> lhs, StringView<CharT, Traits> rhs) noexcept {
		return lhs.compare(rhs) == 0;
	}

	template<Character CharT, typename Traits>
	struct Hash<StringView<CharT, Traits>> {
		EDGE_FORCE_INLINE usize operator()(const StringView<CharT, Traits>& sv) const noexcept {
#if EDGE_HAS_SSE4_2
			return hash_crc32(sv.data(), sv.length() * sizeof(CharT));
#else
			return hash_fnv1a64(sv.data(), sv.length() * sizeof(CharT));
#endif
		}
	};
}

#endif