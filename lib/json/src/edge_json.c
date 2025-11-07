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
    if (!value) return NULL;
    value->type = EDGE_JSON_TYPE_NULL;
    value->allocator = allocation_callbacks;
    return value;
}

edge_json_value_t* edge_json_bool(int val, const edge_allocator_t* allocator) {
    const edge_allocator_t* allocation_callbacks = edge_json_pick_allocator(allocator);

    edge_json_value_t* value = (edge_json_value_t*)edge_allocator_malloc(allocation_callbacks, sizeof(edge_json_value_t));
    if (!value) return NULL;
    value->type = EDGE_JSON_TYPE_BOOL;
    value->as.bool_value = val ? 1 : 0;
    value->allocator = allocation_callbacks;
    return value;
}

edge_json_value_t* edge_json_number(double val, const edge_allocator_t* allocator) {
    const edge_allocator_t* allocation_callbacks = edge_json_pick_allocator(allocator);

    edge_json_value_t* value = (edge_json_value_t*)edge_allocator_malloc(allocation_callbacks, sizeof(edge_json_value_t));
    if (!value) return NULL;
    value->type = EDGE_JSON_TYPE_NUMBER;
    value->as.number_value = val;
    value->allocator = allocation_callbacks;
    return value;
}

edge_json_value_t* edge_json_int(int64_t val, const edge_allocator_t* allocator) {
    const edge_allocator_t* allocation_callbacks = edge_json_pick_allocator(allocator);
    return edge_json_number((double)val, allocation_callbacks);
}

edge_json_value_t* edge_json_string(const char* str, const edge_allocator_t* allocator) {
    const edge_allocator_t* allocation_callbacks = edge_json_pick_allocator(allocator);
    if (!str) return edge_json_null(allocation_callbacks);
    return edge_json_string_len(str, strlen(str), allocation_callbacks);
}

edge_json_value_t* edge_json_string_len(const char* str, size_t length, const edge_allocator_t* allocator) {
    const edge_allocator_t* allocation_callbacks = edge_json_pick_allocator(allocator);

    if (!str) return edge_json_null(allocation_callbacks);

    edge_json_value_t* value = (edge_json_value_t*)edge_allocator_malloc(allocation_callbacks, sizeof(edge_json_value_t));
    if (!value) return NULL;

    value->type = EDGE_JSON_TYPE_STRING;
    value->as.string_value.data = (char*)edge_allocator_malloc(allocation_callbacks, length + 1);
    if (!value->as.string_value.data) {
        edge_allocator_free(allocation_callbacks, value);
        return NULL;
    }

    memcpy(value->as.string_value.data, str, length);
    value->as.string_value.data[length] = '\0';
    value->as.string_value.length = length;
    value->allocator = allocation_callbacks;

    return value;
}

edge_json_value_t* edge_json_array(const edge_allocator_t* allocator) {
    const edge_allocator_t* allocation_callbacks = edge_json_pick_allocator(allocator);

    edge_json_value_t* value = (edge_json_value_t*)edge_allocator_malloc(allocation_callbacks, sizeof(edge_json_value_t));
    if (!value) return NULL;

    value->type = EDGE_JSON_TYPE_ARRAY;
    value->as.array_value.elements = NULL;
    value->as.array_value.size = 0;
    value->as.array_value.capacity = 0;
    value->allocator = allocation_callbacks;

    return value;
}

edge_json_value_t* edge_json_object(const edge_allocator_t* allocator) {
    const edge_allocator_t* allocation_callbacks = edge_json_pick_allocator(allocator);

    edge_json_value_t* value = (edge_json_value_t*)edge_allocator_malloc(allocation_callbacks, sizeof(edge_json_value_t));
    if (!value) return NULL;

    value->type = EDGE_JSON_TYPE_OBJECT;
    value->as.object_value.entries = NULL;
    value->as.object_value.size = 0;
    value->as.object_value.capacity = 0;
    value->allocator = allocation_callbacks;

    return value;
}

void edge_json_free_value(edge_json_value_t* value) {
    if (!value) return;

    switch (value->type) {
    case EDGE_JSON_TYPE_STRING:
        edge_allocator_free(value->allocator, value->as.string_value.data);
        break;

    case EDGE_JSON_TYPE_ARRAY:
        for (size_t i = 0; i < value->as.array_value.size; i++) {
            edge_json_free_value(value->as.array_value.elements[i]);
        }
        edge_allocator_free(value->allocator, value->as.array_value.elements);
        break;

    case EDGE_JSON_TYPE_OBJECT:
        for (size_t i = 0; i < value->as.object_value.size; i++) {
            edge_allocator_free(value->allocator, value->as.object_value.entries[i].key);
            edge_json_free_value(value->as.object_value.entries[i].value);
        }
        edge_allocator_free(value->allocator, value->as.object_value.entries);
        break;

    default:
        break;
    }

    edge_allocator_free(value->allocator, value);
}

