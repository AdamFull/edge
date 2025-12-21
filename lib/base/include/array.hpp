#ifndef EDGE_ARRAY_H
#define EDGE_ARRAY_H

#include "allocator.hpp"

#include <cstring>
#include <cstdlib>
#include <type_traits>

namespace edge {
	template<TrivialType T>
	struct Array {
		void destroy(NotNull<const Allocator*> alloc) {
			if (m_data) {
				alloc->deallocate_array(m_data, m_capacity);
			}
		}

		void clear() {
			for (usize i = 0; i < m_size; ++i) {
				m_data[i].~T();
			}
			m_size = 0;
		}

		bool reserve(NotNull<const Allocator*> alloc, usize capacity) {
			if (capacity == 0) {
				capacity = 16;
			}

			if (capacity <= m_capacity) {
				return true;
			}

			T* new_data = alloc->allocate_array<T>(capacity);
			if (!new_data) {
				return false;
			}

			if (m_data) {
				memcpy(new_data, m_data, sizeof(T) * m_size);
				alloc->deallocate_array(m_data, m_capacity);
			}

			m_data = new_data;
			m_capacity = capacity;

			return true;
		}

		bool resize(NotNull<const Allocator*> alloc, usize new_size) {
			if (new_size > m_capacity) {
				usize new_capacity = m_capacity;
				while (new_capacity < new_size) {
					new_capacity *= 2;
				}
				if (!reserve(alloc, new_capacity)) {
					return false;
				}
			}

			if (new_size > m_size) {
				memset(&m_data[m_size], 0, sizeof(T) * (new_size - m_size));
			}
			else if (new_size < m_size) {
				for (usize i = new_size; i < m_size; ++i) {
					m_data[i].~T();
				}
			}

			m_size = new_size;
			return true;
		}

		usize size() const noexcept {
			return m_size;
		}

		usize capacity() const noexcept {
			return m_capacity;
		}

		bool empty() const noexcept {
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

		T* back() {
			if (m_size == 0) {
				return nullptr;
			}
			return &m_data[m_size - 1];
		}

		bool push_back(NotNull<const Allocator*> alloc, const T element) {
			if (m_size >= m_capacity) {
				usize new_capacity = m_capacity * 2;
				if (!reserve(alloc, new_capacity)) {
					return false;
				}
			}

			m_data[m_size++] = element;
			return true;
		}

		bool pop_back(T* out_element) {
			if (m_size == 0) {
				return false;
			}

			if (out_element) {
				*out_element = m_data[--m_size];
			}

			return true;
		}

		bool insert(NotNull<const Allocator*> alloc, usize index, const T element) {
			if (index > m_size) {
				return false;
			}

			if (m_size >= m_capacity) {
				if (!reserve(alloc, m_capacity * 2)) {
					return false;
				}
			}

			if (index < m_size) {
				memmove(&m_data[index + 1], &m_data[index], sizeof(T) * (m_size - index));
			}

			m_data[index] = element;
			m_size++;
			return true;
		}

		bool remove(usize index, T* out_element) {
			if (index >= m_size) {
				return false;
			}

			if (out_element) {
				*out_element = m_data[index];
			}

			if (index < m_size - 1) {
				memmove(&m_data[index], &m_data[index + 1], sizeof(T) * (m_size - index - 1));
			}

			m_size--;

			return true;
		}

		T* data() noexcept {
			return m_data;
		}

		const T* data() const noexcept {
			return m_data;
		}

	private:
		T* m_data = nullptr;
		usize m_size = 0;
		usize m_capacity = 0;
	};

	template<TrivialType T>
	inline T* begin(Array<T>& arr) {
		return arr.data();
	}

	template<TrivialType T>
	inline T* end(Array<T>& arr) {
		return arr.data() + arr.size();
	}

	template<TrivialType T>
	inline const T* begin(const Array<T>& arr) {
		return arr.data();
	}

	template<TrivialType T>
	inline const T* end(const Array<T>& arr) {
		return arr.data() + arr.size();
	}
}

#endif