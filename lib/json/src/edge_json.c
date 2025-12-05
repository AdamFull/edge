/*
 * libedgejson - JSON Parser and Generator Implementation
 */

#include "edge_json_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <errno.h>

#include <edge_allocator.h>

static int g_default_allocator_initialized = 0;
static char g_error_message[256];

const edge_allocator_t* edge_json_pick_allocator(const edge_allocator_t* allocator) {
    static edge_allocator_t default_allocator;
    if (g_default_allocator_initialized == 0) {
        default_allocator = edge_allocator_create_default();
        g_default_allocator_initialized = 1;
    }
    return allocator ? allocator : &default_allocator;
}

void edge_json_set_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(g_error_message, sizeof(g_error_message), format, args);
    va_end(args);
}

void edge_json_clear_error() {
    g_error_message[0] = '\0';
}

const char* edge_json_get_error() {
    return g_error_message[0] ? g_error_message : NULL;
}

const char* edge_json_version(void) {
    static char version[32];
    snprintf(version, sizeof(version), "%d.%d.%d", EDGE_JSON_VERSION_MAJOR, EDGE_JSON_VERSION_MINOR, EDGE_JSON_VERSION_PATCH);
    return version;
}

edge_json_value_t* edge_json_null(const edge_allocator_t* allocator) {
    const edge_allocator_t* allocation_callbacks = edge_json_pick_allocator(allocator);

    edge_json_value_t* value = (edge_json_value_t*)edge_allocator_malloc(allocation_callbacks, sizeof(edge_json_value_t));
    if (!value) {
        return NULL;
    }

    value->type = EDGE_JSON_TYPE_NULL;
    value->allocator = allocation_callbacks;
    return value;
}

edge_json_value_t* edge_json_bool(bool val, const edge_allocator_t* allocator) {
    const edge_allocator_t* allocation_callbacks = edge_json_pick_allocator(allocator);

    edge_json_value_t* value = (edge_json_value_t*)edge_allocator_malloc(allocation_callbacks, sizeof(edge_json_value_t));
    if (!value) {
        return NULL;
    }

    value->type = EDGE_JSON_TYPE_BOOL;
    value->as.bool_value = val ? 1 : 0;
    value->allocator = allocation_callbacks;
    return value;
}

edge_json_value_t* edge_json_number(f64 val, const edge_allocator_t* allocator) {
    const edge_allocator_t* allocation_callbacks = edge_json_pick_allocator(allocator);

    edge_json_value_t* value = (edge_json_value_t*)edge_allocator_malloc(allocation_callbacks, sizeof(edge_json_value_t));
    if (!value) {
        return NULL;
    }

    value->type = EDGE_JSON_TYPE_NUMBER;
    value->as.number_value = val;
    value->allocator = allocation_callbacks;
    return value;
}

edge_json_value_t* edge_json_int(i64 val, const edge_allocator_t* allocator) {
    const edge_allocator_t* allocation_callbacks = edge_json_pick_allocator(allocator);
    return edge_json_number((f64)val, allocation_callbacks);
}

edge_json_value_t* edge_json_string(const char* str, const edge_allocator_t* allocator) {
    const edge_allocator_t* allocation_callbacks = edge_json_pick_allocator(allocator);
    if (!str) {
        return edge_json_null(allocation_callbacks);
    }
    return edge_json_string_len(str, strlen(str), allocation_callbacks);
}

edge_json_value_t* edge_json_string_len(const char* str, size_t length, const edge_allocator_t* allocator) {
    const edge_allocator_t* allocation_callbacks = edge_json_pick_allocator(allocator);

    if (!str) {
        return edge_json_null(allocation_callbacks);
    }

    edge_json_value_t* value = (edge_json_value_t*)edge_allocator_malloc(allocation_callbacks, sizeof(edge_json_value_t));
    if (!value) {
        return NULL;
    }

    value->type = EDGE_JSON_TYPE_STRING;
    value->as.string_value = edge_string_create_from_buffer(allocation_callbacks, str, length);
    if (!value->as.string_value) {
        edge_allocator_free(allocation_callbacks, value);
        return NULL;
    }

    value->allocator = allocation_callbacks;

    return value;
}

