#include "edge_testing.h"

#include <stdlib.h>
#include <string.h>

struct alloc_stats {
    atomic_size_t alloc_bytes;
    atomic_size_t free_bytes;
};

static struct alloc_stats g_alloc_stats = { 0 };

struct alloc_header {
    size_t size;
};

static void* tracked_malloc(size_t size) {
    if (size == 0) {
        return NULL;
    }

    size_t total_size = sizeof(struct alloc_header) + size;
    struct alloc_header* header = (struct alloc_header*)malloc(total_size);
    if (header == NULL) {
        return NULL;
    }

    header->size = size;

    atomic_fetch_add(&g_alloc_stats.alloc_bytes, size);
    return (void*)(header + 1);
}

static void* tracked_calloc(size_t num, size_t size) {
    if (num == 0 || size == 0) {
        return NULL;
    }

    size_t total_data_size = num * size;
    if (total_data_size / num != size) {
        return NULL;
    }

    void* ptr = tracked_malloc(total_data_size);
    if (ptr != NULL) {
        memset(ptr, 0, total_data_size);
    }

    return ptr;
}

static void tracked_free(void* ptr) {
    if (ptr == NULL) {
        return;
    }

    struct alloc_header* header = ((struct alloc_header*)ptr) - 1;
    atomic_fetch_add(&g_alloc_stats.free_bytes, header->size);

    free(header);
}

static void* tracked_realloc(void* ptr, size_t new_size) {
    if (ptr == NULL) {
        return tracked_malloc(new_size);
    }

    if (new_size == 0) {
        tracked_free(ptr);
        return NULL;
    }

    struct alloc_header* old_header = ((struct alloc_header*)ptr) - 1;
    size_t old_size = old_header->size;

    size_t total_size = sizeof(struct alloc_header) + new_size;
    struct alloc_header* new_header = (struct alloc_header*)realloc(old_header, total_size);
    if (new_header == NULL) {
        return NULL;
    }

    atomic_fetch_add(&g_alloc_stats.free_bytes, old_size);
    atomic_fetch_add(&g_alloc_stats.alloc_bytes, new_size);

    new_header->size = new_size;
    return (void*)(new_header + 1);
}

edge_allocator_t edge_testing_allocator_create(void) {
    return edge_allocator_create(tracked_malloc, tracked_free, tracked_realloc, tracked_calloc, NULL);
}

size_t edge_testing_net_allocated(void) {
    size_t alloc = atomic_load(&g_alloc_stats.alloc_bytes);
    size_t freed = atomic_load(&g_alloc_stats.free_bytes);
    return alloc - freed;
}
