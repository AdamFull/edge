#include "edge_vector.h"
#include "edge_allocator.h"

#include <string.h>
#include <stdlib.h>

#define EDGE_VECTOR_DEFAULT_CAPACITY 16

edge_vector_t* edge_vector_create(const edge_allocator_t* allocator, size_t element_size, size_t initial_capacity) {
    if (!allocator || element_size == 0) {
        return NULL;
    }

    edge_vector_t* vec = (edge_vector_t*)edge_allocator_malloc(allocator, sizeof(edge_vector_t));
    if (!vec) {
        return NULL;
    }

    if (initial_capacity == 0) {
        initial_capacity = EDGE_VECTOR_DEFAULT_CAPACITY;
    }

    vec->data = edge_allocator_malloc(allocator, element_size * initial_capacity);
    if (!vec->data) {
        edge_allocator_free(allocator, vec);
        return NULL;
    }

    vec->size = 0;
    vec->capacity = initial_capacity;
    vec->element_size = element_size;
    vec->allocator = allocator;

    return vec;
}

void edge_vector_destroy(edge_vector_t* vec) {
    if (!vec) {
        return;
    }

    if (vec->data) {
        edge_allocator_free(vec->allocator, vec->data);
    }
    edge_allocator_free(vec->allocator, vec);
}

void edge_vector_clear(edge_vector_t* vec) {
    if (vec) {
        vec->size = 0;
    }
}

bool edge_vector_reserve(edge_vector_t* vec, size_t capacity) {
    if (!vec) {
        return false;
    }

    if (capacity <= vec->capacity) {
        return true;
    }

    void* new_data = edge_allocator_realloc(vec->allocator, vec->data, vec->element_size * capacity);
    if (!new_data) {
        return false;
    }

    vec->data = new_data;
    vec->capacity = capacity;

    return true;
}

bool edge_vector_push_back(edge_vector_t* vec, const void* element) {
    if (!vec || !element) {
        return false;
    }

    if (vec->size >= vec->capacity) {
        size_t new_capacity = vec->capacity * 2;
        if (!edge_vector_reserve(vec, new_capacity)) {
            return false;
        }
    }

    void* dest = (char*)vec->data + (vec->size * vec->element_size);
    memcpy(dest, element, vec->element_size);
    vec->size++;

    return true;
}

bool edge_vector_pop_back(edge_vector_t* vec, void* out_element) {
    if (!vec || vec->size == 0) {
        return false;
    }

    vec->size--;
    if (out_element) {
        void* src = (char*)vec->data + (vec->size * vec->element_size);
        memcpy(out_element, src, vec->element_size);
    }

    return true;
}

void* edge_vector_at(const edge_vector_t* vec, size_t index) {
    if (!vec) {
        return NULL;
    }

    return (char*)vec->data + (index * vec->element_size);
}

void* edge_vector_get(const edge_vector_t* vec, size_t index) {
    if (!vec || index >= vec->size) {
        return NULL;
    }

    return edge_vector_at(vec, index);
}

bool edge_vector_set(edge_vector_t* vec, size_t index, const void* element) {
    if (!vec || !element || index >= vec->size) {
        return false;
    }

    void* dest = edge_vector_at(vec, index);
    memcpy(dest, element, vec->element_size);

    return true;
}

bool edge_vector_insert(edge_vector_t* vec, size_t index, const void* element) {
    if (!vec || !element || index > vec->size) {
        return false;
    }

    if (vec->size >= vec->capacity) {
        if (!edge_vector_reserve(vec, vec->capacity * 2)) {
            return false;
        }
    }

    /* Move elements to make room */
    if (index < vec->size) {
        void* src = (char*)vec->data + (index * vec->element_size);
        void* dest = (char*)vec->data + ((index + 1) * vec->element_size);
        size_t bytes_to_move = (vec->size - index) * vec->element_size;
        memmove(dest, src, bytes_to_move);
    }

    /* Insert new element */
    void* dest = (char*)vec->data + (index * vec->element_size);
    memcpy(dest, element, vec->element_size);
    vec->size++;

    return true;
}

bool edge_vector_remove(edge_vector_t* vec, size_t index, void* out_element) {
    if (!vec || index >= vec->size) {
        return false;
    }

    void* element = edge_vector_at(vec, index);

    if (out_element) {
        memcpy(out_element, element, vec->element_size);
    }

    /* Move elements to fill gap */
    if (index < vec->size - 1) {
        void* dest = element;
        void* src = (char*)vec->data + ((index + 1) * vec->element_size);
        size_t bytes_to_move = (vec->size - index - 1) * vec->element_size;
        memmove(dest, src, bytes_to_move);
    }

    vec->size--;

    return true;
}

void* edge_vector_front(const edge_vector_t* vec) {
    if (!vec || vec->size == 0) {
        return NULL;
    }
    return vec->data;
}

void* edge_vector_back(const edge_vector_t* vec) {
    if (!vec || vec->size == 0) {
        return NULL;
    }
    return (char*)vec->data + ((vec->size - 1) * vec->element_size);
}

void* edge_vector_data(const edge_vector_t* vec) {
    return vec ? vec->data : NULL;
}

size_t edge_vector_size(const edge_vector_t* vec) {
    return vec ? vec->size : 0;
}

size_t edge_vector_capacity(const edge_vector_t* vec) {
    return vec ? vec->capacity : 0;
}

bool edge_vector_empty(const edge_vector_t* vec) {
    return !vec || vec->size == 0;
}

bool edge_vector_resize(edge_vector_t* vec, size_t new_size) {
    if (!vec) {
        return false;
    }

    if (new_size > vec->capacity) {
        size_t new_capacity = vec->capacity;
        while (new_capacity < new_size) {
            new_capacity *= 2;
        }
        if (!edge_vector_reserve(vec, new_capacity)) {
            return false;
        }
    }

    /* Zero out new elements if growing */
    if (new_size > vec->size) {
        void* start = (char*)vec->data + (vec->size * vec->element_size);
        size_t bytes_to_zero = (new_size - vec->size) * vec->element_size;
        memset(start, 0, bytes_to_zero);
    }

    vec->size = new_size;

    return true;
}

bool edge_vector_shrink_to_fit(edge_vector_t* vec) {
    if (!vec || vec->size >= vec->capacity) {
        return true;
    }

    if (vec->size == 0) {
        edge_allocator_free(vec->allocator, vec->data);
        vec->data = edge_allocator_malloc(vec->allocator, vec->element_size * EDGE_VECTOR_DEFAULT_CAPACITY);
        if (!vec->data) {
            return false;
        }
        vec->capacity = EDGE_VECTOR_DEFAULT_CAPACITY;
        return true;
    }

    void* new_data = edge_allocator_realloc(vec->allocator, vec->data, vec->element_size * vec->size);
    if (!new_data) {
        return false;
    }

    vec->data = new_data;
    vec->capacity = vec->size;

    return true;
}

int edge_vector_find(const edge_vector_t* vec, const void* element, int (*compare)(const void*, const void*)) {
    if (!vec || !element || !compare) {
        return -1;
    }

    for (size_t i = 0; i < vec->size; i++) {
        void* current = edge_vector_at(vec, i);
        if (compare(current, element) == 0) {
            return (int)i;
        }
    }

    return -1;
}

void edge_vector_sort(edge_vector_t* vec, int (*compare)(const void*, const void*)) {
    if (!vec || !compare || vec->size < 2) {
        return;
    }
    qsort(vec->data, vec->size, vec->element_size, compare);
}