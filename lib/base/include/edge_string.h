#ifndef EDGE_STRING_H
#define EDGE_STRING_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>

	typedef struct edge_allocator edge_allocator_t;

    typedef struct edge_string {
        char* data;
        size_t length;
        size_t capacity;
        const edge_allocator_t* allocator;
    } edge_string_t;

    /**
     * Create a new string with initial capacity
     */
    edge_string_t* edge_string_create(const edge_allocator_t* allocator, size_t initial_capacity);

    /**
     * Create a string from a C string
     */
    edge_string_t* edge_string_create_from(const edge_allocator_t* allocator, const char* str);

    /**
     * Create a string from a buffer with length
     */
    edge_string_t* edge_string_create_from_buffer(const edge_allocator_t* allocator, const char* buffer, size_t length);

    /**
     * Destroy a string and free its memory
     */
    void edge_string_destroy(edge_string_t* str);

    /**
     * Clear the string contents (length = 0)
     */
    void edge_string_clear(edge_string_t* str);

    /**
     * Append a C string to the edge string
     */
    bool edge_string_append(edge_string_t* str, const char* text);

    /**
     * Append a buffer with length to the edge string
     */
    bool edge_string_append_buffer(edge_string_t* str, const char* buffer, size_t length);

    /**
     * Append a single character
     */
    bool edge_string_append_char(edge_string_t* str, char c);

    /**
     * Append another edge string
     */
    bool edge_string_append_string(edge_string_t* dest, const edge_string_t* src);

    /**
     * Insert text at position
     */
    bool edge_string_insert(edge_string_t* str, size_t pos, const char* text);

    /**
     * Remove characters from position with length
     */
    bool edge_string_remove(edge_string_t* str, size_t pos, size_t length);

    /**
     * Get C string pointer (null-terminated)
     */
    const char* edge_string_cstr(const edge_string_t* str);

    /**
     * Compare with C string
     */
    int edge_string_compare(const edge_string_t* str, const char* other);

    /**
     * Compare two edge strings
     */
    int edge_string_compare_string(const edge_string_t* str1, const edge_string_t* str2);

    /**
     * Find substring, returns position or -1 if not found
     */
    int edge_string_find(const edge_string_t* str, const char* needle);

    /**
     * Reserve capacity
     */
    bool edge_string_reserve(edge_string_t* str, size_t capacity);

    /**
     * Shrink capacity to fit length
     */
    bool edge_string_shrink_to_fit(edge_string_t* str);

    /**
     * Duplicate the string
     */
    edge_string_t* edge_string_duplicate(const edge_string_t* str);

#ifdef __cplusplus
}
#endif

#endif
