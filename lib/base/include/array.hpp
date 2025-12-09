#ifndef EDGE_ARRAY_H
#define EDGE_ARRAY_H

#include "allocator.hpp"

#include <cstring>
#include <cstdlib>
#include <type_traits>

namespace edge {
	template<TrivialType T>
	struct Array {
		T* m_data;
		usize m_size;
		usize m_capacity;
		const Allocator* m_allocator;
	};

	template<TrivialType T>
	bool array_create(const Allocator* alloc, Array<T>* arr, usize initial_capacity) {
		if (!alloc || !arr) {
			return false;
		}

		if (initial_capacity == 0) {
			initial_capacity = 16;
		}

		arr->m_data = allocate<T>(alloc, initial_capacity);
		if (!arr->m_data) {
			return false;
		}

		arr->m_size = 0;
		arr->m_capacity = initial_capacity;
		arr->m_allocator = alloc;

		return true;
	}

	template<TrivialType T>
	void array_destroy(Array<T>* arr) {
		if (!arr) {
			return;
		}

		if (arr->m_data) {
			deallocate(arr->m_allocator, arr->m_data);
		}
	}

	template<TrivialType T>
	void array_clear(Array<T>* arr) {
		if (!arr) {
			return;
		}

		arr->m_size = 0;
	}

	template<TrivialType T>
	bool array_reserve(Array<T>* arr, usize capacity) {
		if (!arr) {
			return false;
		}

		if (capacity <= arr->m_capacity) {
			return true;
		}

		T* new_data = allocate<T>(arr->m_allocator, capacity);
		if (!new_data) {
			return false;
		}

		if (arr->m_data) {
			memcpy(new_data, arr->m_data, sizeof(T) * arr->m_size);
			deallocate(arr->m_allocator, arr->m_data);
		}

		arr->m_data = new_data;
		arr->m_capacity = capacity;

		return true;
	}

	template<TrivialType T>
	bool array_resize(Array<T>* arr, usize new_size) {
		if (!arr) {
			return false;
		}

		if (new_size > arr->m_capacity) {
			usize new_capacity = arr->m_capacity;
			while (new_capacity < new_size) {
				new_capacity *= 2;
			}
			if (!array_reserve(arr, new_capacity)) {
				return false;
			}
		}

		if (new_size > arr->m_size) {
			memset(&arr->m_data[arr->m_size], 0, sizeof(T) * (new_size - arr->m_size));
		}

		arr->m_size = new_size;
		return true;
	}

	template<TrivialType T>
	usize array_size(const Array<T>* arr) {
		return arr ? arr->m_size : 0;
	}

	template<TrivialType T>
	usize array_capacity(const Array<T>* arr) {
		return arr ? arr->m_capacity : 0;
	}

	template<TrivialType T>
	bool array_empty(const Array<T>* arr) {
		return !arr || arr->m_size == 0;
	}

	template<TrivialType T>
	T* array_at(const Array<T>* arr, usize index) {
		if (!arr) {
			return nullptr;
		}
		return &arr->m_data[index];
	}

	template<TrivialType T>
	T* array_get(const Array<T>* arr, usize index) {
		if (!arr || index >= arr->m_size) {
			return nullptr;
		}
		return &arr->m_data[index];
	}

	template<TrivialType T>
	bool array_set(Array<T>* arr, usize index, const T& element) {
		if (!arr || index >= arr->m_size) {
			return false;
		}
		arr->m_data[index] = element;
		return true;
	}

	template<TrivialType T>
	T* array_front(const Array<T>* arr) {
		if (!arr || arr->m_size == 0) {
			return nullptr;
		}
		return &arr->m_data[0];
	}

	template<TrivialType T>
	T* array_back(const Array<T>* arr) {
		if (!arr || arr->m_size == 0) {
			return nullptr;
		}
		return &arr->m_data[arr->m_size - 1];
	}

	template<TrivialType T>
	T* array_data(const Array<T>* arr) {
		return arr ? arr->m_data : nullptr;
	}

	template<TrivialType T>
	bool array_push_back(Array<T>* arr, const T& element) {
		if (!arr) {
			return false;
		}

		if (arr->m_size >= arr->m_capacity) {
			usize new_capacity = arr->m_capacity * 2;
			if (!array_reserve(arr, new_capacity)) {
				return false;
			}
		}

		memcpy(&arr->m_data[arr->m_size], &element, sizeof(T));
		arr->m_size++;

		return true;
	}

	template<TrivialType T>
	bool array_pop_back(Array<T>* arr, T* out_element) {
		if (!arr || arr->m_size == 0) {
			return false;
		}

		arr->m_size--;
		if (out_element) {
			memcpy(out_element, &arr->m_data[arr->m_size], sizeof(T));
		}

		return true;
	}

	template<TrivialType T>
	bool array_insert(Array<T>* arr, usize index, const T& element) {
		if (!arr || index > arr->m_size) {
			return false;
		}

		if (arr->m_size >= arr->m_capacity) {
			if (!array_reserve(arr, arr->m_capacity * 2)) {
				return false;
			}
		}

		if (index < arr->m_size) {
			memmove(&arr->m_data[index + 1], &arr->m_data[index], sizeof(T) * (arr->m_size - index));
		}

		memcpy(&arr->m_data[index], &element, sizeof(T));
		arr->m_size++;
		return true;
	}

	template<TrivialType T>
	bool array_remove(Array<T>* arr, usize index, T* out_element) {
		if (!arr || index >= arr->m_size) {
			return false;
		}

		if (out_element) {
			memcpy(out_element, &arr->m_data[index], sizeof(T));
		}

		if (index < arr->m_size - 1) {
			memmove(&arr->m_data[index], &arr->m_data[index + 1], sizeof(T) * (arr->m_size - index - 1));
		}

		arr->m_size--;

		return true;
	}

	template<TrivialType T>
	inline T* begin(Array<T>& arr) {
		return arr.m_data;
	}

	template<TrivialType T>
	inline T* end(Array<T>& arr) {
		return arr.m_data + arr.m_size;
	}

	template<TrivialType T>
	inline const T* begin(const Array<T>& arr) {
		return arr.m_data;
	}

	template<TrivialType T>
	inline const T* end(const Array<T>& arr) {
		return arr.m_data + arr.m_size;
	}
}

#endif