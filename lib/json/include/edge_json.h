/*
 * libedgejson - JSON Parser and Generator for C
 *
 * A lightweight JSON library with:
 * - Full JSON parsing and generation
 * - Custom memory allocators
 * - Clean C11 API
 * - No external dependencies
 *
 * License: MIT
 */

#ifndef EDGE_JSON_H
#define EDGE_JSON_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#define EDGE_JSON_VERSION_MAJOR 1
#define EDGE_JSON_VERSION_MINOR 0
#define EDGE_JSON_VERSION_PATCH 0

typedef void* (*edge_json_malloc_func)(size_t size);
typedef void (*edge_json_free_func)(void* ptr);
typedef void* (*edge_json_realloc_func)(void* ptr, size_t size);

typedef struct edge_json_context edge_json_context_t;

typedef enum {
    EDGE_JSON_TYPE_NULL,
    EDGE_JSON_TYPE_BOOL,
    EDGE_JSON_TYPE_NUMBER,
    EDGE_JSON_TYPE_STRING,
    EDGE_JSON_TYPE_ARRAY,
    EDGE_JSON_TYPE_OBJECT
} edge_json_type_t;

typedef struct edge_json_value edge_json_value_t;

/**
 * Initialize library with default allocator (malloc/free)
 *
 * @return edge_json_context_t
 */
edge_json_context_t* edge_json_create_default(void);

/**
 * Initialize library with custom allocator
 *
 * @return edge_json_context_t
 */
edge_json_context_t* edge_json_create_context(edge_json_malloc_func pfn_malloc, edge_json_free_func pfn_free, edge_json_realloc_func pfn_realloc);

/**
 * Cleanup library
 */
void edge_json_destroy_context(edge_json_context_t* pctx);

/**
 * Get library version
 *
 * @return Version string
 */
const char* edge_json_version(void);

/**
 * Create a null value
 *
 * @return JSON value
 */
edge_json_value_t* edge_json_null(edge_json_context_t* ctx);

/**
 * Create a boolean value
 *
 * @param value Boolean value (0 = false, non-zero = true)
 * @return JSON value
 */
edge_json_value_t* edge_json_bool(edge_json_context_t* ctx, int value);

/**
 * Create a number value
 *
 * @param value Number value
 * @return JSON value
 */
edge_json_value_t* edge_json_number(edge_json_context_t* ctx, double value);

/**
 * Create an integer number value
 *
 * @param value Integer value
 * @return JSON value
 */
edge_json_value_t* edge_json_int(edge_json_context_t* ctx, int64_t value);

/**
 * Create a string value
 * The string is copied internally
 *
 * @param value String value (null-terminated)
 * @return JSON value
 */
edge_json_value_t* edge_json_string(edge_json_context_t* ctx, const char* value);

/**
 * Create a string value with length
 *
 * @param value String value
 * @param length String length
 * @return JSON value
 */
edge_json_value_t* edge_json_string_len(edge_json_context_t* ctx, const char* value, size_t length);

/**
 * Create an empty array
 *
 * @return JSON array
 */
edge_json_value_t* edge_json_array(edge_json_context_t* ctx);

/**
 * Create an empty object
 *
 * @return JSON object
 */
edge_json_value_t* edge_json_object(edge_json_context_t* ctx);

/**
 * Free a JSON value and all its children
 *
 * @param value JSON value to free
 */
void edge_json_free_value(edge_json_context_t* ctx, edge_json_value_t* value);

/**
 * Get the type of a JSON value
 *
 * @param value JSON value
 * @return Type of the value
 */
edge_json_type_t edge_json_type(const edge_json_value_t* value);

/**
 * Check if value is null
 */
int edge_json_is_null(const edge_json_value_t* value);

/**
 * Check if value is boolean
 */
int edge_json_is_bool(const edge_json_value_t* value);

/**
 * Check if value is number
 */
int edge_json_is_number(const edge_json_value_t* value);

/**
 * Check if value is string
 */
int edge_json_is_string(const edge_json_value_t* value);

/**
 * Check if value is array
 */
int edge_json_is_array(const edge_json_value_t* value);

/**
 * Check if value is object
 */
int edge_json_is_object(const edge_json_value_t* value);

/**
 * Get boolean value
 *
 * @param value JSON value
 * @param default_value Default value if not boolean
 * @return Boolean value (0 = false, 1 = true)
 */
int edge_json_get_bool(const edge_json_value_t* value, int default_value);

/**
 * Get number value
 *
 * @param value JSON value
 * @param default_value Default value if not number
 * @return Number value
 */
double edge_json_get_number(const edge_json_value_t* value, double default_value);

/**
 * Get integer value
 *
 * @param value JSON value
 * @param default_value Default value if not number
 * @return Integer value
 */
int64_t edge_json_get_int(const edge_json_value_t* value, int64_t default_value);

/**
 * Get string value
 *
 * @param value JSON value
 * @param default_value Default value if not string
 * @return String value (do not free)
 */
const char* edge_json_get_string(const edge_json_value_t* value, const char* default_value);

/**
 * Get string length
 *
 * @param value JSON value
 * @return String length, or 0 if not string
 */
size_t edge_json_get_string_length(const edge_json_value_t* value);

/**
 * Get array size
 *
 * @param array JSON array
 * @return Number of elements, or 0 if not array
 */
size_t edge_json_array_size(const edge_json_value_t* array);

/**
 * Get element at index
 *
 * @param array JSON array
 * @param index Element index
 * @return JSON value, or NULL if out of bounds
 */
edge_json_value_t* edge_json_array_get(const edge_json_value_t* array, size_t index);

