#include "edge_free_index_list.h"
#include "edge_allocator.h"

struct edge_free_list {
    u32* indices;
    u32 capacity;
    u32 count;
    const edge_allocator_t* allocator;
};

edge_free_list_t* edge_free_list_create(const edge_allocator_t* allocator, u32 capacity) {
    if (!allocator || capacity == 0) {
        return NULL;
    }

    edge_free_list_t* list = (edge_free_list_t*)edge_allocator_malloc(allocator, sizeof(edge_free_list_t));
    if (!list) {
        return NULL;
    }

    list->indices = (u32*)edge_allocator_malloc(allocator, capacity * sizeof(u32));
    if (!list->indices) {
        edge_allocator_free(allocator, list);
        return NULL;
    }

    list->capacity = capacity;
    list->count = capacity;
    list->allocator = allocator;

    for (u32 i = 0; i < capacity; i++) {
        list->indices[i] = capacity - 1 - i;
    }

    return list;
}

void edge_free_list_destroy(edge_free_list_t* list) {
    if (!list) {
        return;
    }

    if (list->indices) {
        edge_allocator_free(list->allocator, list->indices);
    }

    edge_allocator_free(list->allocator, list);
}

bool edge_free_list_allocate(edge_free_list_t* list, u32* out_index) {
    if (!list || !out_index || list->count == 0) {
        return false;
    }

    *out_index = list->indices[--list->count];

    return true;
}

bool edge_free_list_free(edge_free_list_t* list, u32 index) {
    if (!list || list->count >= list->capacity) {
        return false;
    }

    if (index >= list->capacity) {
        return false;
    }

    list->indices[list->count++] = index;

    return true;
}

u32 edge_free_list_available(const edge_free_list_t* list) {
    return list ? list->count : 0;
}

u32 edge_free_list_capacity(const edge_free_list_t* list) {
    return list ? list->capacity : 0;
}

bool edge_free_list_has_available(const edge_free_list_t* list) {
    return list && list->count > 0;
}

bool edge_free_list_is_full(const edge_free_list_t* list) {
    return list && list->count == list->capacity;
}

bool edge_free_list_is_empty(const edge_free_list_t* list) {
    return !list || list->count == 0;
}

void edge_free_list_reset(edge_free_list_t* list) {
    if (!list) {
        return;
    }

    list->count = list->capacity;

    for (u32 i = 0; i < list->capacity; i++) {
        list->indices[i] = list->capacity - 1 - i;
    }
}

void edge_free_list_clear(edge_free_list_t* list) {
    if (!list) {
        return;
    }

    list->count = 0;
}