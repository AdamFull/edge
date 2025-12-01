#include "edge_handle_pool.h"
#include "edge_allocator.h"
#include "edge_vector.h"
#include <string.h>

edge_handle_pool_t* edge_handle_pool_create(const edge_allocator_t* allocator, size_t element_size, u32 capacity) {
    if (!allocator || element_size == 0 || capacity == 0 || capacity > EDGE_HANDLE_MAX_CAPACITY) {
        return NULL;
    }

    edge_handle_pool_t* pool = (edge_handle_pool_t*)edge_allocator_malloc(allocator, sizeof(edge_handle_pool_t));
    if (!pool) {
        return NULL;
    }

    pool->data = edge_allocator_calloc(allocator, capacity, element_size);
    if (!pool->data) {
        edge_allocator_free(allocator, pool);
        return NULL;
    }

    pool->versions = (edge_ver_t*)edge_allocator_calloc(allocator, capacity, sizeof(edge_ver_t));
    if (!pool->versions) {
        edge_allocator_free(allocator, pool->data);
        edge_allocator_free(allocator, pool);
        return NULL;
    }

    pool->free_indices = edge_vector_create(allocator, sizeof(u32), capacity);
    if (!pool->free_indices) {
        edge_allocator_free(allocator, pool->versions);
        edge_allocator_free(allocator, pool->data);
        edge_allocator_free(allocator, pool);
        return NULL;
    }

    pool->element_size = element_size;
    pool->capacity = capacity;
    pool->count = 0;
    pool->allocator = allocator;

    for (u32 i = 0; i < capacity; i++) {
        u32 index = capacity - 1 - i;
        edge_vector_push_back(pool->free_indices, &index);
        pool->versions[i] = 0;
    }

    return pool;
}

void edge_handle_pool_destroy(edge_handle_pool_t* pool) {
    if (!pool) {
        return;
    }

    if (pool->free_indices) {
        edge_vector_destroy(pool->free_indices);
    }

    if (pool->versions) {
        edge_allocator_free(pool->allocator, pool->versions);
    }

    if (pool->data) {
        edge_allocator_free(pool->allocator, pool->data);
    }

    edge_allocator_free(pool->allocator, pool);
}

edge_handle_t edge_handle_pool_allocate(edge_handle_pool_t* pool) {
    if (!pool || edge_vector_empty(pool->free_indices)) {
        return EDGE_HANDLE_INVALID;
    }

    u32 index;
    if (!edge_vector_pop_back(pool->free_indices, &index)) {
        return EDGE_HANDLE_INVALID;
    }

    void* element = (char*)pool->data + (index * pool->element_size);
    memset(element, 0, pool->element_size);

    edge_ver_t version = pool->versions[index];

    pool->count++;

    return edge_handle_make(index, version);
}

edge_handle_t edge_handle_pool_allocate_with_data(edge_handle_pool_t* pool, const void* element) {
    if (!pool || !element || edge_vector_empty(pool->free_indices)) {
        return EDGE_HANDLE_INVALID;
    }

    u32 index;
    if (!edge_vector_pop_back(pool->free_indices, &index)) {
        return EDGE_HANDLE_INVALID;
    }

    void* dest = (char*)pool->data + (index * pool->element_size);
    memcpy(dest, element, pool->element_size);

    edge_ver_t version = pool->versions[index];

    pool->count++;

    return edge_handle_make(index, version);
}

bool edge_handle_pool_free(edge_handle_pool_t* pool, edge_handle_t handle) {
    if (!pool || handle == EDGE_HANDLE_INVALID) {
        return false;
    }

    u32 index = edge_handle_get_index(handle);
    u32 version = edge_handle_get_version(handle);

    if (index >= pool->capacity) {
        return false;
    }

    if (pool->versions[index] != version) {
        return false;
    }

    pool->versions[index] = ((pool->versions[index] + 1) & EDGE_HANDLE_VERSION_MASK);

    void* element = (char*)pool->data + (index * pool->element_size);
    memset(element, 0, pool->element_size);

    edge_vector_push_back(pool->free_indices, &index);
    pool->count--;

    return true;
}

void* edge_handle_pool_get(edge_handle_pool_t* pool, edge_handle_t handle) {
    if (!pool || handle == EDGE_HANDLE_INVALID) {
        return NULL;
    }

    u32 index = edge_handle_get_index(handle);
    edge_ver_t version = edge_handle_get_version(handle);

    if (index >= pool->capacity) {
        return NULL;
    }

    if (pool->versions[index] != version) {
        return NULL;
    }

    return (char*)pool->data + (index * pool->element_size);
}

const void* edge_handle_pool_get_const(const edge_handle_pool_t* pool, edge_handle_t handle) {
    if (!pool || handle == EDGE_HANDLE_INVALID) {
        return NULL;
    }

    u32 index = edge_handle_get_index(handle);
    edge_ver_t version = edge_handle_get_version(handle);

    if (index >= pool->capacity) {
        return NULL;
    }

    if (pool->versions[index] != version) {
        return NULL;
    }

    return (const char*)pool->data + (index * pool->element_size);
}

bool edge_handle_pool_set(edge_handle_pool_t* pool, edge_handle_t handle, const void* element) {
    if (!pool || !element || handle == EDGE_HANDLE_INVALID) {
        return false;
    }

    u32 index = edge_handle_get_index(handle);
    edge_ver_t version = edge_handle_get_version(handle);

    if (index >= pool->capacity) {
        return false;
    }

    if (pool->versions[index] != version) {
        return false;
    }

    void* dest = (char*)pool->data + (index * pool->element_size);
    memcpy(dest, element, pool->element_size);

    return true;
}

bool edge_handle_pool_is_valid(const edge_handle_pool_t* pool, edge_handle_t handle) {
    if (!pool || handle == EDGE_HANDLE_INVALID) {
        return false;
    }

    u32 index = edge_handle_get_index(handle);
    edge_ver_t version = edge_handle_get_version(handle);

    if (index >= pool->capacity) {
        return false;
    }

    return pool->versions[index] == version;
}

u32 edge_handle_pool_count(const edge_handle_pool_t* pool) {
    return pool ? pool->count : 0;
}

u32 edge_handle_pool_capacity(const edge_handle_pool_t* pool) {
    return pool ? pool->capacity : 0;
}

size_t edge_handle_pool_element_size(const edge_handle_pool_t* pool) {
    return pool ? pool->element_size : 0;
}

bool edge_handle_pool_is_full(const edge_handle_pool_t* pool) {
    return pool ? edge_vector_empty(pool->free_indices) : true;
}

bool edge_handle_pool_is_empty(const edge_handle_pool_t* pool) {
    return pool ? (pool->count == 0) : true;
}

void edge_handle_pool_clear(edge_handle_pool_t* pool) {
    if (!pool) {
        return;
    }

    edge_vector_clear(pool->free_indices);

    for (u32 i = 0; i < pool->capacity; i++) {
        u32 index = pool->capacity - 1 - i;
        edge_vector_push_back(pool->free_indices, &index);

        pool->versions[i] = ((pool->versions[i] + 1) & EDGE_HANDLE_VERSION_MASK);

        void* element = (char*)pool->data + (i * pool->element_size);
        memset(element, 0, pool->element_size);
    }

    pool->count = 0;
}