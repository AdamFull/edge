#include "edge_queue.h"
#include "edge_allocator.h"

#include <string.h>

#define EDGE_QUEUE_DEFAULT_CAPACITY 16

edge_queue_t* edge_queue_create(const edge_allocator_t* allocator, size_t element_size, size_t initial_capacity) {
    if (!allocator || element_size == 0) {
        return NULL;
    }

    edge_queue_t* queue = (edge_queue_t*)edge_allocator_malloc(allocator, sizeof(edge_queue_t));
    if (!queue) {
        return NULL;
    }

    if (initial_capacity == 0) {
        initial_capacity = EDGE_QUEUE_DEFAULT_CAPACITY;
    }

    queue->data = edge_allocator_malloc(allocator, element_size * initial_capacity);
    if (!queue->data) {
        edge_allocator_free(allocator, queue);
        return NULL;
    }

    queue->head = 0;
    queue->tail = 0;
    queue->size = 0;
    queue->capacity = initial_capacity;
    queue->element_size = element_size;
    queue->allocator = allocator;

    return queue;
}

void edge_queue_destroy(edge_queue_t* queue) {
    if (!queue) {
        return;
    }

    if (queue->data) {
        edge_allocator_free(queue->allocator, queue->data);
    }
    edge_allocator_free(queue->allocator, queue);
}

void edge_queue_clear(edge_queue_t* queue) {
    if (!queue) {
        return;
    }

    queue->head = 0;
    queue->tail = 0;
    queue->size = 0;
}

bool edge_queue_reserve(edge_queue_t* queue, size_t capacity) {
    if (!queue) {
        return false;
    }

    if (capacity <= queue->capacity) {
        return true;
    }

    void* new_data = edge_allocator_malloc(queue->allocator, queue->element_size * capacity);
    if (!new_data) {
        return false;
    }

    /* Copy elements to new buffer in linear order */
    if (queue->size > 0) {
        size_t first_part_size = 0;
        size_t second_part_size = 0;

        if (queue->head < queue->tail) {
            /* Contiguous */
            first_part_size = queue->size;
        }
        else {
            /* Wrapped around */
            first_part_size = queue->capacity - queue->head;
            second_part_size = queue->tail;
        }

        /* Copy first part */
        if (first_part_size > 0) {
            void* src = (char*)queue->data + (queue->head * queue->element_size);
            memcpy(new_data, src, first_part_size * queue->element_size);
        }

        /* Copy second part */
        if (second_part_size > 0) {
            void* dest = (char*)new_data + (first_part_size * queue->element_size);
            memcpy(dest, queue->data, second_part_size * queue->element_size);
        }
    }

    edge_allocator_free(queue->allocator, queue->data);
    queue->data = new_data;
    queue->head = 0;
    queue->tail = queue->size;
    queue->capacity = capacity;

    return true;
}

bool edge_queue_enqueue(edge_queue_t* queue, const void* element) {
    if (!queue || !element) {
        return false;
    }

    if (queue->size >= queue->capacity) {
        if (!edge_queue_reserve(queue, queue->capacity * 2)) {
            return false;
        }
    }

    void* dest = (char*)queue->data + (queue->tail * queue->element_size);
    memcpy(dest, element, queue->element_size);

    queue->tail = (queue->tail + 1) % queue->capacity;
    queue->size++;

    return true;
}

bool edge_queue_dequeue(edge_queue_t* queue, void* out_element) {
    if (!queue || queue->size == 0) {
        return false;
    }

    void* src = (char*)queue->data + (queue->head * queue->element_size);

    if (out_element) {
        memcpy(out_element, src, queue->element_size);
    }

    queue->head = (queue->head + 1) % queue->capacity;
    queue->size--;

    return true;
}

void* edge_queue_front(const edge_queue_t* queue) {
    if (!queue || queue->size == 0) {
        return NULL;
    }
    return (char*)queue->data + (queue->head * queue->element_size);
}

void* edge_queue_back(const edge_queue_t* queue) {
    if (!queue || queue->size == 0) {
        return NULL;
    }

    size_t back_index = (queue->tail == 0) ? (queue->capacity - 1) : (queue->tail - 1);
    return (char*)queue->data + (back_index * queue->element_size);
}

size_t edge_queue_size(const edge_queue_t* queue) {
    return queue ? queue->size : 0;
}

bool edge_queue_empty(const edge_queue_t* queue) {
    return !queue || queue->size == 0;
}