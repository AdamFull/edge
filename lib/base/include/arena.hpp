#ifndef EDGE_ARENA_H
#define EDGE_ARENA_H

#include "allocator.hpp"
#include "vmem.hpp"

namespace edge {
	constexpr usize ARENA_MAX_SIZE = 256 * 1024 * 1024;
	constexpr usize ARENA_COMMIT_CHUNK_SIZE = 64 * 1024;

	enum class ArenaGuard {
		None,
		PushFront,
		PushBack
	};

	struct Arena {
		void* m_base = nullptr;
		usize m_reserved = 0ull;
		usize m_committed = 0ull;
		usize m_offset = 0ull;
		usize m_page_size = 0ull;

		bool create(usize size = 0);
		void destroy();

		bool protect(void* addr, usize size, VMemProt prot);
		void* alloc_ex(usize size, usize alignment);
		void* alloc(usize size) {
			return alloc_ex(size, alignof(max_align_t));
		}

		template<typename T>
		T* alloc(usize count = 1) {
			return static_cast<T*>(alloc_ex(sizeof(T) * count, alignof(T)));
		}

		void reset(bool zero_memory = false);
	};
}

#endif