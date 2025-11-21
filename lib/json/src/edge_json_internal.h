#ifndef EDGE_JSON_INTERNAL_H
#define EDGE_JSON_INTERNAL_H

#include "edge_json.h"

typedef struct edge_json_object_entry {
    char* key;
    edge_json_value_t* value;
} edge_json_object_entry_t;

typedef struct edge_json_array_data {
    edge_json_value_t** elements;
    size_t size;
    size_t capacity;
} edge_json_array_data_t;

typedef struct edge_json_object_data {
    edge_json_object_entry_t* entries;
    size_t size;
    size_t capacity;
} edge_json_object_data_t;

struct edge_json_value {
    edge_json_type_t type;
    union {
        int bool_value;
        double number_value;
        struct {
            char* data;
            size_t length;
        } string_value;
        edge_json_array_data_t array_value;
        edge_json_object_data_t object_value;
    } as;

    const edge_allocator_t* allocator;
};

const edge_allocator_t* edge_json_pick_allocator(const edge_allocator_t* allocator);
void edge_json_set_error(const char* format, ...);
void edge_json_clear_error();

#endif /* EDGE_JSON_INTERNAL_H */