/**
 * Append element to array
 * The array takes ownership of the value
 *
 * @param array JSON array
 * @param value Value to append
 * @return 1 on success, 0 on failure
 */
int edge_json_array_append(edge_json_context_t* ctx, edge_json_value_t* array, edge_json_value_t* value);

/**
 * Insert element at index
 *
 * @param array JSON array
 * @param index Insert position
 * @param value Value to insert
 * @return 1 on success, 0 on failure
 */
int edge_json_array_insert(edge_json_context_t* ctx, edge_json_value_t* array, size_t index, edge_json_value_t* value);

/**
 * Remove element at index
 *
 * @param array JSON array
 * @param index Element index
 * @return 1 on success, 0 on failure
 */
int edge_json_array_remove(edge_json_context_t* ctx, edge_json_value_t* array, size_t index);

/**
 * Clear all elements from array
 *
 * @param array JSON array
 */
void edge_json_array_clear(edge_json_context_t* ctx, edge_json_value_t* array);

/**
 * Get number of keys in object
 *
 * @param object JSON object
 * @return Number of keys, or 0 if not object
 */
size_t edge_json_object_size(const edge_json_value_t* object);

/**
 * Get value by key
 *
 * @param object JSON object
 * @param key Key name
 * @return JSON value, or NULL if not found
 */
edge_json_value_t* edge_json_object_get(const edge_json_value_t* object, const char* key);

/**
 * Set value by key
 * The object takes ownership of the value
 * If key exists, old value is replaced
 *
 * @param object JSON object
 * @param key Key name
 * @param value Value to set
 * @return 1 on success, 0 on failure
 */
int edge_json_object_set(edge_json_context_t* ctx, edge_json_value_t* object, const char* key, edge_json_value_t* value);

/**
 * Remove key from object
 *
 * @param object JSON object
 * @param key Key name
 * @return 1 on success, 0 if key not found
 */
int edge_json_object_remove(edge_json_context_t* ctx, edge_json_value_t* object, const char* key);

/**
 * Check if object has key
 *
 * @param object JSON object
 * @param key Key name
 * @return 1 if key exists, 0 otherwise
 */
int edge_json_object_has(const edge_json_value_t* object, const char* key);

/**
 * Clear all keys from object
 *
 * @param object JSON object
 */
void edge_json_object_clear(edge_json_context_t* ctx, edge_json_value_t* object);

/**
 * Get key at index
 *
 * @param object JSON object
 * @param index Key index
 * @return Key name, or NULL if out of bounds
 */
const char* edge_json_object_get_key(const edge_json_value_t* object, size_t index);

/**
 * Get value at index
 *
 * @param object JSON object
 * @param index Value index
 * @return JSON value, or NULL if out of bounds
 */
edge_json_value_t* edge_json_object_get_value_at(const edge_json_value_t* object, size_t index);

/**
 * Parse JSON from string
 *
 * @param json JSON string
 * @return JSON value, or NULL on error
 */
edge_json_value_t* edge_json_parse(edge_json_context_t* ctx, const char* json);

/**
 * Parse JSON from string with length
 *
 * @param json JSON string
 * @param length String length
 * @return JSON value, or NULL on error
 */
edge_json_value_t* edge_json_parse_len(edge_json_context_t* ctx, const char* json, size_t length);

/**
 * Get last parse error message
 *
 * @return Error message, or NULL if no error
 */
const char* edge_json_get_error(edge_json_context_t* ctx);

/**
 * Serialize JSON to string
 * Returned string must be freed with edge_json_free_string()
 *
 * @param value JSON value
 * @return JSON string, or NULL on error
 */
char* edge_json_stringify(edge_json_context_t* ctx, const edge_json_value_t* value);

/**
 * Serialize JSON to string with formatting
 *
 * @param value JSON value
 * @param indent Indentation string (e.g., "  " or "\t")
 * @return JSON string, or NULL on error
 */
char* edge_json_stringify_pretty(edge_json_context_t* ctx, const edge_json_value_t* value, const char* indent);

/**
 * Free string returned by edge_json_stringify
 *
 * @param str String to free
 */
void edge_json_free_string(edge_json_context_t* ctx, char* str);

/**
 * Clone a JSON value (deep copy)
 *
 * @param value JSON value to clone
 * @return Cloned value, or NULL on error
 */
edge_json_value_t* edge_json_clone(edge_json_context_t* ctx, const edge_json_value_t* value);

/**
 * Compare two JSON values for equality
 *
 * @param a First value
 * @param b Second value
 * @return 1 if equal, 0 otherwise
 */
int edge_json_equals(const edge_json_value_t* a, const edge_json_value_t* b);

/**
 * Merge two objects (shallow merge)
 * Keys from 'source' are added to 'dest'
 * Existing keys in 'dest' are overwritten
 *
 * @param dest Destination object
 * @param source Source object
 * @return 1 on success, 0 on failure
 */
int edge_json_object_merge(edge_json_context_t* ctx, edge_json_value_t* dest, const edge_json_value_t* source);

/**
 * Create object and set values in one call
 * Usage: edge_json_build_object("name", edge_json_string("John"), "age", edge_json_int(30), NULL)
 * Must be terminated with NULL
 */
edge_json_value_t* edge_json_build_object(edge_json_context_t* ctx, const char* key, ...);

/**
 * Create array from values
 * Usage: edge_json_build_array(edge_json_int(1), edge_json_int(2), edge_json_int(3), NULL)
 * Must be terminated with NULL
 */
edge_json_value_t* edge_json_build_array(edge_json_context_t* ctx, edge_json_value_t* value, ...);

#ifdef __cplusplus
}
#endif

#endif /* EDGE_JSON_H */