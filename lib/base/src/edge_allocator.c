#include "edge_allocator.h"

#include <stdlib.h>
#include <string.h>

#if _WIN32
#include <Windows.h>
#else
#include <sys/mman.h>
#endif

edge_allocator_t edge_allocator_create(edge_malloc_func malloc_fn, edge_free_func free_fn, edge_realloc_func realloc_fn, 
    edge_calloc_func calloc_fn, edge_strdup_func strdup_fn) {
    
    edge_allocator_t allocator;
    allocator.malloc_fn = malloc_fn;
    allocator.free_fn = free_fn;
    allocator.realloc_fn = realloc_fn;
    allocator.calloc_fn = calloc_fn;
    allocator.strdup_fn = strdup_fn;
    return allocator;
}

edge_allocator_t edge_allocator_create_default(void) {
    return edge_allocator_create(malloc, free, realloc, NULL, NULL);
}

/* Fallback implementations for when calloc/strdup aren't provided */
static void* fallback_calloc(const edge_allocator_t* allocator, size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    void* ptr = allocator->malloc_fn(total);
    if (ptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}

static char* fallback_strdup(const edge_allocator_t* allocator, const char* str) {
    if (!str) return NULL;
    size_t len = strlen(str);
    char* copy = (char*)allocator->malloc_fn(len + 1);
    if (copy) {
        memcpy(copy, str, len + 1);
    }
    return copy;
}

void* edge_allocator_malloc(const edge_allocator_t* allocator, size_t size) {
    if (!allocator || !allocator->malloc_fn) return NULL;
    return allocator->malloc_fn(size);
}

void edge_allocator_free(const edge_allocator_t* allocator, void* ptr) {
    if (!allocator || !allocator->free_fn || !ptr) return;
    allocator->free_fn(ptr);
}

void* edge_allocator_realloc(const edge_allocator_t* allocator, void* ptr, size_t size) {
    if (!allocator || !allocator->realloc_fn) return NULL;
    return allocator->realloc_fn(ptr, size);
}

void* edge_allocator_calloc(const edge_allocator_t* allocator, size_t nmemb, size_t size) {
    if (!allocator) return NULL;

    if (allocator->calloc_fn) {
        return allocator->calloc_fn(nmemb, size);
    }

    /* Fallback to malloc + memset if calloc not provided */
    return fallback_calloc(allocator, nmemb, size);
}

char* edge_allocator_strdup(const edge_allocator_t* allocator, const char* str) {
    if (!allocator || !str) return NULL;

    if (allocator->strdup_fn) {
        return allocator->strdup_fn(str);
    }

    /* Fallback to malloc + memcpy if strdup not provided */
    return fallback_strdup(allocator, str);
}

char* edge_allocator_strndup(const edge_allocator_t* allocator, const char* str, size_t n) {
    if (!allocator || !str) return NULL;

    size_t len = strlen(str);
    if (n < len) len = n;

    char* copy = (char*)allocator->malloc_fn(len + 1);
    if (copy) {
        memcpy(copy, str, len);
        copy[len] = '\0';
    }
    return copy;
}

void edge_allocator_protect(void* ptr, size_t size, edge_allocator_memprotect_flags_t flags) {
#if _WIN32
    DWORD old_flags;
    DWORD new_flags = 0;

    if (flags & EDGE_ALLOCATOR_PROTECT_READ_WRITE) {
        new_flags = PAGE_READWRITE;
    }
    else if (flags & EDGE_ALLOCATOR_PROTECT_READ) {
        new_flags = PAGE_READONLY;
    }

    VirtualProtect(ptr, size, new_flags, &old_flags);
#else
    int new_flags = 0;
    if (flags & EDGE_ALLOCATOR_PROTECT_READ) {
        new_flags |= PROT_READ;
    }
    if (flags & EDGE_ALLOCATOR_PROTECT_WRITE) {
        new_flags |= PROT_WRITE;
    }

    mprotect(ptr, size, new_flags);
#endif
}