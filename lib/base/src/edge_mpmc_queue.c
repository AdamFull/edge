#include "edge_mpmc_queue.h"
#include "edge_allocator.h"
#include "edge_math.h"

#include <string.h>

edge_mpmc_queue_t* edge_mpmc_queue_create(const edge_allocator_t* allocator,
    size_t element_size,
    size_t capacity) {
    if (!allocator || element_size == 0 || capacity == 0) {
        return NULL;
    }

    if (!em_is_pow2(capacity)) {
        capacity = em_next_pow2(capacity);
    }

    if (capacity > __SIZE_MAX__ / 2) {
        return NULL;
    }

    edge_mpmc_queue_t* queue = (edge_mpmc_queue_t*)edge_allocator_malloc(allocator, sizeof(edge_mpmc_queue_t));
    if (!queue) {
        return NULL;
    }

    size_t node_size = sizeof(edge_mpmc_node_t) + element_size;
    queue->buffer = (edge_mpmc_node_t*)edge_allocator_malloc(allocator, node_size * capacity);
    if (!queue->buffer) {
        edge_allocator_free(allocator, queue);
        return NULL;
    }

    for (size_t i = 0; i < capacity; i++) {
        edge_mpmc_node_t* node = (edge_mpmc_node_t*)((char*)queue->buffer + i * node_size);
        atomic_init(&node->sequence, i);
    }

    queue->capacity = capacity;
    queue->element_size = element_size;
    queue->mask = capacity - 1;
    queue->allocator = allocator;

    atomic_init(&queue->enqueue_pos, 0);
    atomic_init(&queue->dequeue_pos, 0);

    return queue;
}

void edge_mpmc_queue_destroy(edge_mpmc_queue_t* queue) {
    if (!queue) {
        return;
    }

    if (queue->buffer) {
        edge_allocator_free(queue->allocator, queue->buffer);
    }
    edge_allocator_free(queue->allocator, queue);
}

bool edge_mpmc_queue_enqueue(edge_mpmc_queue_t* queue, const void* element) {
    if (!queue || !element) {
        return false;
    }

    size_t node_size = sizeof(edge_mpmc_node_t) + queue->element_size;
    size_t pos;
    edge_mpmc_node_t* node;
    size_t seq;

    for (;;) {
        pos = atomic_load_explicit(&queue->enqueue_pos, memory_order_relaxed);
        node = (edge_mpmc_node_t*)((char*)queue->buffer + (pos & queue->mask) * node_size);
        seq = atomic_load_explicit(&node->sequence, memory_order_acquire);

        intptr_t diff = (intptr_t)seq - (intptr_t)pos;

        if (diff == 0) {
            if (atomic_compare_exchange_weak_explicit(&queue->enqueue_pos, &pos, pos + 1, memory_order_relaxed, memory_order_relaxed)) {
                break;
            }
        }
        else if (diff < 0) {
            return false;
        }
    }

    memcpy(node->data, element, queue->element_size);

    atomic_store_explicit(&node->sequence, pos + 1, memory_order_release);

    return true;
}

bool edge_mpmc_queue_dequeue(edge_mpmc_queue_t* queue, void* out_element) {
    if (!queue) {
        return false;
    }

    size_t node_size = sizeof(edge_mpmc_node_t) + queue->element_size;
    size_t pos;
    edge_mpmc_node_t* node;
    size_t seq;

    for (;;) {
        pos = atomic_load_explicit(&queue->dequeue_pos, memory_order_relaxed);
        node = (edge_mpmc_node_t*)((char*)queue->buffer + (pos & queue->mask) * node_size);
        seq = atomic_load_explicit(&node->sequence, memory_order_acquire);

        intptr_t diff = (intptr_t)seq - (intptr_t)(pos + 1);

        if (diff == 0) {
            if (atomic_compare_exchange_weak_explicit(&queue->dequeue_pos, &pos, pos + 1, memory_order_relaxed, memory_order_relaxed)) {
                break;
            }
        }
        else if (diff < 0) {
            return false;
        }
    }

    if (out_element) {
        memcpy(out_element, node->data, queue->element_size);
    }

    atomic_store_explicit(&node->sequence, pos + queue->mask + 1, memory_order_release);

    return true;
}

