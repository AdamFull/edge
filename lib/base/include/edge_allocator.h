#ifndef EDGE_ALLOCATOR_H
#define EDGE_ALLOCATOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/* Allocator function pointers */
typedef void* (*edge_malloc_func)(size_t size);
typedef void  (*edge_free_func)(void* ptr);
typedef void* (*edge_realloc_func)(void* ptr, size_t size);
typedef void* (*edge_calloc_func)(size_t nmemb, size_t size);
typedef char* (*edge_strdup_func)(const char* str);

typedef struct edge_allocator {
    edge_malloc_func  malloc_fn;
    edge_free_func    free_fn;
    edge_realloc_func realloc_fn;
    edge_calloc_func  calloc_fn;
    edge_strdup_func  strdup_fn;
} edge_allocator_t;

typedef enum edge_allocator_memprotect_flags {
    EDGE_ALLOCATOR_PROTECT_NONE = 0,
    EDGE_ALLOCATOR_PROTECT_READ = 1,
    EDGE_ALLOCATOR_PROTECT_WRITE = 2,
    EDGE_ALLOCATOR_PROTECT_READ_WRITE = 3
} edge_allocator_memprotect_flags_t;

/**
 * Create an allocator with custom functions
 *
 * @param malloc_fn Custom malloc function
 * @param free_fn Custom free function
 * @param realloc_fn Custom realloc function
 * @param calloc_fn Custom calloc function (can be NULL, will be implemented via malloc)
 * @param strdup_fn Custom strdup function (can be NULL, will be implemented via malloc)
 * @return Allocator instance
 */
edge_allocator_t edge_allocator_create(edge_malloc_func malloc_fn, edge_free_func free_fn, edge_realloc_func realloc_fn, edge_calloc_func calloc_fn, edge_strdup_func strdup_fn);

/**
 * Create an allocator using the default system allocator (malloc/free/etc.)
 *
 * @return Allocator instance using system functions
 */
edge_allocator_t edge_allocator_create_default(void);

/**
 * Allocate memory using the allocator
 */
void* edge_allocator_malloc(const edge_allocator_t* allocator, size_t size);

/**
 * Free memory using the allocator
 */
void edge_allocator_free(const edge_allocator_t* allocator, void* ptr);

/**
 * Reallocate memory using the allocator
 */
void* edge_allocator_realloc(const edge_allocator_t* allocator, void* ptr, size_t size);

/**
 * Allocate and zero memory using the allocator
 */
void* edge_allocator_calloc(const edge_allocator_t* allocator, size_t nmemb, size_t size);

/**
 * Duplicate a string using the allocator
 */
char* edge_allocator_strdup(const edge_allocator_t* allocator, const char* str);

/**
 * Duplicate a string with length limit using the allocator
 */
char* edge_allocator_strndup(const edge_allocator_t* allocator, const char* str, size_t n);

/**
 * Protect memory range
 */
void edge_allocator_protect(void* ptr, size_t size, edge_allocator_memprotect_flags_t flags);

#ifdef __cplusplus
}
#endif

#endif