edge_json_value_t* edge_json_array(const edge_allocator_t* allocator) {
    const edge_allocator_t* allocation_callbacks = edge_json_pick_allocator(allocator);

    edge_json_value_t* value = (edge_json_value_t*)edge_allocator_malloc(allocation_callbacks, sizeof(edge_json_value_t));
    if (!value) {
        return NULL;
    }

    value->type = EDGE_JSON_TYPE_ARRAY;
    value->as.array_value = edge_vector_create(allocation_callbacks, sizeof(edge_json_value_t*), 0);
    if (!value->as.array_value) {
        edge_allocator_free(allocation_callbacks, value);
        return NULL;
    }

    value->allocator = allocation_callbacks;

    return value;
}

edge_json_value_t* edge_json_object(const edge_allocator_t* allocator) {
    const edge_allocator_t* allocation_callbacks = edge_json_pick_allocator(allocator);

    edge_json_value_t* value = (edge_json_value_t*)edge_allocator_malloc(allocation_callbacks, sizeof(edge_json_value_t));
    if (!value) {
        return NULL;
    }

    value->type = EDGE_JSON_TYPE_OBJECT;
    value->as.object_value = edge_vector_create(allocation_callbacks, sizeof(edge_json_object_entry_t), 0);
    if (!value->as.object_value) {
        edge_allocator_free(allocation_callbacks, value);
        return NULL;
    }

    value->allocator = allocation_callbacks;

    return value;
}

void edge_json_free_value(edge_json_value_t* value) {
    if (!value) {
        return;
    }

    switch (value->type) {
    case EDGE_JSON_TYPE_STRING: {
        edge_string_destroy(value->as.string_value);
        break;
    }
    case EDGE_JSON_TYPE_ARRAY: {
        size_t size = edge_vector_size(value->as.array_value);
        for (size_t i = 0; i < size; i++) {
            edge_json_value_t** elem_ptr = (edge_json_value_t**)edge_vector_at(value->as.array_value, i);
            edge_json_free_value(*elem_ptr);
        }
        edge_vector_destroy(value->as.array_value);
        break;
    }
    case EDGE_JSON_TYPE_OBJECT: {
        size_t size = edge_vector_size(value->as.object_value);
        for (size_t i = 0; i < size; i++) {
            edge_json_object_entry_t* entry = (edge_json_object_entry_t*)edge_vector_at(value->as.object_value, i);
            edge_string_destroy(entry->key);
            edge_json_free_value(entry->value);
        }
        edge_vector_destroy(value->as.object_value);
        break;
    }
    default:
        break;
    }

    edge_allocator_free(value->allocator, value);
}

edge_json_type_t edge_json_type(const edge_json_value_t* value) {
    return value ? value->type : EDGE_JSON_TYPE_NULL;
}

bool edge_json_is_null(const edge_json_value_t* value) {
    return value && value->type == EDGE_JSON_TYPE_NULL;
}

bool edge_json_is_bool(const edge_json_value_t* value) {
    return value && value->type == EDGE_JSON_TYPE_BOOL;
}

bool edge_json_is_number(const edge_json_value_t* value) {
    return value && value->type == EDGE_JSON_TYPE_NUMBER;
}

bool edge_json_is_string(const edge_json_value_t* value) {
    return value && value->type == EDGE_JSON_TYPE_STRING;
}

bool edge_json_is_array(const edge_json_value_t* value) {
    return value && value->type == EDGE_JSON_TYPE_ARRAY;
}

bool edge_json_is_object(const edge_json_value_t* value) {
    return value && value->type == EDGE_JSON_TYPE_OBJECT;
}

bool edge_json_get_bool(const edge_json_value_t* value, bool default_value) {
    if (!value || value->type != EDGE_JSON_TYPE_BOOL) {
        return default_value;
    }
    return value->as.bool_value;
}

f64 edge_json_get_number(const edge_json_value_t* value, f64 default_value) {
    if (!value || value->type != EDGE_JSON_TYPE_NUMBER) {
        return default_value;
    }
    return value->as.number_value;
}

i64 edge_json_get_int(const edge_json_value_t* value, i64 default_value) {
    if (!value || value->type != EDGE_JSON_TYPE_NUMBER) {
        return default_value;
    }
    return (i64)value->as.number_value;
}

