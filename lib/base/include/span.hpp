#ifndef EDGE_SPAN_H
#define EDGE_SPAN_H

#include "stddef.hpp"

#include <cstring>
#include <type_traits>

namespace edge {
	template<TrivialType T>
	struct Span {
		constexpr Span() noexcept = default;

		constexpr Span(T* data, usize size) noexcept
			: m_data(data), m_size(size) {
		}

		constexpr Span(T* begin, T* end) noexcept
			: m_data(begin), m_size(static_cast<usize>(end - begin)) {
			assert(end >= begin);
		}

		template<usize N>
		constexpr Span(T(&arr)[N]) noexcept
			: m_data(arr), m_size(N) {
		}

		constexpr usize size() const noexcept {
			return m_size;
		}

		constexpr usize size_bytes() const noexcept {
			return m_size * sizeof(T);
		}

		constexpr bool empty() const noexcept {
			return m_size == 0;
		}

		T& operator[](usize index) {
			assert(index < m_size);
			return m_data[index];
		}

		const T& operator[](usize index) const {
			assert(index < m_size);
			return m_data[index];
		}

		T* front() {
			if (m_size == 0) {
				return nullptr;
			}
			return &m_data[0];
		}

		const T* front() const {
			if (m_size == 0) {
				return nullptr;
			}
			return &m_data[0];
		}

		T* back() {
			if (m_size == 0) {
				return nullptr;
			}
			return &m_data[m_size - 1];
		}

		const T* back() const {
			if (m_size == 0) {
				return nullptr;
			}
			return &m_data[m_size - 1];
		}

		T* data() noexcept {
			return m_data;
		}

		const T* data() const noexcept {
			return m_data;
		}

		T* begin() noexcept {
			return m_data;
		}

		T* end() noexcept {
			return m_data + m_size;
		}

		const T* begin() const noexcept {
			return m_data;
		}

		const T* end() const noexcept {
			return m_data + m_size;
		}

		Span<T> subspan(usize offset, usize count) const {
			if (offset >= m_size) {
				return Span<T>();
			}

			usize actual_count = count;
			if (offset + count > m_size) {
				actual_count = m_size - offset;
			}

			return Span<T>(m_data + offset, actual_count);
		}

		Span<T> subspan(usize offset) const {
			return subspan(offset, m_size);
		}

		Span<T> first(usize count) const {
			if (count > m_size) {
				count = m_size;
			}
			return Span<T>(m_data, count);
		}

		Span<T> last(usize count) const {
			if (count > m_size) {
				count = m_size;
			}
			return Span<T>(m_data + (m_size - count), count);
		}

		void copy_to(T* dest) const {
			if (m_data && dest && m_size > 0) {
				memcpy(dest, m_data, sizeof(T) * m_size);
			}
		}

		void copy_from(const T* src, usize count) {
			if (m_data && src && count > 0) {
				usize actual_count = count > m_size ? m_size : count;
				memcpy(m_data, src, sizeof(T) * actual_count);
			}
		}

	private:
		T* m_data = nullptr;
		usize m_size = 0;
	};
}
#endif