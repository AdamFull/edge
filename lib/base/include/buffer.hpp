#ifndef EDGE_BUFFER_H
#define EDGE_BUFFER_H

#include "allocator.hpp"

namespace edge {
	template<TrivialType T>
	struct HeapStorage {
		T* m_data = nullptr;
		usize m_capacity = 0;

		bool create(NotNull<const Allocator*> alloc, usize capacity) noexcept {
			if (capacity > 0) {
				m_data = alloc->allocate_array<T>(capacity);
			}
			m_capacity = capacity;
		}

		void destroy(NotNull<const Allocator*> alloc) noexcept {
			if (m_data) {
				alloc->deallocate_array(m_data, m_capacity);
			}
		}

		T* data() noexcept { return m_data; }
		const T* data() const noexcept { return m_data; }
		usize capacity() const noexcept { return m_capacity; }
	};

	template<TrivialType T, usize N>
	struct StackStorage {
		static_assert(N > 0, "StackStorage size must be greater than 0");

		T m_data[N];

		T* data() noexcept { return m_data; }
		const T* data() const noexcept { return m_data; }
		constexpr usize capacity() const noexcept { return N; }
	};

	template<TrivialType T>
	struct ExternalStorage {
		T* m_data = nullptr;
		usize m_capacity = 0;

		T* data() noexcept { return m_data; }
		const T* data() const noexcept { return m_data; }
		usize capacity() const noexcept { return m_capacity; }

		void attach(T* data, usize capacity) {
			m_data = data;
			m_capacity = capacity;
		}

		void detach() {
			m_data = nullptr;
			m_capacity = 0;
		}
	};

	template<TrivialType T, typename StorageProvider>
	struct Buffer {
		StorageProvider m_storage;
		usize m_size = 0;

		usize size() const noexcept {
			return m_size;
		}

		usize capacity() const noexcept {
			return m_storage.capacity();
		}

		bool empty() const noexcept {
			return m_size == 0;
		}

		T* data() noexcept {
			return m_storage.data();
		}

		const T* data() const noexcept {
			return m_storage.data();
		}

		void clear() {
			for (usize i = 0; i < m_size; ++i) {
				m_storage.data()[i].~T();
			}
			m_size = 0;
		}

		T& operator[](usize index) {
			assert(index < m_size);
			return m_storage.data()[index];
		}

		const T& operator[](usize index) const {
			assert(index < m_size);
			return m_storage.data()[index];
		}

		T* front() {
			if (m_size == 0) {
				return nullptr;
			}
			return &m_storage.data()[0];
		}

		const T* front() const {
			if (m_size == 0) {
				return nullptr;
			}
			return &m_storage.data()[0];
		}

		T* back() {
			if (m_size == 0) {
				return nullptr;
			}
			return &m_storage.data()[m_size - 1];
		}

		const T* back() const {
			if (m_size == 0) {
				return nullptr;
			}
			return &m_storage.data()[m_size - 1];
		}

		bool push_back(const T element) {
			if (m_size >= m_storage.capacity()) {
				return false;
			}
			m_storage.data()[m_size++] = element;
			return true;
		}

		bool pop_back(T* out_element = nullptr) {
			if (m_size == 0) {
				return false;
			}

			--m_size;
			if (out_element) {
				*out_element = m_storage.data()[m_size];
			}
			m_storage.data()[m_size].~T();

			return true;
		}

		bool insert(usize index, const T element) {
			if (index > m_size || m_size >= m_storage.capacity()) {
				return false;
			}

			if (index < m_size) {
				memmove(&m_storage.data()[index + 1], &m_storage.data()[index],
					sizeof(T) * (m_size - index));
			}

			m_storage.data()[index] = element;
			m_size++;
			return true;
		}

		bool remove(usize index, T* out_element = nullptr) {
			if (index >= m_size) {
				return false;
			}

			if (out_element) {
				*out_element = m_storage.data()[index];
			}

			if (index < m_size - 1) {
				memmove(&m_storage.data()[index], &m_storage.data()[index + 1],
					sizeof(T) * (m_size - index - 1));
			}

			m_size--;
			m_storage.data()[m_size].~T();

			return true;
		}
	};

	template<TrivialType T, typename StorageProvider>
	inline T* begin(Buffer<T, StorageProvider>& buf) {
		return buf.data();
	}

	template<TrivialType T, typename StorageProvider>
	inline T* end(Buffer<T, StorageProvider>& buf) {
		return buf.data() + buf.size();
	}

	template<TrivialType T, typename StorageProvider>
	inline const T* begin(const Buffer<T, StorageProvider>& buf) {
		return buf.data();
	}

	template<TrivialType T, typename StorageProvider>
	inline const T* end(const Buffer<T, StorageProvider>& buf) {
		return buf.data() + buf.size();
	}
}
#endif