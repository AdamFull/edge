#ifndef EDGE_QUEUE_H
#define EDGE_QUEUE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>

    typedef struct edge_allocator edge_allocator_t;

    typedef struct edge_queue {
        void* data;
        size_t head;
        size_t tail;
        size_t size;
        size_t capacity;
        size_t element_size;
        const edge_allocator_t* allocator;
    } edge_queue_t;

    /**
     * Create a new queue with circular buffer implementation
     */
    edge_queue_t* edge_queue_create(const edge_allocator_t* allocator, size_t element_size, size_t initial_capacity);

    /**
     * Destroy queue and free memory
     */
    void edge_queue_destroy(edge_queue_t* queue);

    /**
     * Clear all elements
     */
    void edge_queue_clear(edge_queue_t* queue);

    /**
     * Enqueue element at the back
     */
    bool edge_queue_enqueue(edge_queue_t* queue, const void* element);

    /**
     * Dequeue element from the front
     */
    bool edge_queue_dequeue(edge_queue_t* queue, void* out_element);

    /**
     * Peek at front element without removing it
     */
    void* edge_queue_front(const edge_queue_t* queue);

    /**
     * Peek at back element without removing it
     */
    void* edge_queue_back(const edge_queue_t* queue);

    /**
     * Get queue size
     */
    size_t edge_queue_size(const edge_queue_t* queue);

    /**
     * Check if empty
     */
    bool edge_queue_empty(const edge_queue_t* queue);

    /**
     * Reserve capacity
     */
    bool edge_queue_reserve(edge_queue_t* queue, size_t capacity);

#ifdef __cplusplus
}
#endif

#endif