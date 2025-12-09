#include "arena.hpp"
#include "math.hpp"

namespace edge {
	namespace detail {
		inline void* ptr_add(void* p, usize bytes) {
			return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(p) + bytes);
		}

		inline bool arena_ensure_committed_locked(Arena* arena, usize required_bytes) {
			if (required_bytes <= arena->m_committed) {
				return true;
			}

			usize max_size = arena->m_reserved;
			if (required_bytes > max_size) {
				return false;
			}

			usize need = required_bytes - arena->m_committed;
			usize commit_size = align_up(need, ARENA_COMMIT_CHUNK_SIZE);
			if (arena->m_committed + commit_size > max_size) {
				commit_size = max_size - arena->m_committed;
			}

			void* commit_addr = ptr_add(arena->m_base, arena->m_committed);

			if (!vmem_commit(commit_addr, commit_size)) {
				return false;
			}

			arena->m_committed += commit_size;
			return true;
		}
	}

	bool arena_create(const Allocator* alloc, Arena* arena, usize size) {
		if (!alloc || !arena) {
			return false;
		}

		if (size == 0) {
			size = ARENA_MAX_SIZE;
		}

		usize page_size = vmem_page_size();
		size = align_up(size, page_size);

		void* base = nullptr;
		if (!vmem_reserve(&base, size)) {
			return false;
		}

		arena->m_allocator = alloc;
		arena->m_base = base;
		arena->m_reserved = size;
		arena->m_committed = 0;
		arena->m_offset = 0;
		arena->m_page_size = page_size;

		return true;
	}

	void arena_destroy(Arena* arena) {
		if (!arena) {
			return;
		}

		if (arena->m_base) {
			vmem_release(arena->m_base, arena->m_reserved);
			arena->m_base = nullptr;
		}
	}

	bool arena_protect(Arena* arena, void* addr, usize size, VMemProt prot) {
		if (!arena || !arena->m_base) {
			return false;
		}

		uintptr_t base = reinterpret_cast<uintptr_t>(arena->m_base);
		uintptr_t astart = reinterpret_cast<uintptr_t>(addr);
		if (astart < base) {
			return false;
		}
		if (astart + size > base + arena->m_reserved) {
			return false;
		}

		uintptr_t page_mask = ~static_cast<uintptr_t>(arena->m_page_size - 1);
		uintptr_t page_addr = astart & page_mask;
		usize page_off = astart - page_addr;
		usize total = align_up(size + page_off, arena->m_page_size);
		return vmem_protect(reinterpret_cast<void*>(page_addr), total, prot);
	}

	void* arena_alloc_ex(Arena* arena, usize size, usize alignment) {
		if (!arena || !arena->m_base || size == 0) {
			return nullptr;
		}

		if (alignment == 0) {
			alignment = alignof(max_align_t);
		}

		if ((alignment & (alignment - 1)) != 0) {
			return nullptr;
		}

		// TODO: Update after threads port
		//std::lock_guard<std::recursive_mutex> lock(arena->m_mutex);

		usize offset = arena->m_offset;
		usize aligned_offset = align_up(offset, alignment);

		if (aligned_offset > SIZE_MAX - size) {
			return nullptr;
		}

		usize max_size = arena->m_reserved;
		usize new_offset = aligned_offset + size;
		if (new_offset > max_size) {
			return nullptr;
		}

		if (!detail::arena_ensure_committed_locked(arena, new_offset)) {
			return nullptr;
		}

		void* result = detail::ptr_add(arena->m_base, aligned_offset);
		arena->m_offset = new_offset;

		return result;
	}
}