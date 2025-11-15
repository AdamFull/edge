#ifndef EDGE_HASHMAP_H
#define EDGE_HASHMAP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>

    typedef struct edge_allocator edge_allocator_t;

    typedef struct edge_hashmap_entry {
        void* key;
        void* value;
        size_t hash;
        struct edge_hashmap_entry* next;
    } edge_hashmap_entry_t;

    typedef struct edge_hashmap {
        edge_hashmap_entry_t** buckets;
        size_t bucket_count;
        size_t size;
        size_t key_size;
        size_t value_size;
        const edge_allocator_t* allocator;
        size_t(*hash_func)(const void* key, size_t key_size);
        int (*compare_func)(const void* key1, const void* key2, size_t key_size);
    } edge_hashmap_t;

    /**
     * Default hash function (FNV-1a)
     */
    size_t edge_hashmap_default_hash(const void* key, size_t key_size);

    /**
     * Default comparison function (memcmp)
     */
    int edge_hashmap_default_compare(const void* key1, const void* key2, size_t key_size);

    /**
     * Create a new hashmap
     */
    edge_hashmap_t* edge_hashmap_create(const edge_allocator_t* allocator,
        size_t key_size,
        size_t value_size,
        size_t initial_bucket_count);

    /**
     * Create a hashmap with custom hash and compare functions
     */
    edge_hashmap_t* edge_hashmap_create_custom(const edge_allocator_t* allocator,
        size_t key_size,
        size_t value_size,
        size_t initial_bucket_count,
        size_t(*hash_func)(const void*, size_t),
        int (*compare_func)(const void*, const void*, size_t));

    /**
     * Destroy hashmap and free all memory
     */
    void edge_hashmap_destroy(edge_hashmap_t* map);

    /**
     * Clear all entries
     */
    void edge_hashmap_clear(edge_hashmap_t* map);

    /**
     * Insert or update a key-value pair
     */
    bool edge_hashmap_insert(edge_hashmap_t* map, const void* key, const void* value);

    /**
     * Get value by key
     * Returns NULL if key not found
     */
    void* edge_hashmap_get(const edge_hashmap_t* map, const void* key);

    /**
     * Remove entry by key
     * Returns true if key was found and removed
     */
    bool edge_hashmap_remove(edge_hashmap_t* map, const void* key, void* out_value);

    /**
     * Check if key exists
     */
    bool edge_hashmap_contains(const edge_hashmap_t* map, const void* key);

    /**
     * Get hashmap size
     */
    size_t edge_hashmap_size(const edge_hashmap_t* map);

    /**
     * Check if empty
     */
    bool edge_hashmap_empty(const edge_hashmap_t* map);

    /**
     * Get load factor (size / bucket_count)
     */
    float edge_hashmap_load_factor(const edge_hashmap_t* map);

    /**
     * Rehash with new bucket count
     */
    bool edge_hashmap_rehash(edge_hashmap_t* map, size_t new_bucket_count);

    /* Iterator support */
    typedef struct edge_hashmap_iterator {
        const edge_hashmap_t* map;
        size_t bucket_index;
        edge_hashmap_entry_t* current;
    } edge_hashmap_iterator_t;

    edge_hashmap_iterator_t edge_hashmap_begin(const edge_hashmap_t* map);
    bool edge_hashmap_iterator_valid(const edge_hashmap_iterator_t* it);
    void edge_hashmap_iterator_next(edge_hashmap_iterator_t* it);
    void* edge_hashmap_iterator_key(const edge_hashmap_iterator_t* it);
    void* edge_hashmap_iterator_value(const edge_hashmap_iterator_t* it);

#ifdef __cplusplus
}
#endif

#endif