const char* edge_json_get_string(const edge_json_value_t* value, const char* default_value) {
    if (!value || value->type != EDGE_JSON_TYPE_STRING) {
        return default_value;
    }
    return edge_string_cstr(value->as.string_value);
}

size_t edge_json_get_string_length(const edge_json_value_t* value) {
    if (!value || value->type != EDGE_JSON_TYPE_STRING) {
        return 0;
    }
    return value->as.string_value->length;
}

size_t edge_json_array_size(const edge_json_value_t* array) {
    if (!array || array->type != EDGE_JSON_TYPE_ARRAY) {
        return 0;
    }
    return edge_vector_size(array->as.array_value);
}

edge_json_value_t* edge_json_array_get(const edge_json_value_t* array, size_t index) {
    if (!array || array->type != EDGE_JSON_TYPE_ARRAY) {
        return NULL;
    }

    edge_json_value_t** elem_ptr = (edge_json_value_t**)edge_vector_get(array->as.array_value, index);
    return elem_ptr ? *elem_ptr : NULL;
}

bool edge_json_array_append(edge_json_value_t* array, edge_json_value_t* value) {
    if (!array || array->type != EDGE_JSON_TYPE_ARRAY || !value) {
        return false;
    }

    return edge_vector_push_back(array->as.array_value, &value);
}

bool edge_json_array_insert(edge_json_value_t* array, size_t index, edge_json_value_t* value) {
    if (!array || array->type != EDGE_JSON_TYPE_ARRAY || !value) {
        return false;
    }

    return edge_vector_insert(array->as.array_value, index, &value);
}

bool edge_json_array_remove(edge_json_value_t* array, size_t index) {
    if (!array || array->type != EDGE_JSON_TYPE_ARRAY) {
        return false;
    }

    edge_json_value_t* removed_value = NULL;
    if (!edge_vector_remove(array->as.array_value, index, &removed_value)) {
        return false;
    }

    edge_json_free_value(removed_value);
    return true;
}

void edge_json_array_clear(edge_json_value_t* array) {
    if (!array || array->type != EDGE_JSON_TYPE_ARRAY) {
        return;
    }

    size_t size = edge_vector_size(array->as.array_value);
    for (size_t i = 0; i < size; i++) {
        edge_json_value_t** elem_ptr = (edge_json_value_t**)edge_vector_at(array->as.array_value, i);
        edge_json_free_value(*elem_ptr);
    }

    edge_vector_clear(array->as.array_value);
}

size_t edge_json_object_size(const edge_json_value_t* object) {
    if (!object || object->type != EDGE_JSON_TYPE_OBJECT) {
        return 0;
    }
    return edge_vector_size(object->as.object_value);
}

edge_json_value_t* edge_json_object_get(const edge_json_value_t* object, const char* key) {
    if (!object || object->type != EDGE_JSON_TYPE_OBJECT || !key) {
        return NULL;
    }

    size_t size = edge_vector_size(object->as.object_value);
    for (size_t i = 0; i < size; i++) {
        edge_json_object_entry_t* entry = (edge_json_object_entry_t*)edge_vector_at(object->as.object_value, i);
        if (edge_string_compare(entry->key, key) == 0) {
            return entry->value;
        }
    }

    return NULL;
}

bool edge_json_object_set(edge_json_value_t* object, const char* key, edge_json_value_t* value) {
    if (!object || object->type != EDGE_JSON_TYPE_OBJECT || !key || !value) {
        return false;
    }

    /* Check if key exists and replace value */
    size_t size = edge_vector_size(object->as.object_value);
    for (size_t i = 0; i < size; i++) {
        edge_json_object_entry_t* entry = (edge_json_object_entry_t*)edge_vector_at(object->as.object_value, i);
        if (edge_string_compare(entry->key, key) == 0) {
            edge_json_free_value(entry->value);
            entry->value = value;
            return true;
        }
    }

    /* Add new entry */
    edge_json_object_entry_t new_entry;
    new_entry.key = edge_string_create_from(object->allocator, key);
    if (!new_entry.key) {
        return false;
    }

    new_entry.value = value;

    if (!edge_vector_push_back(object->as.object_value, &new_entry)) {
        edge_string_destroy(new_entry.key);
        return false;
    }

    return true;
}

