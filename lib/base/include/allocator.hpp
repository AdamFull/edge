#ifndef EDGE_ALLOCATOR_H
#define EDGE_ALLOCATOR_H

#include "stddef.hpp"
#include <atomic>
#include <new>

namespace edge {
	namespace detail {
		struct AllocatorStats {
			std::atomic_size_t alloc_bytes;
			std::atomic_size_t free_bytes;
		};

		struct AllocationHeader {
			usize size;
		};

		inline void* tracked_malloc(usize size, void* user_data) {
			if (size == 0) {
				return nullptr;
			}

			usize total_size = sizeof(AllocationHeader) + size;
			AllocationHeader* header = (AllocationHeader*)malloc(total_size);
			if (!header) {
				return nullptr;
			}

			header->size = size;

			AllocatorStats* stats = (AllocatorStats*)user_data;
			stats->alloc_bytes.fetch_add(size);

			return (void*)(header + 1);
		}

		inline void tracked_free(void* ptr, void* user_data) {
			if (!ptr) {
				return;
			}

			AllocationHeader* header = ((AllocationHeader*)ptr) - 1;

			AllocatorStats* stats = (AllocatorStats*)user_data;
			stats->free_bytes.fetch_add(header->size);

			free(header);
		}

		inline void* tracked_realloc(void* ptr, usize size, void* user_data) {
			if (!ptr) {
				return tracked_malloc(size, user_data);
			}

			if (size == 0) {
				tracked_free(ptr, user_data);
				return nullptr;
			}

			AllocationHeader* old_header = ((AllocationHeader*)ptr) - 1;
			usize old_size = old_header->size;

			usize total_size = sizeof(AllocationHeader) + size;
			AllocationHeader* new_header = (AllocationHeader*)realloc(old_header, total_size);
			if (new_header == NULL) {
				return NULL;
			}

			AllocatorStats* stats = (AllocatorStats*)user_data;
			stats->free_bytes.fetch_add(old_size);
			stats->alloc_bytes.fetch_add(size);

			new_header->size = size;
			return (void*)(new_header + 1);
		}
	}

	using malloc_fn = void* (*)(usize size, void* user_data);
	using free_fn = void (*)(void* ptr, void* user_data);
	using realloc_fn = void* (*)(void* ptr, usize size, void* user_data);

	struct Allocator {
		malloc_fn m_malloc;
		free_fn m_free;
		realloc_fn m_realloc;
		void* user_data;

		static Allocator create(malloc_fn malloc_pfn, free_fn free_pfn, realloc_fn realloc_pfn, void* user_data) {
			return { malloc_pfn, free_pfn, realloc_pfn, user_data };
		}

		static Allocator create_default() {
			return create(
				[](usize size, void*) { return ::malloc(size); },
				[](void* ptr, void*) { ::free(ptr); },
				[](void* ptr, usize size, void*) { return ::realloc(ptr, size); },
				nullptr);
		}

		static Allocator create_tracking() {
			static detail::AllocatorStats stats = {};
			return create(
				detail::tracked_malloc,
				detail::tracked_free,
				detail::tracked_realloc,
				&stats
			);
		}

		usize get_net() const noexcept {
			detail::AllocatorStats* stats = (detail::AllocatorStats*)user_data;
			if (!stats) {
				return ~0ull;
			}

			return stats->alloc_bytes.load() - stats->free_bytes.load();
		}

		void* malloc(usize size) const noexcept {
			if (!m_malloc) {
				return nullptr;
			}
			return m_malloc(size, user_data);
		}

		void free(void* ptr) const noexcept {
			if (!m_free) {
				return;
			}
			return m_free(ptr, user_data);
		}

		void* realloc(void* ptr, usize size) const noexcept {
			if (!m_realloc) {
				return nullptr;
			}
			return m_realloc(ptr, size, user_data);
		}

		void* zeroed(usize nmemb, usize size) const noexcept {
			usize total = nmemb * size;
			void* ptr = malloc(total);
			if (ptr) {
				memset(ptr, 0, total);
			}
			return ptr;
		}

		char* strdup(const char* str) const noexcept {
			if (!str) {
				return nullptr;
			}

			usize len = strlen(str);
			char* copy = (char*)malloc(len + 1);
			if (copy) {
				memcpy(copy, str, len + 1);
			}
			return copy;
		}

		char* strndup(const char* str, usize n) const noexcept {
			if (!str) {
				return nullptr;
			}

			usize len = strlen(str);
			if (n < len) len = n;

			char* copy = (char*)malloc(len + 1);
			if (copy) {
				memcpy(copy, str, len);
				copy[len] = '\0';
			}
			return copy;
		}

		template<typename T, typename... Args>
		T* allocate(Args&&... args) const noexcept {
			T* ptr = (T*)malloc(sizeof(T));
			if (!ptr) {
				return nullptr;
			}

			//if constexpr (!std::is_trivially_constructible_v<T, Args...>) {
				new (ptr) T(std::forward<Args>(args)...);
			//}

			return ptr;
		}

		template<typename T>
		T* allocate_zeroed(usize count = 1) const noexcept {
			return (T*)calloc(count, sizeof(T));
		}

		template<typename T>
		T* allocate_array(usize count) const noexcept {
			if (count == 0) {
				return nullptr;
			}

			T* ptr = (T*)malloc(sizeof(T) * count);
			if (!ptr) {
				return nullptr;
			}

			if constexpr (!std::is_trivially_constructible_v<T>) {
				for (usize i = 0; i < count; ++i) {
					new (&ptr[i]) T();
				}
			}

			return ptr;
		}

		template<typename T>
		void deallocate(T* ptr) const noexcept {
			if (!ptr) {
				return;
			}

			if constexpr (!std::is_trivially_destructible_v<T>) {
				ptr->~T();
			}

			free(ptr);
		}

		template<typename T>
		void deallocate_array(T* ptr, usize count) const noexcept {
			if (!ptr) {
				return;
			}

			if constexpr (!std::is_trivially_destructible_v<T>) {
				for (usize i = count; i > 0; --i) {
					ptr[i - 1].~T();
				}
			}

			free(ptr);
		}
	};
}

#endif