edge_json_type_t edge_json_type(const edge_json_value_t* value) {
    return value ? value->type : EDGE_JSON_TYPE_NULL;
}

int edge_json_is_null(const edge_json_value_t* value) {
    return value && value->type == EDGE_JSON_TYPE_NULL;
}

int edge_json_is_bool(const edge_json_value_t* value) {
    return value && value->type == EDGE_JSON_TYPE_BOOL;
}

int edge_json_is_number(const edge_json_value_t* value) {
    return value && value->type == EDGE_JSON_TYPE_NUMBER;
}

int edge_json_is_string(const edge_json_value_t* value) {
    return value && value->type == EDGE_JSON_TYPE_STRING;
}

int edge_json_is_array(const edge_json_value_t* value) {
    return value && value->type == EDGE_JSON_TYPE_ARRAY;
}

int edge_json_is_object(const edge_json_value_t* value) {
    return value && value->type == EDGE_JSON_TYPE_OBJECT;
}

int edge_json_get_bool(const edge_json_value_t* value, int default_value) {
    if (!value || value->type != EDGE_JSON_TYPE_BOOL) {
        return default_value;
    }
    return value->as.bool_value;
}

double edge_json_get_number(const edge_json_value_t* value, double default_value) {
    if (!value || value->type != EDGE_JSON_TYPE_NUMBER) {
        return default_value;
    }
    return value->as.number_value;
}

int64_t edge_json_get_int(const edge_json_value_t* value, int64_t default_value) {
    if (!value || value->type != EDGE_JSON_TYPE_NUMBER) {
        return default_value;
    }
    return (int64_t)value->as.number_value;
}

const char* edge_json_get_string(const edge_json_value_t* value, const char* default_value) {
    if (!value || value->type != EDGE_JSON_TYPE_STRING) {
        return default_value;
    }
    return value->as.string_value.data;
}

size_t edge_json_get_string_length(const edge_json_value_t* value) {
    if (!value || value->type != EDGE_JSON_TYPE_STRING) {
        return 0;
    }
    return value->as.string_value.length;
}

size_t edge_json_array_size(const edge_json_value_t* array) {
    if (!array || array->type != EDGE_JSON_TYPE_ARRAY) {
        return 0;
    }
    return array->as.array_value.size;
}

edge_json_value_t* edge_json_array_get(const edge_json_value_t* array, size_t index) {
    if (!array || array->type != EDGE_JSON_TYPE_ARRAY) {
        return NULL;
    }
    if (index >= array->as.array_value.size) {
        return NULL;
    }
    return array->as.array_value.elements[index];
}

int edge_json_array_append(edge_json_value_t* array, edge_json_value_t* value) {
    if (!array || array->type != EDGE_JSON_TYPE_ARRAY || !value) {
        return 0;
    }

    /* Resize if needed */
    if (array->as.array_value.size >= array->as.array_value.capacity) {
        size_t new_capacity = array->as.array_value.capacity == 0 ? 8 : array->as.array_value.capacity * 2;
        edge_json_value_t** new_elements = (edge_json_value_t**)edge_allocator_realloc(
            array->allocator,
            array->as.array_value.elements,
            new_capacity * sizeof(edge_json_value_t*)
        );
        if (!new_elements) return 0;

        array->as.array_value.elements = new_elements;
        array->as.array_value.capacity = new_capacity;
    }

    array->as.array_value.elements[array->as.array_value.size++] = value;
    return 1;
}

int edge_json_array_insert(edge_json_value_t* array, size_t index, edge_json_value_t* value) {
    if (!array || array->type != EDGE_JSON_TYPE_ARRAY || !value) {
        return 0;
    }

    if (index > array->as.array_value.size) {
        return 0;
    }

    if (index == array->as.array_value.size) {
        return edge_json_array_append(array, value);
    }

    /* Ensure capacity */
    if (array->as.array_value.size >= array->as.array_value.capacity) {
        size_t new_capacity = array->as.array_value.capacity == 0 ? 8 : array->as.array_value.capacity * 2;
        edge_json_value_t** new_elements = (edge_json_value_t**)edge_allocator_realloc(
            array->allocator,
            array->as.array_value.elements,
            new_capacity * sizeof(edge_json_value_t*)
        );
        if (!new_elements) return 0;

        array->as.array_value.elements = new_elements;
        array->as.array_value.capacity = new_capacity;
    }

    /* Shift elements */
    memmove(&array->as.array_value.elements[index + 1],
        &array->as.array_value.elements[index],
        (array->as.array_value.size - index) * sizeof(edge_json_value_t*));

    array->as.array_value.elements[index] = value;
    array->as.array_value.size++;

    return 1;
}