bool edge_json_object_remove(edge_json_value_t* object, const char* key) {
    if (!object || object->type != EDGE_JSON_TYPE_OBJECT || !key) {
        return false;
    }

    size_t size = edge_vector_size(object->as.object_value);
    for (size_t i = 0; i < size; i++) {
        edge_json_object_entry_t* entry = (edge_json_object_entry_t*)edge_vector_at(object->as.object_value, i);
        if (edge_string_compare(entry->key, key) == 0) {
            edge_string_destroy(entry->key);
            edge_json_free_value(entry->value);

            edge_vector_remove(object->as.object_value, i, NULL);
            return true;
        }
    }

    return false;
}

bool edge_json_object_has(const edge_json_value_t* object, const char* key) {
    return edge_json_object_get(object, key) != NULL;
}

void edge_json_object_clear(edge_json_value_t* object) {
    if (!object || object->type != EDGE_JSON_TYPE_OBJECT) {
        return;
    }

    size_t size = edge_vector_size(object->as.object_value);
    for (size_t i = 0; i < size; i++) {
        edge_json_object_entry_t* entry = (edge_json_object_entry_t*)edge_vector_at(object->as.object_value, i);
        edge_string_destroy(entry->key);
        edge_json_free_value(entry->value);
    }

    edge_vector_clear(object->as.object_value);
}

const char* edge_json_object_get_key(const edge_json_value_t* object, size_t index) {
    if (!object || object->type != EDGE_JSON_TYPE_OBJECT) {
        return NULL;
    }

    edge_json_object_entry_t* entry = (edge_json_object_entry_t*)edge_vector_get(object->as.object_value, index);
    return entry ? edge_string_cstr(entry->key) : NULL;
}

edge_json_value_t* edge_json_object_get_value_at(const edge_json_value_t* object, size_t index) {
    if (!object || object->type != EDGE_JSON_TYPE_OBJECT) {
        return NULL;
    }
    
    edge_json_object_entry_t* entry = (edge_json_object_entry_t*)edge_vector_get(object->as.object_value, index);
    return entry ? entry->value : NULL;
}

/* Clone */
edge_json_value_t* edge_json_clone(const edge_json_value_t* value) {
    if (!value) {
        return NULL;
    }

    switch (value->type) {
    case EDGE_JSON_TYPE_NULL:
        return edge_json_null(value->allocator);

    case EDGE_JSON_TYPE_BOOL:
        return edge_json_bool(value->as.bool_value, value->allocator);

    case EDGE_JSON_TYPE_NUMBER:
        return edge_json_number(value->as.number_value, value->allocator);

    case EDGE_JSON_TYPE_STRING:
        return edge_json_string_len(edge_string_cstr(value->as.string_value), value->as.string_value->length, value->allocator);

    case EDGE_JSON_TYPE_ARRAY: {
        edge_json_value_t* array = edge_json_array(value->allocator);
        if (!array) {
            return NULL;
        }

        size_t size = edge_vector_size(value->as.array_value);
        for (size_t i = 0; i < size; i++) {
            edge_json_value_t** elem_ptr = (edge_json_value_t**)edge_vector_at(value->as.array_value, i);
            edge_json_value_t* elem = edge_json_clone(*elem_ptr);
            if (!elem || !edge_json_array_append(array, elem)) {
                if (elem) edge_json_free_value(elem);
                edge_json_free_value(array);
                return NULL;
            }
        }

        return array;
    }

    case EDGE_JSON_TYPE_OBJECT: {
        edge_json_value_t* object = edge_json_object(value->allocator);
        if (!object) {
            return NULL;
        }

        size_t size = edge_vector_size(value->as.object_value);
        for (size_t i = 0; i < size; i++) {
            edge_json_object_entry_t* entry = (edge_json_object_entry_t*)edge_vector_at(value->as.object_value, i);
            const char* key = edge_string_cstr(entry->key);
            edge_json_value_t* val = edge_json_clone(entry->value);

            if (!val || !edge_json_object_set(object, key, val)) {
                if (val) edge_json_free_value(val);
                edge_json_free_value(object);
                return NULL;
            }
        }

        return object;
    }
    }

    return NULL;
}

