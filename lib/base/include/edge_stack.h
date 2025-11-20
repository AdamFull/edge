#ifndef EDGE_STACK_H
#define EDGE_STACK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>

    typedef struct edge_allocator edge_allocator_t;

    typedef struct edge_stack {
        void* data;
        size_t size;
        size_t capacity;
        size_t element_size;
        const edge_allocator_t* allocator;
    } edge_stack_t;

    /**
     * Create a new stack
     */
    edge_stack_t* edge_stack_create(const edge_allocator_t* allocator, size_t element_size, size_t initial_capacity);

    /**
     * Destroy stack and free memory
     */
    void edge_stack_destroy(edge_stack_t* stack);

    /**
     * Clear all elements
     */
    void edge_stack_clear(edge_stack_t* stack);

    /**
     * Push element onto the stack
     */
    bool edge_stack_push(edge_stack_t* stack, const void* element);

    /**
     * Pop element from the stack
     */
    bool edge_stack_pop(edge_stack_t* stack, void* out_element);

    /**
     * Peek at top element without removing it
     */
    void* edge_stack_top(const edge_stack_t* stack);

    /**
     * Get stack size
     */
    size_t edge_stack_size(const edge_stack_t* stack);

    /**
     * Check if empty
     */
    bool edge_stack_empty(const edge_stack_t* stack);

    /**
     * Reserve capacity
     */
    bool edge_stack_reserve(edge_stack_t* stack, size_t capacity);

#ifdef __cplusplus
}
#endif

#endif
