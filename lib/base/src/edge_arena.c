#include "edge_arena.h"

#include "edge_allocator.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else

#include <unistd.h>
#include <sys/mman.h>
#include <pthread.h>
#include <sys/types.h>

#endif

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
	bool has_guard;

#if _WIN32
	CRITICAL_SECTION cs;
#else
	pthread_mutex_t cs;
#endif
};

static size_t align_up(size_t size, size_t alignment) {
	if (alignment == 0) {
		return size;
	}

	assert(alignment > 0 && (alignment & (alignment - 1)) == 0);
	return (size + alignment - 1) & ~(alignment - 1);
}

static void* ptr_add(void* p, size_t bytes) {
	return (void*)((uintptr_t)p + bytes);
}

static size_t get_page_size() {
#ifdef _WIN32
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	return (size_t)si.dwPageSize;
#else
	long page_size = sysconf(_SC_PAGESIZE);
	if (page_size <= 0) {
		return 4096;
	}
	return (size_t)page_size;
#endif
}

static bool virt_reserve(void** out_base, size_t reserve_bytes) {
#ifdef _WIN32
	void* base = VirtualAlloc(NULL, reserve_bytes, MEM_RESERVE, PAGE_NOACCESS);
	if (!base) {
		return false;
	}
	*out_base = base;
	return true;
#else
	// Use mmap with PROT_NONE to reserve address space.
	void* base = mmap(NULL, reserve_bytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (base == MAP_FAILED) {
		return false;
	}
	*out_base = base;
	return true;
#endif
}

static bool virt_release(void* base, size_t reserve_bytes) {
#ifdef _WIN32
	return VirtualFree(base, 0, MEM_RELEASE) != 0;
#else
	return munmap(base, reserve_bytes) == 0;
#endif
}

static bool virt_commit(void* addr, size_t size) {
#ifdef _WIN32
	return VirtualAlloc(addr, size, MEM_COMMIT, PAGE_READWRITE) != NULL;
#else
	int prot = PROT_READ | PROT_WRITE;
	if (mprotect(addr, size, prot) != 0) {
		return false;
	}

	return true;
#endif
}

#if 0
static bool virt_decommit(void* addr, size_t size) {
#ifdef _WIN32
	return VirtualFree(addr, size, MEM_DECOMMIT) != 0;
#else
	(void)madvise(addr, size, MADV_DONTNEED);
	if (mprotect(addr, size, PROT_NONE) != 0) {
		return false;
	}
	return true;
#endif
}
#endif

#ifdef _WIN32
static DWORD translate_protection_flags(edge_arena_prot_t p) {
	if (p == EDGE_ARENA_PROT_NONE) {
		return PAGE_NOACCESS;
	}

	if ((p & EDGE_ARENA_PROT_WRITE) != 0) {
		if ((p & EDGE_ARENA_PROT_EXEC) != 0) {
			return PAGE_EXECUTE_READWRITE;
		}
		return PAGE_READWRITE;
	}
	// no write
	if ((p & EDGE_ARENA_PROT_EXEC) != 0) {
		return PAGE_EXECUTE_READ;
	}
	return PAGE_READONLY;
}
#else
static int translate_protection_flags(edge_arena_prot_t p) {
	int flags = 0;
	if (p == EDGE_ARENA_PROT_NONE) {
		return PROT_NONE;
	}
	if ((p & EDGE_ARENA_PROT_READ) != 0) {
		flags |= PROT_READ;
	}
	if ((p & EDGE_ARENA_PROT_WRITE) != 0) {
		flags |= PROT_WRITE;
	}
	if ((p & EDGE_ARENA_PROT_EXEC) != 0) {
		flags |= PROT_EXEC;
	}
	return flags;
}
#endif

static bool virt_protect(void* addr, size_t size, edge_arena_prot_t prot) {
#ifdef _WIN32
	DWORD old;
	DWORD newf = translate_protection_flags(prot);
	if (!VirtualProtect(addr, size, newf, &old)) {
		return false;
	}
#else
	int flags = translate_protection_flags(prot);
	if (mprotect(addr, size, flags) != 0) {
		return false;
	}
#endif

	return true;
}

static void edge_arena_lock(edge_arena_t* arena) {
#ifdef _WIN32
	EnterCriticalSection(&arena->cs);
#else
	pthread_mutex_lock(&arena->cs);
#endif
}

static void edge_arena_unlock(edge_arena_t* arena) {
#ifdef _WIN32
	LeaveCriticalSection(&arena->cs);
#else
	pthread_mutex_unlock(&arena->cs);
#endif
}

edge_arena_t* edge_arena_create(edge_allocator_t* allocator, size_t size, bool guard_page) {
	if (!allocator) {
		return NULL;
	}

	if (size == 0) {
		size = EDGE_ARENA_MAX_SIZE;
	}

	size_t page_size = get_page_size();
	size = align_up(size, page_size);

	if (guard_page) {
		size += page_size;
	}

	void* base = NULL;
	if (!virt_reserve(&base, size)) {
		return NULL;
	}

	edge_arena_t* arena = (edge_arena_t*)edge_allocator_malloc(allocator, sizeof(edge_arena_t));
	if (!arena) {
		virt_release(base, size);
		return NULL;
	}

	arena->allocator = allocator;
	arena->base = base;
	arena->reserved = size;
	arena->committed = 0;
	arena->offset = 0;
	arena->page_size = page_size;
	arena->has_guard = guard_page;

#ifdef _WIN32
	InitializeCriticalSection(&arena->cs);
#else
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	int err = pthread_mutex_init(&arena->cs, &attr);
	pthread_mutexattr_destroy(&attr);
	if (err != 0) {
		virt_release(base, size);
		edge_allocator_free(allocator, arena);
		return NULL;
	}
#endif

	if (guard_page) {
		void* guard_addr = ptr_add(arena->base, arena->reserved - arena->page_size);
		if (!edge_arena_protect(arena, guard_addr, arena->page_size, EDGE_ARENA_PROT_NONE)) {
			edge_arena_destroy(arena);
			return NULL;
		}
	}

	return arena;
}

bool edge_arena_protect(edge_arena_t* arena, void* addr, size_t size, edge_arena_prot_t prot) {
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
	size_t total = align_up(size + page_off, arena->page_size);
	return virt_protect((void*)page_addr, total, prot);
}

void edge_arena_destroy(edge_arena_t* arena) {
	if (!arena) {
		return;
	}

	if (arena->base) {
		virt_release(arena->base, arena->reserved);
	}

#ifdef _WIN32
	DeleteCriticalSection(&arena->cs);
#else
	pthread_mutex_destroy(&arena->cs);
#endif

	edge_allocator_t* alloc = arena->allocator;
	edge_allocator_free(alloc, arena);
}

static bool arena_ensure_committed_locked(edge_arena_t* arena, size_t required_bytes) {
	if (required_bytes <= arena->committed) {
		return true;
	}

	size_t max_size = arena->reserved;
	if (arena->has_guard) {
		max_size -= arena->page_size;
	}

	if (required_bytes > max_size) {
		return false;
	}

	size_t need = required_bytes - arena->committed;
	size_t commit_size = align_up(need, EDGE_ARENA_COMMIT_CHUNK_SIZE);
	if (arena->committed + commit_size > max_size) {
		commit_size = max_size - arena->committed;
	}

	void* commit_addr = ptr_add(arena->base, arena->committed);

	if (!virt_commit(commit_addr, commit_size)) {
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

	edge_arena_lock(arena);

	size_t offset = arena->offset;
	size_t aligned_offset = align_up(offset, alignment);

	if (aligned_offset > SIZE_MAX - size) {
		edge_arena_unlock(arena);
		return NULL; 
	}

	size_t max_size = arena->reserved;
	if (arena->has_guard) {
		max_size -= arena->page_size;
	}

	size_t new_offset = aligned_offset + size;
	if (new_offset > max_size) {
		edge_arena_unlock(arena);
		return NULL;
	}

	if (!arena_ensure_committed_locked(arena, new_offset)) {
		edge_arena_unlock(arena);
		return NULL;
	}
	void* result = ptr_add(arena->base, aligned_offset);
	arena->offset = new_offset;
	edge_arena_unlock(arena);

	return result;
}

void* edge_arena_alloc(edge_arena_t* arena, size_t size) {
	return edge_arena_alloc_ex(arena, size, 0);
}

void edge_arena_reset(edge_arena_t* arena, bool zero_memory) {
	if (!arena) {
		return;
	}

	edge_arena_lock(arena);
	if (zero_memory && arena->committed > 0) {
		memset(arena->base, 0, arena->committed);
	}
	arena->offset = 0;
	edge_arena_unlock(arena);
}