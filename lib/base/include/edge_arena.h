#ifndef EDGE_STACK_ALLOCATOR_H
#define EDGE_STACK_ALLOCATOR_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define EDGE_ARENA_MAX_SIZE (256 * 1024 * 1024)
#define EDGE_ARENA_COMMIT_CHUNK_SIZE (64 * 1024)

#ifdef __cplusplus
extern "C" {
#endif

	typedef enum edge_arena_prot {
		EDGE_ARENA_PROT_NONE = 0,
		EDGE_ARENA_PROT_READ = 0x01,
		EDGE_ARENA_PROT_WRITE = 0x02,
		EDGE_ARENA_PROT_EXEC = 0x04,
	} edge_arena_prot_t;

	typedef struct edge_allocator edge_allocator_t;
	typedef struct edge_arena edge_arena_t;

	edge_arena_t* edge_arena_create(edge_allocator_t* allocator, size_t size, bool guard_page);

	void edge_arena_destroy(edge_arena_t* arena);

	bool edge_arena_protect(edge_arena_t* arena, void* addr, size_t size, edge_arena_prot_t prot);

	void* edge_arena_alloc_ex(edge_arena_t* arena, size_t size, size_t alignment);
	void* edge_arena_alloc(edge_arena_t* arena, size_t size);

	void edge_arena_reset(edge_arena_t* arena, bool zero_memory);

#ifdef __cplusplus
}
#endif

#endif // EDGE_STACK_ALLOCATOR_H