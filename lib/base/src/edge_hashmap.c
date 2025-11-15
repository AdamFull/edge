#include "edge_hashmap.h"
#include "edge_allocator.h"

#include <string.h>

#define EDGE_HASHMAP_DEFAULT_BUCKET_COUNT 16
#define EDGE_HASHMAP_MAX_LOAD_FACTOR 0.75f

size_t edge_hashmap_default_hash(const void* key, size_t key_size) {
    /* FNV-1a hash */
    const unsigned char* data = (const unsigned char*)key;
    size_t hash = 2166136261u;

    for (size_t i = 0; i < key_size; i++) {
        hash ^= data[i];
        hash *= 16777619u;
    }

    return hash;
}

int edge_hashmap_default_compare(const void* key1, const void* key2, size_t key_size) {
    return memcmp(key1, key2, key_size);
}

static edge_hashmap_entry_t* create_entry(
    const edge_allocator_t* allocator, size_t key_size, size_t value_size,
    const void* key, const void* value, size_t hash) {
    edge_hashmap_entry_t* entry = (edge_hashmap_entry_t*)edge_allocator_malloc(allocator, sizeof(edge_hashmap_entry_t));
    if (!entry) {
        return NULL;
    }

    entry->key = edge_allocator_malloc(allocator, key_size);
    if (!entry->key) {
        edge_allocator_free(allocator, entry);
        return NULL;
    }

    entry->value = edge_allocator_malloc(allocator, value_size);
    if (!entry->value) {
        edge_allocator_free(allocator, entry->key);
        edge_allocator_free(allocator, entry);
        return NULL;
    }

    memcpy(entry->key, key, key_size);
    memcpy(entry->value, value, value_size);
    entry->hash = hash;
    entry->next = NULL;

    return entry;
}

static void destroy_entry(const edge_allocator_t* allocator, edge_hashmap_entry_t* entry) {
    if (!entry) {
        return;
    }

    if (entry->key) edge_allocator_free(allocator, entry->key);
    if (entry->value) edge_allocator_free(allocator, entry->value);
    edge_allocator_free(allocator, entry);
}

edge_hashmap_t* edge_hashmap_create(const edge_allocator_t* allocator,
    size_t key_size, size_t value_size, size_t initial_bucket_count) {
    return edge_hashmap_create_custom(allocator, key_size, value_size, initial_bucket_count, edge_hashmap_default_hash, edge_hashmap_default_compare);
}

edge_hashmap_t* edge_hashmap_create_custom(const edge_allocator_t* allocator,
    size_t key_size, size_t value_size, size_t initial_bucket_count,
    size_t(*hash_func)(const void*, size_t), int (*compare_func)(const void*, const void*, size_t)) {
    if (!allocator || key_size == 0 || value_size == 0) {
        return NULL;
    }

    edge_hashmap_t* map = (edge_hashmap_t*)edge_allocator_malloc(allocator, sizeof(edge_hashmap_t));
    if (!map) {
        return NULL;
    }

    if (initial_bucket_count == 0) {
        initial_bucket_count = EDGE_HASHMAP_DEFAULT_BUCKET_COUNT;
    }

    map->buckets = (edge_hashmap_entry_t**)edge_allocator_calloc(allocator, initial_bucket_count, sizeof(edge_hashmap_entry_t*));
    if (!map->buckets) {
        edge_allocator_free(allocator, map);
        return NULL;
    }

    map->bucket_count = initial_bucket_count;
    map->size = 0;
    map->key_size = key_size;
    map->value_size = value_size;
    map->allocator = allocator;
    map->hash_func = hash_func ? hash_func : edge_hashmap_default_hash;
    map->compare_func = compare_func ? compare_func : edge_hashmap_default_compare;

    return map;
}

void edge_hashmap_destroy(edge_hashmap_t* map) {
    if (!map) {
        return;
    }

    edge_hashmap_clear(map);

    if (map->buckets) {
        edge_allocator_free(map->allocator, map->buckets);
    }
    edge_allocator_free(map->allocator, map);
}

void edge_hashmap_clear(edge_hashmap_t* map) {
    if (!map) {
        return;
    }

    for (size_t i = 0; i < map->bucket_count; i++) {
        edge_hashmap_entry_t* entry = map->buckets[i];
        while (entry) {
            edge_hashmap_entry_t* next = entry->next;
            destroy_entry(map->allocator, entry);
            entry = next;
        }
        map->buckets[i] = NULL;
    }

    map->size = 0;
}

bool edge_hashmap_rehash(edge_hashmap_t* map, size_t new_bucket_count) {
    if (!map || new_bucket_count == 0) {
        return false;
    }

    edge_hashmap_entry_t** new_buckets = (edge_hashmap_entry_t**)edge_allocator_calloc(
        map->allocator, new_bucket_count, sizeof(edge_hashmap_entry_t*));
    if (!new_buckets) {
        return false;
    }

    /* Rehash all entries */
    for (size_t i = 0; i < map->bucket_count; i++) {
        edge_hashmap_entry_t* entry = map->buckets[i];
        while (entry) {
            edge_hashmap_entry_t* next = entry->next;

            size_t bucket_index = entry->hash % new_bucket_count;
            entry->next = new_buckets[bucket_index];
            new_buckets[bucket_index] = entry;

            entry = next;
        }
    }

    edge_allocator_free(map->allocator, map->buckets);
    map->buckets = new_buckets;
    map->bucket_count = new_bucket_count;

    return true;
}

