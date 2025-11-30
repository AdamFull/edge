#include "edge_arena.h"
#include "edge_threads.h"
#include "edge_allocator.h"
#include "edge_math.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct edge_arena {
	struct edge_allocator* allocator;

	void* base;
	size_t reserved;
	size_t committed;
	size_t offset;
	size_t page_size;

	edge_mtx_t mtx;
};

static void* ptr_add(void* p, size_t bytes) {
	return (void*)((uintptr_t)p + bytes);
}

edge_arena_t* edge_arena_create(edge_allocator_t* allocator, size_t size) {
	if (!allocator) {
		return NULL;
	}

	if (size == 0) {
		size = EDGE_ARENA_MAX_SIZE;
	}

	size_t page_size = edge_vmem_page_size();
	size = em_align_up(size, page_size);

	void* base = NULL;
	if (!edge_vmem_reserve(&base, size)) {
		return NULL;
	}

	edge_arena_t* arena = (edge_arena_t*)edge_allocator_malloc(allocator, sizeof(edge_arena_t));
	if (!arena) {
		edge_vmem_release(base, size);
		return NULL;
	}

	arena->allocator = allocator;
	arena->base = base;
	arena->reserved = size;
	arena->committed = 0;
	arena->offset = 0;
	arena->page_size = page_size;

	edge_mtx_init(&arena->mtx, edge_mtx_recursive);

	return arena;
}

bool edge_arena_protect(edge_arena_t* arena, void* addr, size_t size, edge_vmem_prot_t prot) {
	if (!arena || !arena->base) {
		return false;
	}

	uintptr_t base = (uintptr_t)arena->base;
	uintptr_t astart = (uintptr_t)addr;
	if (astart < base) {
		return false;
	}
	if (astart + size > base + arena->reserved) {
		return false;
	}

	uintptr_t page_mask = ~(uintptr_t)(arena->page_size - 1);
	uintptr_t page_addr = astart & page_mask;
	size_t page_off = astart - page_addr;
	size_t total = em_align_up(size + page_off, arena->page_size);
	return edge_vmem_protect((void*)page_addr, total, prot);
}

void edge_arena_destroy(edge_arena_t* arena) {
	if (!arena) {
		return;
	}

	if (arena->base) {
		edge_vmem_release(arena->base, arena->reserved);
	}

	edge_mtx_destroy(&arena->mtx);

	edge_allocator_t* alloc = arena->allocator;
	edge_allocator_free(alloc, arena);
}

static bool arena_ensure_committed_locked(edge_arena_t* arena, size_t required_bytes) {
	if (required_bytes <= arena->committed) {
		return true;
	}

	size_t max_size = arena->reserved;
	if (required_bytes > max_size) {
		return false;
	}

	size_t need = required_bytes - arena->committed;
	size_t commit_size = em_align_up(need, EDGE_ARENA_COMMIT_CHUNK_SIZE);
	if (arena->committed + commit_size > max_size) {
		commit_size = max_size - arena->committed;
	}

	void* commit_addr = ptr_add(arena->base, arena->committed);

	if (!edge_vmem_commit(commit_addr, commit_size)) {
		return false;
	}

	arena->committed += commit_size;
	return true;
}

void* edge_arena_alloc_ex(edge_arena_t* arena, size_t size, size_t alignment) {
	if (!arena || !arena->base || size == 0) {
		return NULL;
	}

	if (alignment == 0) {
		alignment = _Alignof(max_align_t);
	}

	if ((alignment & (alignment - 1)) != 0) {
		return NULL;
	}

	edge_mtx_lock(&arena->mtx);

	size_t offset = arena->offset;
	size_t aligned_offset = em_align_up(offset, alignment);

	if (aligned_offset > __SIZE_MAX__ - size) {
		edge_mtx_unlock(&arena->mtx);
		return NULL; 
	}

	size_t max_size = arena->reserved;
	size_t new_offset = aligned_offset + size;
	if (new_offset > max_size) {
		edge_mtx_unlock(&arena->mtx);
		return NULL;
	}

	if (!arena_ensure_committed_locked(arena, new_offset)) {
		edge_mtx_unlock(&arena->mtx);
		return NULL;
	}
	void* result = ptr_add(arena->base, aligned_offset);
	arena->offset = new_offset;
	edge_mtx_unlock(&arena->mtx);

	return result;
}

void* edge_arena_alloc(edge_arena_t* arena, size_t size) {
	return edge_arena_alloc_ex(arena, size, 0);
}

void edge_arena_reset(edge_arena_t* arena, bool zero_memory) {
	if (!arena) {
		return;
	}

	edge_mtx_lock(&arena->mtx);
	if (zero_memory && arena->committed > 0) {
		memset(arena->base, 0, arena->committed);
	}
	arena->offset = 0;
	edge_mtx_unlock(&arena->mtx);
}