int edge_json_array_remove(edge_json_value_t* array, size_t index) {
    if (!array || array->type != EDGE_JSON_TYPE_ARRAY) {
        return 0;
    }

    if (index >= array->as.array_value.size) {
        return 0;
    }

    edge_json_free_value(array->as.array_value.elements[index]);

    /* Shift elements */
    if (index < array->as.array_value.size - 1) {
        memmove(&array->as.array_value.elements[index],
            &array->as.array_value.elements[index + 1],
            (array->as.array_value.size - index - 1) * sizeof(edge_json_value_t*));
    }

    array->as.array_value.size--;
    return 1;
}

void edge_json_array_clear(edge_json_value_t* array) {
    if (!array || array->type != EDGE_JSON_TYPE_ARRAY) {
        return;
    }

    for (size_t i = 0; i < array->as.array_value.size; i++) {
        edge_json_free_value(array->as.array_value.elements[i]);
    }

    array->as.array_value.size = 0;
}

size_t edge_json_object_size(const edge_json_value_t* object) {
    if (!object || object->type != EDGE_JSON_TYPE_OBJECT) {
        return 0;
    }
    return object->as.object_value.size;
}

edge_json_value_t* edge_json_object_get(const edge_json_value_t* object, const char* key) {
    if (!object || object->type != EDGE_JSON_TYPE_OBJECT || !key) {
        return NULL;
    }

    for (size_t i = 0; i < object->as.object_value.size; i++) {
        if (strcmp(object->as.object_value.entries[i].key, key) == 0) {
            return object->as.object_value.entries[i].value;
        }
    }

    return NULL;
}

int edge_json_object_set(edge_json_value_t* object, const char* key, edge_json_value_t* value) {
    if (!object || object->type != EDGE_JSON_TYPE_OBJECT || !key || !value) {
        return 0;
    }

    /* Check if key exists */
    for (size_t i = 0; i < object->as.object_value.size; i++) {
        if (strcmp(object->as.object_value.entries[i].key, key) == 0) {
            /* Replace existing value */
            edge_json_free_value(object->as.object_value.entries[i].value);
            object->as.object_value.entries[i].value = value;
            return 1;
        }
    }

    /* Add new entry */
    if (object->as.object_value.size >= object->as.object_value.capacity) {
        size_t new_capacity = object->as.object_value.capacity == 0 ? 8 : object->as.object_value.capacity * 2;
        edge_json_object_entry_t* new_entries = (edge_json_object_entry_t*)edge_allocator_realloc(
            object->allocator,
            object->as.object_value.entries,
            new_capacity * sizeof(edge_json_object_entry_t)
        );
        if (!new_entries) return 0;

        object->as.object_value.entries = new_entries;
        object->as.object_value.capacity = new_capacity;
    }

    char* key_copy = edge_allocator_strdup(object->allocator, key);
    if (!key_copy) return 0;

    object->as.object_value.entries[object->as.object_value.size].key = key_copy;
    object->as.object_value.entries[object->as.object_value.size].value = value;
    object->as.object_value.size++;

    return 1;
}

int edge_json_object_remove(edge_json_value_t* object, const char* key) {
    if (!object || object->type != EDGE_JSON_TYPE_OBJECT || !key) {
        return 0;
    }

    for (size_t i = 0; i < object->as.object_value.size; i++) {
        if (strcmp(object->as.object_value.entries[i].key, key) == 0) {
            edge_allocator_free(object->allocator, object->as.object_value.entries[i].key);
            edge_json_free_value(object->as.object_value.entries[i].value);

            /* Shift entries */
            if (i < object->as.object_value.size - 1) {
                memmove(&object->as.object_value.entries[i],
                    &object->as.object_value.entries[i + 1],
                    (object->as.object_value.size - i - 1) * sizeof(edge_json_object_entry_t));
            }

            object->as.object_value.size--;
            return 1;
        }
    }

    return 0;
}

int edge_json_object_has(const edge_json_value_t* object, const char* key) {
    return edge_json_object_get(object, key) != NULL;
}

void edge_json_object_clear(edge_json_value_t* object) {
    if (!object || object->type != EDGE_JSON_TYPE_OBJECT) {
        return;
    }

    for (size_t i = 0; i < object->as.object_value.size; i++) {
        edge_allocator_free(object->allocator, object->as.object_value.entries[i].key);
        edge_json_free_value(object->as.object_value.entries[i].value);
    }

    object->as.object_value.size = 0;
}