bool edge_hashmap_insert(edge_hashmap_t* map, const void* key, const void* value) {
    if (!map || !key || !value) {
        return false;
    }

    /* Check load factor and rehash if needed */
    if (edge_hashmap_load_factor(map) >= EDGE_HASHMAP_MAX_LOAD_FACTOR) {
        edge_hashmap_rehash(map, map->bucket_count * 2);
    }

    size_t hash = map->hash_func(key, map->key_size);
    size_t bucket_index = hash % map->bucket_count;

    /* Check if key already exists */
    edge_hashmap_entry_t* entry = map->buckets[bucket_index];
    while (entry) {
        if (entry->hash == hash && map->compare_func(entry->key, key, map->key_size) == 0) {
            /* Update existing value */
            memcpy(entry->value, value, map->value_size);
            return true;
        }
        entry = entry->next;
    }

    /* Create new entry */
    edge_hashmap_entry_t* new_entry = create_entry(map->allocator, map->key_size, map->value_size, key, value, hash);
    if (!new_entry) {
        return false;
    }

    new_entry->next = map->buckets[bucket_index];
    map->buckets[bucket_index] = new_entry;
    map->size++;

    return true;
}

void* edge_hashmap_get(const edge_hashmap_t* map, const void* key) {
    if (!map || !key) {
        return NULL;
    }

    size_t hash = map->hash_func(key, map->key_size);
    size_t bucket_index = hash % map->bucket_count;

    edge_hashmap_entry_t* entry = map->buckets[bucket_index];
    while (entry) {
        if (entry->hash == hash && map->compare_func(entry->key, key, map->key_size) == 0) {
            return entry->value;
        }
        entry = entry->next;
    }

    return NULL;
}

bool edge_hashmap_remove(edge_hashmap_t* map, const void* key, void* out_value) {
    if (!map || !key) {
        return false;
    }

    size_t hash = map->hash_func(key, map->key_size);
    size_t bucket_index = hash % map->bucket_count;

    edge_hashmap_entry_t* entry = map->buckets[bucket_index];
    edge_hashmap_entry_t* prev = NULL;

    while (entry) {
        if (entry->hash == hash && map->compare_func(entry->key, key, map->key_size) == 0) {
            if (out_value) {
                memcpy(out_value, entry->value, map->value_size);
            }

            if (prev) {
                prev->next = entry->next;
            }
            else {
                map->buckets[bucket_index] = entry->next;
            }

            destroy_entry(map->allocator, entry);
            map->size--;

            return true;
        }

        prev = entry;
        entry = entry->next;
    }

    return false;
}

bool edge_hashmap_contains(const edge_hashmap_t* map, const void* key) {
    return edge_hashmap_get(map, key) != NULL;
}

size_t edge_hashmap_size(const edge_hashmap_t* map) {
    return map ? map->size : 0;
}

bool edge_hashmap_empty(const edge_hashmap_t* map) {
    return !map || map->size == 0;
}

float edge_hashmap_load_factor(const edge_hashmap_t* map) {
    if (!map || map->bucket_count == 0) {
        return 0.0f;
    }
    return (float)map->size / (float)map->bucket_count;
}

edge_hashmap_iterator_t edge_hashmap_begin(const edge_hashmap_t* map) {
    edge_hashmap_iterator_t it;
    it.map = map;
    it.bucket_index = 0;
    it.current = NULL;

    if (map) {
        /* Find first non-empty bucket */
        for (size_t i = 0; i < map->bucket_count; i++) {
            if (map->buckets[i]) {
                it.bucket_index = i;
                it.current = map->buckets[i];
                break;
            }
        }
    }

    return it;
}

bool edge_hashmap_iterator_valid(const edge_hashmap_iterator_t* it) {
    return it && it->current != NULL;
}

void edge_hashmap_iterator_next(edge_hashmap_iterator_t* it) {
    if (!it || !it->map || !it->current) {
        return;
    }

    /* Try next in chain */
    if (it->current->next) {
        it->current = it->current->next;
        return;
    }

    /* Find next non-empty bucket */
    for (size_t i = it->bucket_index + 1; i < it->map->bucket_count; i++) {
        if (it->map->buckets[i]) {
            it->bucket_index = i;
            it->current = it->map->buckets[i];
            return;
        }
    }

    /* End of iteration */
    it->current = NULL;
}

void* edge_hashmap_iterator_key(const edge_hashmap_iterator_t* it) {
    if (!it || !it->current) {
        return NULL;
    }
    return it->current->key;
}

void* edge_hashmap_iterator_value(const edge_hashmap_iterator_t* it) {
    if (!it || !it->current) {
        return NULL;
    }
    return it->current->value;
}