/* Equals */
bool edge_json_equals(const edge_json_value_t* a, const edge_json_value_t* b) {
    if (a == b) {
        return true;
    }

    if (!a || !b) {
        return false;
    }

    if (a->type != b->type) {
        return false;
    }

    switch (a->type) {
    case EDGE_JSON_TYPE_NULL:
        return true;
    case EDGE_JSON_TYPE_BOOL:
        return a->as.bool_value == b->as.bool_value;
    case EDGE_JSON_TYPE_NUMBER:
        return a->as.number_value == b->as.number_value;
    case EDGE_JSON_TYPE_STRING:
        return edge_string_compare_string(a->as.string_value, b->as.string_value) == 0;
    case EDGE_JSON_TYPE_ARRAY: {
        size_t size_a = edge_vector_size(a->as.array_value);
        size_t size_b = edge_vector_size(b->as.array_value);
        if (size_a != size_b) {
            return false;
        }

        for (size_t i = 0; i < size_a; i++) {
            edge_json_value_t** elem_a = (edge_json_value_t**)edge_vector_at(a->as.array_value, i);
            edge_json_value_t** elem_b = (edge_json_value_t**)edge_vector_at(b->as.array_value, i);
            if (!edge_json_equals(*elem_a, *elem_b)) {
                return false;
            }
        }

        return true;
    }

    case EDGE_JSON_TYPE_OBJECT: {
        size_t size_a = edge_vector_size(a->as.object_value);
        size_t size_b = edge_vector_size(b->as.object_value);
        if (size_a != size_b) {
            return false;
        }

        for (size_t i = 0; i < size_a; i++) {
            edge_json_object_entry_t* entry_a = (edge_json_object_entry_t*)edge_vector_at(a->as.object_value, i);
            const char* key = edge_string_cstr(entry_a->key);
            edge_json_value_t* val_b = edge_json_object_get(b, key);

            if (!val_b || !edge_json_equals(entry_a->value, val_b)) {
                return false;
            }
        }

        return true;
    }
    }

    return false;
}

/* Merge */
bool edge_json_object_merge(edge_json_value_t* dest, const edge_json_value_t* source) {
    if (!dest || !source || dest->type != EDGE_JSON_TYPE_OBJECT || source->type != EDGE_JSON_TYPE_OBJECT) {
        return false;
    }

    size_t size = edge_vector_size(source->as.object_value);
    for (size_t i = 0; i < size; i++) {
        edge_json_object_entry_t* entry = (edge_json_object_entry_t*)edge_vector_at(source->as.object_value, i);
        const char* key = edge_string_cstr(entry->key);
        edge_json_value_t* value = edge_json_clone(entry->value);

        if (!value || !edge_json_object_set(dest, key, value)) {
            if (value) edge_json_free_value(value);
            return false;
        }
    }

    return true;
}

/* Builder functions */
edge_json_value_t* edge_json_build_object(const edge_allocator_t* allocator, const char* key, ...) {
    const edge_allocator_t* allocation_callbacks = edge_json_pick_allocator(allocator);

    edge_json_value_t* object = edge_json_object(allocation_callbacks);
    if (!object) {
        return NULL;
    }

    va_list args;
    va_start(args, key);

    while (key != NULL) {
        edge_json_value_t* value = va_arg(args, edge_json_value_t*);
        if (!value || !edge_json_object_set(object, key, value)) {
            if (value) edge_json_free_value(value);
            edge_json_free_value(object);
            va_end(args);
            return NULL;
        }

        key = va_arg(args, const char*);
    }

    va_end(args);
    return object;
}

edge_json_value_t* edge_json_build_array(edge_json_value_t* value, ...) {
    edge_json_value_t* array = edge_json_array(value->allocator);
    if (!array) {
        return NULL;
    }

    va_list args;
    va_start(args, value);

    while (value != NULL) {
        if (!edge_json_array_append(array, value)) {
            edge_json_free_value(value);
            edge_json_free_value(array);
            va_end(args);
            return NULL;
        }

        value = va_arg(args, edge_json_value_t*);
    }

    va_end(args);
    return array;
}
