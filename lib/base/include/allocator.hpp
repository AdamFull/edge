#ifndef EDGE_ALLOCATOR_H
#define EDGE_ALLOCATOR_H

#include "stddef.hpp"
#include <atomic>

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
	};

	inline Allocator allocator_create(malloc_fn malloc_pfn, free_fn free_pfn, realloc_fn realloc_pfn, void* user_data) {
		return { malloc_pfn, free_pfn, realloc_pfn, user_data };
	}

	inline Allocator allocator_create_default() {
		return allocator_create(
			[](usize size, void*) { return malloc(size); },
			[](void* ptr, void*) { free(ptr); },
			[](void* ptr, usize size, void*) { return realloc(ptr, size); },
			nullptr);
	}

	inline Allocator allocator_create_tracking() {
		static detail::AllocatorStats stats = {};
		return allocator_create(
			detail::tracked_malloc,
			detail::tracked_free,
			detail::tracked_realloc,
			&stats
		);
	}

	inline usize allocator_get_net(const edge::Allocator* alloc) {
		detail::AllocatorStats* stats = (detail::AllocatorStats*)alloc->user_data;
		if (!stats) {
			return ~0ull;
		}

		return stats->alloc_bytes.load() - stats->free_bytes.load();
	}

	inline void* allocator_malloc(const Allocator* alloc, usize size) {
		if (!alloc || !alloc->m_malloc) return nullptr;
		return alloc->m_malloc(size, alloc->user_data);
	}

	inline void allocator_free(const Allocator* alloc, void* ptr) {
		if (!alloc || !alloc->m_free) return;
		return alloc->m_free(ptr, alloc->user_data);
	}

	inline void* allocator_realloc(const Allocator* alloc, void* ptr, usize size) {
		if (!alloc || !alloc->m_realloc) return nullptr;
		return alloc->m_realloc(ptr, size, alloc->user_data);
	}

	inline void* allocator_calloc(const Allocator* alloc, usize nmemb, usize size) {
		if (!alloc) return nullptr;

		usize total = nmemb * size;
		void* ptr = allocator_malloc(alloc, total);
		if (ptr) {
			memset(ptr, 0, total);
		}
		return ptr;
	}

	inline char* allocator_strdup(const Allocator* alloc, const char* str) {
		if (!alloc || !str) return nullptr;

		usize len = strlen(str);
		char* copy = (char*)allocator_malloc(alloc, len + 1);
		if (copy) {
			memcpy(copy, str, len + 1);
		}
		return copy;
	}

	inline char* allocator_strndup(const Allocator* alloc, const char* str, usize n) {
		if (!alloc || !str) return nullptr;

		usize len = strlen(str);
		if (n < len) len = n;

		char* copy = (char*)allocator_malloc(alloc, len + 1);
		if (copy) {
			memcpy(copy, str, len);
			copy[len] = '\0';
		}
		return copy;
	}

	template<typename T>
	inline T* allocate(const Allocator* alloc, usize count = 1) {
		return (T*)allocator_malloc(alloc, sizeof(T) * count);
	}

	template<typename T>
	inline T* allocate_zeroed(const Allocator* alloc, usize count = 1) {
		return (T*)allocator_calloc(alloc, count, sizeof(T));
	}

	template<typename T>
	inline T* reallocate(const Allocator* alloc, T* ptr, usize count) {
		return (T*)allocator_realloc(alloc, ptr, sizeof(T) * count);
	}

	template<typename T>
	inline void deallocate(const Allocator* alloc, T* ptr) {
		allocator_free(alloc, ptr);
	}
}

#endif