#ifndef EDGE_LIST_H
#define EDGE_LIST_H

#include <stddef.h>
#include "edge_base.h"

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct edge_allocator edge_allocator_t;

    typedef struct edge_list_node {
        void* data;
        struct edge_list_node* next;
        struct edge_list_node* prev;
    } edge_list_node_t;

    typedef struct edge_list {
        edge_list_node_t* head;
        edge_list_node_t* tail;
        size_t size;
        size_t element_size;
        const edge_allocator_t* allocator;
    } edge_list_t;

    typedef struct edge_list_iterator {
        edge_list_node_t* current;
    } edge_list_iterator_t;

    /**
     * Create a new list
     */
    edge_list_t* edge_list_create(const edge_allocator_t* allocator, size_t element_size);

    /**
     * Destroy list and free all nodes
     */
    void edge_list_destroy(edge_list_t* list);

    /**
     * Clear all elements
     */
    void edge_list_clear(edge_list_t* list);

    /**
     * Push element to front
     */
    bool edge_list_push_front(edge_list_t* list, const void* element);

    /**
     * Push element to back
     */
    bool edge_list_push_back(edge_list_t* list, const void* element);

    /**
     * Pop element from front
     */
    bool edge_list_pop_front(edge_list_t* list, void* out_element);

    /**
     * Pop element from back
     */
    bool edge_list_pop_back(edge_list_t* list, void* out_element);

    /**
     * Get front element
     */
    void* edge_list_front(const edge_list_t* list);

    /**
     * Get back element
     */
    void* edge_list_back(const edge_list_t* list);

    /**
     * Get element at index (O(n) operation)
     */
    void* edge_list_get(const edge_list_t* list, size_t index);

    /**
     * Insert element at index
     */
    bool edge_list_insert(edge_list_t* list, size_t index, const void* element);

    /**
     * Remove element at index
     */
    bool edge_list_remove(edge_list_t* list, size_t index, void* out_element);

    /**
     * Get list size
     */
    size_t edge_list_size(const edge_list_t* list);

    /**
     * Check if empty
     */
    bool edge_list_empty(const edge_list_t* list);

    /**
     * Find element using comparison function
     */
    edge_list_node_t* edge_list_find(const edge_list_t* list, const void* element, i32 (*compare)(const void*, const void*));

    /**
     * Reverse the list
     */
    void edge_list_reverse(edge_list_t* list);

    /**
     * Sort the list using merge sort
     */
    void edge_list_sort(edge_list_t* list, i32 (*compare)(const void*, const void*));

    /* Iterator functions */
    edge_list_iterator_t edge_list_begin(const edge_list_t* list);
    edge_list_iterator_t edge_list_end(const edge_list_t* list);
    bool edge_list_iterator_valid(const edge_list_iterator_t* it);
    void edge_list_iterator_next(edge_list_iterator_t* it);
    void edge_list_iterator_prev(edge_list_iterator_t* it);
    void* edge_list_iterator_get(const edge_list_iterator_t* it);

#ifdef __cplusplus
}
#endif

#endif
