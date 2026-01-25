#ifndef EDGE_SPAN_H
#define EDGE_SPAN_H

#include "stddef.hpp"

#include <cstring>
#include <type_traits>

namespace edge {
	template<typename T>
	struct Array;

	template<TrivialType T, typename StorageProvider>
	struct Buffer;

	template<TrivialType T>
	struct Span {
		constexpr Span() = default;

		constexpr Span(T* data, usize size)
			: m_data(data), m_size(size) {
		}

		constexpr Span(T* begin, T* end)
			: m_data(begin), m_size(static_cast<usize>(end - begin)) {
			assert(end >= begin);
		}

		template<usize N>
		constexpr Span(T(&arr)[N])
			: m_data(arr), m_size(N) {
		}

		constexpr Span(Array<T>& arr) noexcept
			: m_data(arr.data()), m_size(arr.size()) {
		}

		constexpr Span(const Array<T>& arr) noexcept requires std::is_const_v<T>
			: m_data(arr.data()), m_size(arr.size()) {
		}

		template<typename StorageProvider>
		constexpr Span(Buffer<T, StorageProvider>& arr) noexcept
			: m_data(arr.data()), m_size(arr.size()) {
		}

		template<typename StorageProvider>
		constexpr Span(const Buffer<T, StorageProvider>& arr) noexcept requires std::is_const_v<T>
			: m_data(arr.data()), m_size(arr.size()) {
		}

		constexpr usize size() const {
			return m_size;
		}

		constexpr usize size_bytes() const {
			return m_size * sizeof(T);
		}

		constexpr bool empty() const {
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

		T* data() {
			return m_data;
		}

		const T* data() const {
			return m_data;
		}

		T* begin() {
			return m_data;
		}

		T* end() {
			return m_data + m_size;
		}

		const T* begin() const {
			return m_data;
		}

		const T* end() const {
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