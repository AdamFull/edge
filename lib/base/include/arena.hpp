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
		const Allocator* m_allocator;
		void* m_base;
		usize m_reserved;
		usize m_committed;
		usize m_offset;
		usize m_page_size;
		//std::recursive_mutex m_mutex;
	};

	bool arena_create(const Allocator* alloc, Arena* arena, usize size = 0);
	void arena_destroy(Arena* arena);
	bool arena_protect(Arena* arena, void* addr, usize size, VMemProt prot);
	void* arena_alloc_ex(Arena* arena, usize size, usize alignment);

	inline void* arena_alloc(Arena* arena, usize size) {
		return arena_alloc_ex(arena, size, 0);
	}

	template<typename T>
	inline T* arena_alloc(Arena* arena, usize count = 1) {
		return static_cast<T*>(arena_alloc_ex(arena, sizeof(T) * count, alignof(T)));
	}

	void arena_reset(Arena* arena, bool zero_memory = false);

	inline usize arena_offset(const Arena* arena) {
		return arena ? arena->m_offset : 0;
	}

	inline usize arena_committed(const Arena* arena) {
		return arena ? arena->m_committed : 0;
	}

	inline usize arena_reserved(const Arena* arena) {
		return arena ? arena->m_reserved : 0;
	}

	inline usize arena_available(const Arena* arena) {
		return arena ? (arena->m_reserved - arena->m_offset) : 0;
	}

	inline void* arena_base(const Arena* arena) {
		return arena ? arena->m_base : nullptr;
	}
}

#endif