const char* edge_json_object_get_key(const edge_json_value_t* object, size_t index) {
    if (!object || object->type != EDGE_JSON_TYPE_OBJECT) {
        return NULL;
    }
    if (index >= object->as.object_value.size) {
        return NULL;
    }
    return object->as.object_value.entries[index].key;
}

edge_json_value_t* edge_json_object_get_value_at(const edge_json_value_t* object, size_t index) {
    if (!object || object->type != EDGE_JSON_TYPE_OBJECT) {
        return NULL;
    }
    if (index >= object->as.object_value.size) {
        return NULL;
    }
    return object->as.object_value.entries[index].value;
}

/* Clone */
edge_json_value_t* edge_json_clone(const edge_json_value_t* value) {
    if (!value) return NULL;

    switch (value->type) {
    case EDGE_JSON_TYPE_NULL:
        return edge_json_null(value->allocator);

    case EDGE_JSON_TYPE_BOOL:
        return edge_json_bool(value->as.bool_value, value->allocator);

    case EDGE_JSON_TYPE_NUMBER:
        return edge_json_number(value->as.number_value, value->allocator);

    case EDGE_JSON_TYPE_STRING:
        return edge_json_string_len(value->as.string_value.data, value->as.string_value.length, value->allocator);

    case EDGE_JSON_TYPE_ARRAY: {
        edge_json_value_t* array = edge_json_array(value->allocator);
        if (!array) return NULL;

        for (size_t i = 0; i < value->as.array_value.size; i++) {
            edge_json_value_t* elem = edge_json_clone(value->as.array_value.elements[i]);
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
        if (!object) return NULL;

        for (size_t i = 0; i < value->as.object_value.size; i++) {
            const char* key = value->as.object_value.entries[i].key;
            edge_json_value_t* val = edge_json_clone(value->as.object_value.entries[i].value);

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
int edge_json_equals(const edge_json_value_t* a, const edge_json_value_t* b) {
    if (a == b) return 1;
    if (!a || !b) return 0;
    if (a->type != b->type) return 0;

    switch (a->type) {
    case EDGE_JSON_TYPE_NULL:
        return 1;

    case EDGE_JSON_TYPE_BOOL:
        return a->as.bool_value == b->as.bool_value;

    case EDGE_JSON_TYPE_NUMBER:
        return a->as.number_value == b->as.number_value;

    case EDGE_JSON_TYPE_STRING:
        return a->as.string_value.length == b->as.string_value.length &&
            memcmp(a->as.string_value.data, b->as.string_value.data, a->as.string_value.length) == 0;

    case EDGE_JSON_TYPE_ARRAY: {
        if (a->as.array_value.size != b->as.array_value.size) return 0;

        for (size_t i = 0; i < a->as.array_value.size; i++) {
            if (!edge_json_equals(a->as.array_value.elements[i], b->as.array_value.elements[i])) {
                return 0;
            }
        }

        return 1;
    }

    case EDGE_JSON_TYPE_OBJECT: {
        if (a->as.object_value.size != b->as.object_value.size) return 0;

        for (size_t i = 0; i < a->as.object_value.size; i++) {
            const char* key = a->as.object_value.entries[i].key;
            edge_json_value_t* val_a = a->as.object_value.entries[i].value;
            edge_json_value_t* val_b = edge_json_object_get(b, key);

            if (!val_b || !edge_json_equals(val_a, val_b)) {
                return 0;
            }
        }

        return 1;
    }
    }

    return 0;
}

/* Merge */
int edge_json_object_merge(edge_json_value_t* dest, const edge_json_value_t* source) {
    if (!dest || !source || dest->type != EDGE_JSON_TYPE_OBJECT || source->type != EDGE_JSON_TYPE_OBJECT) {
        return 0;
    }

    for (size_t i = 0; i < source->as.object_value.size; i++) {
        const char* key = source->as.object_value.entries[i].key;
        edge_json_value_t* value = edge_json_clone(source->as.object_value.entries[i].value);

        if (!value || !edge_json_object_set(dest, key, value)) {
            if (value) edge_json_free_value(value);
            return 0;
        }
    }

    return 1;
}

/* Builder functions */
edge_json_value_t* edge_json_build_object(const edge_allocator_t* allocator, const char* key, ...) {
    const edge_allocator_t* allocation_callbacks = edge_json_pick_allocator(allocator);

    edge_json_value_t* object = edge_json_object(allocation_callbacks);
    if (!object) return NULL;

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
    if (!array) return NULL;

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