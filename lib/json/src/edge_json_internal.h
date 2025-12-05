#ifndef EDGE_JSON_INTERNAL_H
#define EDGE_JSON_INTERNAL_H

#include "edge_json.h"
#include "edge_string.h"
#include "edge_vector.h"

typedef struct edge_json_object_entry {
    edge_string_t* key;
    edge_json_value_t* value;
} edge_json_object_entry_t;

struct edge_json_value {
    edge_json_type_t type;
    union {
        bool bool_value;
        f64 number_value;
        edge_string_t* string_value;
        edge_vector_t* array_value;
        edge_vector_t* object_value;
    } as;

    const edge_allocator_t* allocator;
};

const edge_allocator_t* edge_json_pick_allocator(const edge_allocator_t* allocator);
void edge_json_set_error(const char* format, ...);
void edge_json_clear_error();

#endif /* EDGE_JSON_INTERNAL_H */
