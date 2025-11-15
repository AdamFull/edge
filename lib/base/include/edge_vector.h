#ifndef EDGE_VECTOR_H
#define EDGE_VECTOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>

    typedef struct edge_allocator edge_allocator_t;

    typedef struct edge_vector {
        void* data;
        size_t size;
        size_t capacity;
        size_t element_size;
        const edge_allocator_t* allocator;
    } edge_vector_t;

    /**
     * Create a new vector
     *
     * @param allocator Memory allocator to use
     * @param element_size Size of each element in bytes
     * @param initial_capacity Initial capacity (0 for default)
     */
    edge_vector_t* edge_vector_create(const edge_allocator_t* allocator, size_t element_size, size_t initial_capacity);

    /**
     * Destroy vector and free memory
     */
    void edge_vector_destroy(edge_vector_t* vec);

    /**
     * Clear all elements (size = 0)
     */
    void edge_vector_clear(edge_vector_t* vec);

    /**
     * Push an element to the back
     */
    bool edge_vector_push_back(edge_vector_t* vec, const void* element);

    /**
     * Pop an element from the back
     *
     * @param out_element Optional pointer to store the popped element
     * @return true if successful, false if empty
     */
    bool edge_vector_pop_back(edge_vector_t* vec, void* out_element);

    /**
     * Get element at index (no bounds checking for performance)
     */
    void* edge_vector_at(const edge_vector_t* vec, size_t index);

    /**
     * Get element at index with bounds checking
     */
    void* edge_vector_get(const edge_vector_t* vec, size_t index);

    /**
     * Set element at index
     */
    bool edge_vector_set(edge_vector_t* vec, size_t index, const void* element);

    /**
     * Insert element at position
     */
    bool edge_vector_insert(edge_vector_t* vec, size_t index, const void* element);

    /**
     * Remove element at position
     *
     * @param out_element Optional pointer to store the removed element
     */
    bool edge_vector_remove(edge_vector_t* vec, size_t index, void* out_element);

    /**
     * Get pointer to front element
     */
    void* edge_vector_front(const edge_vector_t* vec);

    /**
     * Get pointer to back element
     */
    void* edge_vector_back(const edge_vector_t* vec);

    /**
     * Get raw data pointer
     */
    void* edge_vector_data(const edge_vector_t* vec);

    /**
     * Get current size
     */
    size_t edge_vector_size(const edge_vector_t* vec);

    /**
     * Get current capacity
     */
    size_t edge_vector_capacity(const edge_vector_t* vec);

    /**
     * Check if empty
     */
    bool edge_vector_empty(const edge_vector_t* vec);

    /**
     * Reserve capacity
     */
    bool edge_vector_reserve(edge_vector_t* vec, size_t capacity);

    /**
     * Resize vector to new size
     * If growing, new elements are zeroed
     */
    bool edge_vector_resize(edge_vector_t* vec, size_t new_size);

    /**
     * Shrink capacity to fit size
     */
    bool edge_vector_shrink_to_fit(edge_vector_t* vec);

    /**
     * Find element using comparison function
     * Returns index or -1 if not found
     */
    int edge_vector_find(const edge_vector_t* vec, const void* element, int (*compare)(const void*, const void*));

    /**
     * Sort vector using comparison function
     */
    void edge_vector_sort(edge_vector_t* vec, int (*compare)(const void*, const void*));

#ifdef __cplusplus
}
#endif

#endif