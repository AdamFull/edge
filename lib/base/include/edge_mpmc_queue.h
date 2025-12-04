#ifndef EDGE_MPMC_QUEUE_H
#define EDGE_MPMC_QUEUE_H

#include <stddef.h>
#include <stdatomic.h>
#include "edge_base.h"

#ifdef __cplusplus
extern "C" {
#endif

	typedef struct edge_allocator edge_allocator_t;

    typedef struct edge_mpmc_node {
        _Atomic(size_t)sequence;
        char data[];
    } edge_mpmc_node_t;

    typedef struct edge_mpmc_queue {
        edge_mpmc_node_t* buffer;
        size_t capacity;
        size_t element_size;
        size_t mask;

        _Alignas(64) _Atomic(size_t) enqueue_pos;
        _Alignas(64) _Atomic(size_t) dequeue_pos;
        _Alignas(64) const edge_allocator_t* allocator;
    } edge_mpmc_queue_t;

    /**
     * Create a queue
     *
     * @param allocator Memory allocator to use
     * @param element_size Size of each element in bytes
     * @param capacity Maximum number of elements (must be power of 2)
     * @return Pointer to created queue, or NULL on failure
     * 
     */
    edge_mpmc_queue_t* edge_mpmc_queue_create(const edge_allocator_t* allocator, size_t element_size, size_t capacity);

    /**
     * Destroy the queue and free memory
     */
    void edge_mpmc_queue_destroy(edge_mpmc_queue_t* queue);

    /**
     * Enqueue an element
     *
     * @param queue The queue
     * @param element Pointer to element to enqueue
     * @return true if successful, false if queue is full
     */
    bool edge_mpmc_queue_enqueue(edge_mpmc_queue_t* queue, const void* element);

    /**
     * Dequeue an element
     *
     * @param queue The queue
     * @param out_element Pointer to store dequeued element (nullable)
     * @return true if successful, false if queue is empty
     */
    bool edge_mpmc_queue_dequeue(edge_mpmc_queue_t* queue, void* out_element);

    /**
     * Try to enqueue with limited retry attempts
     *
     * @param queue The queue
     * @param element Pointer to element to enqueue
     * @param max_retries Maximum number of retry attempts
     * @return true if successful, false if failed after retries
     */
    bool edge_mpmc_queue_try_enqueue(edge_mpmc_queue_t* queue, const void* element, size_t max_retries);

    /**
     * Try to dequeue with limited retry attempts
     *
     * @param queue The queue
     * @param out_element Pointer to store dequeued element (nullable)
     * @param max_retries Maximum number of retry attempts
     * @return true if successful, false if failed after retries
     */
    bool edge_mpmc_queue_try_dequeue(edge_mpmc_queue_t* queue, void* out_element, size_t max_retries);

    /**
     * Get approximate size of queue
     *
     * @param queue The queue
     * @return Approximate number of elements in queue
     */
    size_t edge_mpmc_queue_size_approx(const edge_mpmc_queue_t* queue);

    /**
     * Get capacity of queue
     *
     * @param queue The queue
     * @return Maximum capacity
     */
    size_t edge_mpmc_queue_capacity(const edge_mpmc_queue_t* queue);

    /**
     * Check if queue is approximately empty
     * Note: Result may be stale immediately after return in concurrent context
     */
    bool edge_mpmc_queue_empty_approx(const edge_mpmc_queue_t* queue);

    /**
     * Check if queue is approximately full
     * Note: Result may be stale immediately after return in concurrent context
     */
    bool edge_mpmc_queue_full_approx(const edge_mpmc_queue_t* queue);

#ifdef __cplusplus
}
#endif

#endif