bool edge_mpmc_queue_try_enqueue(edge_mpmc_queue_t* queue, const void* element, size_t max_retries) {
    if (!queue || !element) {
        return false;
    }

    size_t node_size = sizeof(edge_mpmc_node_t) + queue->element_size;
    size_t pos;
    edge_mpmc_node_t* node;
    size_t seq;
    size_t retries = 0;

    for (;;) {
        if (retries >= max_retries) {
            return false;
        }

        pos = atomic_load_explicit(&queue->enqueue_pos, memory_order_relaxed);
        node = (edge_mpmc_node_t*)((char*)queue->buffer + (pos & queue->mask) * node_size);
        seq = atomic_load_explicit(&node->sequence, memory_order_acquire);

        intptr_t diff = (intptr_t)seq - (intptr_t)pos;

        if (diff == 0) {
            if (atomic_compare_exchange_weak_explicit(&queue->enqueue_pos, &pos, pos + 1, memory_order_relaxed, memory_order_relaxed)) {
                break;
            }
            retries++;
        }
        else if (diff < 0) {
            return false;
        }
        else {
            retries++;
        }
    }

    memcpy(node->data, element, queue->element_size);
    atomic_store_explicit(&node->sequence, pos + 1, memory_order_release);

    return true;
}

bool edge_mpmc_queue_try_dequeue(edge_mpmc_queue_t* queue, void* out_element, size_t max_retries) {
    if (!queue) {
        return false;
    }

    size_t node_size = sizeof(edge_mpmc_node_t) + queue->element_size;
    size_t pos;
    edge_mpmc_node_t* node;
    size_t seq;
    size_t retries = 0;

    for (;;) {
        if (retries >= max_retries) {
            return false;
        }

        pos = atomic_load_explicit(&queue->dequeue_pos, memory_order_relaxed);
        node = (edge_mpmc_node_t*)((char*)queue->buffer + (pos & queue->mask) * node_size);
        seq = atomic_load_explicit(&node->sequence, memory_order_acquire);

        intptr_t diff = (intptr_t)seq - (intptr_t)(pos + 1);

        if (diff == 0) {
            if (atomic_compare_exchange_weak_explicit(&queue->dequeue_pos, &pos, pos + 1, memory_order_relaxed, memory_order_relaxed)) {
                break;
            }
            retries++;
        }
        else if (diff < 0) {
            return false;
        }
        else {
            retries++;
        }
    }

    if (out_element) {
        memcpy(out_element, node->data, queue->element_size);
    }

    atomic_store_explicit(&node->sequence, pos + queue->mask + 1, memory_order_release);

    return true;
}

size_t edge_mpmc_queue_size_approx(const edge_mpmc_queue_t* queue) {
    if (!queue) {
        return 0;
    }

    size_t enqueue = atomic_load_explicit(&queue->enqueue_pos, memory_order_relaxed);
    size_t dequeue = atomic_load_explicit(&queue->dequeue_pos, memory_order_relaxed);

    if (enqueue >= dequeue) {
        return enqueue - dequeue;
    }
    else {
        return (__SIZE_MAX__ - dequeue) + enqueue + 1;
    }
}

size_t edge_mpmc_queue_capacity(const edge_mpmc_queue_t* queue) {
    return queue ? queue->capacity : 0;
}

bool edge_mpmc_queue_empty_approx(const edge_mpmc_queue_t* queue) {
    if (!queue) {
        return true;
    }

    return edge_mpmc_queue_size_approx(queue) == 0;
}

bool edge_mpmc_queue_full_approx(const edge_mpmc_queue_t* queue) {
    if (!queue) {
        return false;
    }

    return edge_mpmc_queue_size_approx(queue) >= queue->capacity;
}