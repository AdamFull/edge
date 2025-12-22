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
				if (!m_data) {
					return false;
				}
			}
			m_capacity = capacity;
			return true;
		}

		void destroy(NotNull<const Allocator*> alloc) noexcept {
			if (m_data) {
				alloc->deallocate_array(m_data, m_capacity);
			}
		}

		T* data() noexcept { return m_data; }
		const T* data() const noexcept { return m_data; }
		usize capacity() const noexcept { return m_capacity; }

		T& operator[](usize index) {
			assert(index < m_capacity);
			return m_data[index];
		}

		const T& operator[](usize index) const {
			assert(index < m_capacity);
			return m_data[index];
		}
	};

	template<TrivialType T>
	inline T* begin(HeapStorage<T>& storage) {
		return storage.data();
	}

	template<TrivialType T>
	inline T* end(HeapStorage<T>& storage) {
		return storage.data() + storage.capacity();
	}

	template<TrivialType T>
	inline const T* begin(const HeapStorage<T>& storage) {
		return storage.data();
	}

	template<TrivialType T>
	inline const T* end(const HeapStorage<T>& storage) {
		return storage.data() + storage.capacity();
	}


	template<TrivialType T, usize N>
	struct StackStorage {
		static_assert(N > 0, "StackStorage size must be greater than 0");

		T m_data[N];

		T* data() noexcept { return m_data; }
		const T* data() const noexcept { return m_data; }
		constexpr usize capacity() const noexcept { return N; }

		T& operator[](usize index) {
			assert(index < N);
			return m_data[index];
		}

		const T& operator[](usize index) const {
			assert(index < N);
			return m_data[index];
		}
	};

	template<TrivialType T, usize N>
	inline T* begin(StackStorage<T, N>& storage) {
		return storage.data();
	}

	template<TrivialType T, usize N>
	inline T* end(StackStorage<T, N>& storage) {
		return storage.data() + storage.capacity();
	}

	template<TrivialType T, usize N>
	inline const T* begin(const StackStorage<T, N>& storage) {
		return storage.data();
	}

	template<TrivialType T, usize N>
	inline const T* end(const StackStorage<T, N>& storage) {
		return storage.data() + storage.capacity();
	}


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

		T& operator[](usize index) {
			assert(index < m_capacity);
			return m_data[index];
		}

		const T& operator[](usize index) const {
			assert(index < m_capacity);
			return m_data[index];
		}
	};

	template<TrivialType T>
	inline T* begin(ExternalStorage<T>& storage) {
		return storage.data();
	}

	template<TrivialType T>
	inline T* end(ExternalStorage<T>& storage) {
		return storage.data() + storage.capacity();
	}

	template<TrivialType T>
	inline const T* begin(const ExternalStorage<T>& storage) {
		return storage.data();
	}

	template<TrivialType T>
	inline const T* end(const ExternalStorage<T>& storage) {
		return storage.data() + storage.capacity();
	}


	template<TrivialType T, typename StorageProvider>
	struct Buffer {
		StorageProvider m_storage = {};
		usize m_size = 0;

		template<typename S = StorageProvider>
			requires std::is_same_v<S, HeapStorage<T>>
		bool create(NotNull<const Allocator*> alloc, usize capacity) noexcept {
			return m_storage.create(alloc, capacity);
		}

		template<typename S = StorageProvider>
			requires std::is_same_v<S, HeapStorage<T>>
		void destroy(NotNull<const Allocator*> alloc) noexcept {
			m_storage.destroy(alloc);
		}

		template<typename S = StorageProvider>
			requires std::is_same_v<S, ExternalStorage<T>>
		void attach(T* data, usize capacity) noexcept {
			m_storage.attach(data, capacity);
		}

		template<typename S = StorageProvider>
			requires std::is_same_v<S, ExternalStorage<T>>
		void detach() noexcept {
			m_storage.detach();
		}

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

	template<typename T>
	using HeapBuffer = Buffer<T, HeapStorage<T>>;
	template<typename T, usize N>
	using StackBuffer = Buffer<T, StackStorage<T, N>>;
	template<typename T>
	using ExternalBuffer = Buffer<T, ExternalStorage<T>>;

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