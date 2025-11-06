/*
 * libedgejson - JSON Parser and Generator Implementation
 */

#include "edge_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <errno.h>

static edge_json_allocator_t g_allocator = {
    .malloc_fn = malloc,
    .free_fn = free,
    .realloc_fn = realloc
};

static int g_initialized = 0;
static char g_error_message[256] = { 0 };

static void* edge_json_malloc(size_t size) {
    return g_allocator.malloc_fn(size);
}

static void edge_json_free(void* ptr) {
    g_allocator.free_fn(ptr);
}

static void* edge_json_realloc(void* ptr, size_t size) {
    return g_allocator.realloc_fn(ptr, size);
}

static char* edge_json_strdup(const char* str) {
    if (!str) return NULL;
    size_t len = strlen(str);
    char* copy = (char*)edge_json_malloc(len + 1);
    if (copy) {
        memcpy(copy, str, len + 1);
    }
    return copy;
}

static char* edge_json_strndup(const char* str, size_t n) {
    if (!str) return NULL;
    size_t len = strlen(str);
    if (n < len) len = n;
    char* copy = (char*)edge_json_malloc(len + 1);
    if (copy) {
        memcpy(copy, str, len);
        copy[len] = '\0';
    }
    return copy;
}

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
};

static void edge_json_set_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(g_error_message, sizeof(g_error_message), format, args);
    va_end(args);
}

static void edge_json_clear_error(void) {
    g_error_message[0] = '\0';
}

const char* edge_json_get_error(void) {
    return g_error_message[0] ? g_error_message : NULL;
}

int edge_json_global_init(void) {
    if (g_initialized) return 1;
    g_initialized = 1;
    edge_json_clear_error();
    return 1;
}

int edge_json_global_init_allocator(const edge_json_allocator_t* allocator) {
    if (g_initialized) return 0;

    if (!allocator || !allocator->malloc_fn || !allocator->free_fn || !allocator->realloc_fn) {
        return 0;
    }

    g_allocator = *allocator;
    return edge_json_global_init();
}

void edge_json_global_cleanup(void) {
    g_initialized = 0;
    edge_json_clear_error();
}

const char* edge_json_version(void) {
    static char version[32];
    snprintf(version, sizeof(version), "%d.%d.%d", EDGE_JSON_VERSION_MAJOR, EDGE_JSON_VERSION_MINOR, EDGE_JSON_VERSION_PATCH);
    return version;
}

edge_json_value_t* edge_json_null(void) {
    edge_json_value_t* value = (edge_json_value_t*)edge_json_malloc(sizeof(edge_json_value_t));
    if (!value) return NULL;
    value->type = EDGE_JSON_TYPE_NULL;
    return value;
}

edge_json_value_t* edge_json_bool(int val) {
    edge_json_value_t* value = (edge_json_value_t*)edge_json_malloc(sizeof(edge_json_value_t));
    if (!value) return NULL;
    value->type = EDGE_JSON_TYPE_BOOL;
    value->as.bool_value = val ? 1 : 0;
    return value;
}

edge_json_value_t* edge_json_number(double val) {
    edge_json_value_t* value = (edge_json_value_t*)edge_json_malloc(sizeof(edge_json_value_t));
    if (!value) return NULL;
    value->type = EDGE_JSON_TYPE_NUMBER;
    value->as.number_value = val;
    return value;
}

edge_json_value_t* edge_json_int(int64_t val) {
    return edge_json_number((double)val);
}

edge_json_value_t* edge_json_string(const char* str) {
    if (!str) return edge_json_null();
    return edge_json_string_len(str, strlen(str));
}

edge_json_value_t* edge_json_string_len(const char* str, size_t length) {
    if (!str) return edge_json_null();

    edge_json_value_t* value = (edge_json_value_t*)edge_json_malloc(sizeof(edge_json_value_t));
    if (!value) return NULL;

    value->type = EDGE_JSON_TYPE_STRING;
    value->as.string_value.data = (char*)edge_json_malloc(length + 1);
    if (!value->as.string_value.data) {
        edge_json_free(value);
        return NULL;
    }

    memcpy(value->as.string_value.data, str, length);
    value->as.string_value.data[length] = '\0';
    value->as.string_value.length = length;

    return value;
}

edge_json_value_t* edge_json_array(void) {
    edge_json_value_t* value = (edge_json_value_t*)edge_json_malloc(sizeof(edge_json_value_t));
    if (!value) return NULL;

    value->type = EDGE_JSON_TYPE_ARRAY;
    value->as.array_value.elements = NULL;
    value->as.array_value.size = 0;
    value->as.array_value.capacity = 0;

    return value;
}

edge_json_value_t* edge_json_object(void) {
    edge_json_value_t* value = (edge_json_value_t*)edge_json_malloc(sizeof(edge_json_value_t));
    if (!value) return NULL;

    value->type = EDGE_JSON_TYPE_OBJECT;
    value->as.object_value.entries = NULL;
    value->as.object_value.size = 0;
    value->as.object_value.capacity = 0;

    return value;
}

void edge_json_free_value(edge_json_value_t* value) {
    if (!value) return;

    switch (value->type) {
    case EDGE_JSON_TYPE_STRING:
        edge_json_free(value->as.string_value.data);
        break;

    case EDGE_JSON_TYPE_ARRAY:
        for (size_t i = 0; i < value->as.array_value.size; i++) {
            edge_json_free(value->as.array_value.elements[i]);
        }
        edge_json_free(value->as.array_value.elements);
        break;

    case EDGE_JSON_TYPE_OBJECT:
        for (size_t i = 0; i < value->as.object_value.size; i++) {
            edge_json_free(value->as.object_value.entries[i].key);
            edge_json_free(value->as.object_value.entries[i].value);
        }
        edge_json_free(value->as.object_value.entries);
        break;

    default:
        break;
    }

    edge_json_free(value);
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
        edge_json_value_t** new_elements = (edge_json_value_t**)edge_json_realloc(
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
        edge_json_value_t** new_elements = (edge_json_value_t**)edge_json_realloc(
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

    edge_json_free(array->as.array_value.elements[index]);

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
        edge_json_free(array->as.array_value.elements[i]);
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
            edge_json_free(object->as.object_value.entries[i].value);
            object->as.object_value.entries[i].value = value;
            return 1;
        }
    }

    /* Add new entry */
    if (object->as.object_value.size >= object->as.object_value.capacity) {
        size_t new_capacity = object->as.object_value.capacity == 0 ? 8 : object->as.object_value.capacity * 2;
        edge_json_object_entry_t* new_entries = (edge_json_object_entry_t*)edge_json_realloc(
            object->as.object_value.entries,
            new_capacity * sizeof(edge_json_object_entry_t)
        );
        if (!new_entries) return 0;

        object->as.object_value.entries = new_entries;
        object->as.object_value.capacity = new_capacity;
    }

    char* key_copy = edge_json_strdup(key);
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
            edge_json_free(object->as.object_value.entries[i].key);
            edge_json_free(object->as.object_value.entries[i].value);

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
        edge_json_free(object->as.object_value.entries[i].key);
        edge_json_free(object->as.object_value.entries[i].value);
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

/* Parser state */
typedef struct {
    const char* json;
    size_t pos;
    size_t length;
} edge_json_parser_t;

static edge_json_value_t* parse_value(edge_json_parser_t* parser);

/* Skip whitespace */
static void skip_whitespace(edge_json_parser_t* parser) {
    while (parser->pos < parser->length) {
        char c = parser->json[parser->pos];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            parser->pos++;
        }
        else {
            break;
        }
    }
}

/* Parse string */
static char* parse_string_content(edge_json_parser_t* parser, size_t* out_length) {
    if (parser->pos >= parser->length || parser->json[parser->pos] != '"') {
        return NULL;
    }

    parser->pos++; /* Skip opening quote */

    size_t start = parser->pos;
    size_t capacity = 32;
    size_t length = 0;
    char* result = (char*)edge_json_malloc(capacity);
    if (!result) return NULL;

    while (parser->pos < parser->length) {
        char c = parser->json[parser->pos];

        if (c == '"') {
            parser->pos++; /* Skip closing quote */
            result[length] = '\0';
            if (out_length) *out_length = length;
            return result;
        }

        if (c == '\\') {
            parser->pos++;
            if (parser->pos >= parser->length) {
                edge_json_free(result);
                edge_json_set_error("Unexpected end of string");
                return NULL;
            }

            char escaped = parser->json[parser->pos];
            char unescaped;

            switch (escaped) {
            case '"':  unescaped = '"';  break;
            case '\\': unescaped = '\\'; break;
            case '/':  unescaped = '/';  break;
            case 'b':  unescaped = '\b'; break;
            case 'f':  unescaped = '\f'; break;
            case 'n':  unescaped = '\n'; break;
            case 'r':  unescaped = '\r'; break;
            case 't':  unescaped = '\t'; break;
            case 'u': {
                /* Unicode escape - simplified, only supports \uXXXX */
                parser->pos++;
                if (parser->pos + 3 >= parser->length) {
                    edge_json_free(result);
                    edge_json_set_error("Invalid unicode escape");
                    return NULL;
                }
                /* For simplicity, just copy the escape sequence */
                unescaped = '?';
                parser->pos += 3;
                break;
            }
            default:
                edge_json_free(result);
                edge_json_set_error("Invalid escape sequence");
                return NULL;
            }

            c = unescaped;
        }

        /* Resize if needed */
        if (length >= capacity - 1) {
            capacity *= 2;
            char* new_result = (char*)edge_json_realloc(result, capacity);
            if (!new_result) {
                edge_json_free(result);
                return NULL;
            }
            result = new_result;
        }

        result[length++] = c;
        parser->pos++;
    }

    edge_json_free(result);
    edge_json_set_error("Unterminated string");
    return NULL;
}

/* Parse number */
static edge_json_value_t* parse_number(edge_json_parser_t* parser) {
    size_t start = parser->pos;

    /* Sign */
    if (parser->json[parser->pos] == '-') {
        parser->pos++;
    }

    /* Integer part */
    if (parser->pos >= parser->length || !isdigit(parser->json[parser->pos])) {
        edge_json_set_error("Invalid number");
        return NULL;
    }

    if (parser->json[parser->pos] == '0') {
        parser->pos++;
    }
    else {
        while (parser->pos < parser->length && isdigit(parser->json[parser->pos])) {
            parser->pos++;
        }
    }

    /* Decimal part */
    if (parser->pos < parser->length && parser->json[parser->pos] == '.') {
        parser->pos++;
        if (parser->pos >= parser->length || !isdigit(parser->json[parser->pos])) {
            edge_json_set_error("Invalid number");
            return NULL;
        }
        while (parser->pos < parser->length && isdigit(parser->json[parser->pos])) {
            parser->pos++;
        }
    }

    /* Exponent */
    if (parser->pos < parser->length && (parser->json[parser->pos] == 'e' || parser->json[parser->pos] == 'E')) {
        parser->pos++;
        if (parser->pos < parser->length && (parser->json[parser->pos] == '+' || parser->json[parser->pos] == '-')) {
            parser->pos++;
        }
        if (parser->pos >= parser->length || !isdigit(parser->json[parser->pos])) {
            edge_json_set_error("Invalid number");
            return NULL;
        }
        while (parser->pos < parser->length && isdigit(parser->json[parser->pos])) {
            parser->pos++;
        }
    }

    /* Convert to number */
    char* num_str = edge_json_strndup(parser->json + start, parser->pos - start);
    if (!num_str) return NULL;

    double value = strtod(num_str, NULL);
    edge_json_free(num_str);

    return edge_json_number(value);
}

/* Parse array */
static edge_json_value_t* parse_array(edge_json_parser_t* parser) {
    if (parser->json[parser->pos] != '[') {
        return NULL;
    }

    parser->pos++; /* Skip '[' */
    skip_whitespace(parser);

    edge_json_value_t* array = edge_json_array();
    if (!array) return NULL;

    /* Empty array */
    if (parser->pos < parser->length && parser->json[parser->pos] == ']') {
        parser->pos++;
        return array;
    }

    while (parser->pos < parser->length) {
        edge_json_value_t* element = parse_value(parser);
        if (!element) {
            edge_json_free(array);
            return NULL;
        }

        if (!edge_json_array_append(array, element)) {
            edge_json_free(element);
            edge_json_free(array);
            return NULL;
        }

        skip_whitespace(parser);

        if (parser->pos >= parser->length) {
            edge_json_free(array);
            edge_json_set_error("Unexpected end of array");
            return NULL;
        }

        if (parser->json[parser->pos] == ']') {
            parser->pos++;
            return array;
        }

        if (parser->json[parser->pos] == ',') {
            parser->pos++;
            skip_whitespace(parser);
        }
        else {
            edge_json_free(array);
            edge_json_set_error("Expected ',' or ']' in array");
            return NULL;
        }
    }

    edge_json_free(array);
    edge_json_set_error("Unexpected end of array");
    return NULL;
}

/* Parse object */
static edge_json_value_t* parse_object(edge_json_parser_t* parser) {
    if (parser->json[parser->pos] != '{') {
        return NULL;
    }

    parser->pos++; /* Skip '{' */
    skip_whitespace(parser);

    edge_json_value_t* object = edge_json_object();
    if (!object) return NULL;

    /* Empty object */
    if (parser->pos < parser->length && parser->json[parser->pos] == '}') {
        parser->pos++;
        return object;
    }

    while (parser->pos < parser->length) {
        /* Parse key */
        if (parser->json[parser->pos] != '"') {
            edge_json_free(object);
            edge_json_set_error("Expected string key in object");
            return NULL;
        }

        size_t key_length;
        char* key = parse_string_content(parser, &key_length);
        if (!key) {
            edge_json_free(object);
            return NULL;
        }

        skip_whitespace(parser);

        /* Expect ':' */
        if (parser->pos >= parser->length || parser->json[parser->pos] != ':') {
            edge_json_free(key);
            edge_json_free(object);
            edge_json_set_error("Expected ':' after key");
            return NULL;
        }

        parser->pos++;
        skip_whitespace(parser);

        /* Parse value */
        edge_json_value_t* value = parse_value(parser);
        if (!value) {
            edge_json_free(key);
            edge_json_free(object);
            return NULL;
        }

        if (!edge_json_object_set(object, key, value)) {
            edge_json_free(key);
            edge_json_free(value);
            edge_json_free(object);
            return NULL;
        }

        edge_json_free(key);
        skip_whitespace(parser);

        if (parser->pos >= parser->length) {
            edge_json_free(object);
            edge_json_set_error("Unexpected end of object");
            return NULL;
        }

        if (parser->json[parser->pos] == '}') {
            parser->pos++;
            return object;
        }

        if (parser->json[parser->pos] == ',') {
            parser->pos++;
            skip_whitespace(parser);
        }
        else {
            edge_json_free(object);
            edge_json_set_error("Expected ',' or '}' in object");
            return NULL;
        }
    }

    edge_json_free(object);
    edge_json_set_error("Unexpected end of object");
    return NULL;
}

/* Parse value */
static edge_json_value_t* parse_value(edge_json_parser_t* parser) {
    skip_whitespace(parser);

    if (parser->pos >= parser->length) {
        edge_json_set_error("Unexpected end of input");
        return NULL;
    }

    char c = parser->json[parser->pos];

    /* null */
    if (c == 'n') {
        if (parser->pos + 4 <= parser->length &&
            memcmp(parser->json + parser->pos, "null", 4) == 0) {
            parser->pos += 4;
            return edge_json_null();
        }
        edge_json_set_error("Invalid literal");
        return NULL;
    }

    /* true */
    if (c == 't') {
        if (parser->pos + 4 <= parser->length &&
            memcmp(parser->json + parser->pos, "true", 4) == 0) {
            parser->pos += 4;
            return edge_json_bool(1);
        }
        edge_json_set_error("Invalid literal");
        return NULL;
    }

    /* false */
    if (c == 'f') {
        if (parser->pos + 5 <= parser->length &&
            memcmp(parser->json + parser->pos, "false", 5) == 0) {
            parser->pos += 5;
            return edge_json_bool(0);
        }
        edge_json_set_error("Invalid literal");
        return NULL;
    }

    /* string */
    if (c == '"') {
        size_t length;
        char* str = parse_string_content(parser, &length);
        if (!str) return NULL;
        edge_json_value_t* value = edge_json_string_len(str, length);
        edge_json_free(str);
        return value;
    }

    /* number */
    if (c == '-' || isdigit(c)) {
        return parse_number(parser);
    }

    /* array */
    if (c == '[') {
        return parse_array(parser);
    }

    /* object */
    if (c == '{') {
        return parse_object(parser);
    }

    edge_json_set_error("Unexpected character");
    return NULL;
}

/* Public parsing functions */
edge_json_value_t* edge_json_parse(const char* json) {
    if (!json) return NULL;
    return edge_json_parse_len(json, strlen(json));
}

edge_json_value_t* edge_json_parse_len(const char* json, size_t length) {
    if (!json || length == 0) {
        edge_json_set_error("Empty input");
        return NULL;
    }

    edge_json_clear_error();

    edge_json_parser_t parser = {
        .json = json,
        .pos = 0,
        .length = length
    };

    edge_json_value_t* value = parse_value(&parser);

    if (value) {
        skip_whitespace(&parser);
        if (parser.pos < parser.length) {
            edge_json_free(value);
            edge_json_set_error("Unexpected data after JSON value");
            return NULL;
        }
    }

    return value;
}

typedef struct {
    char* data;
    size_t length;
    size_t capacity;
} string_builder_t;

static string_builder_t* sb_create(void) {
    string_builder_t* sb = (string_builder_t*)edge_json_malloc(sizeof(string_builder_t));
    if (!sb) return NULL;

    sb->capacity = 256;
    sb->data = (char*)edge_json_malloc(sb->capacity);
    if (!sb->data) {
        edge_json_free(sb);
        return NULL;
    }

    sb->data[0] = '\0';
    sb->length = 0;

    return sb;
}

static void sb_free(string_builder_t* sb) {
    if (!sb) return;
    edge_json_free(sb->data);
    edge_json_free(sb);
}

static int sb_append(string_builder_t* sb, const char* str, size_t length) {
    if (sb->length + length >= sb->capacity) {
        size_t new_capacity = sb->capacity;
        while (sb->length + length >= new_capacity) {
            new_capacity *= 2;
        }

        char* new_data = (char*)edge_json_realloc(sb->data, new_capacity);
        if (!new_data) return 0;

        sb->data = new_data;
        sb->capacity = new_capacity;
    }

    memcpy(sb->data + sb->length, str, length);
    sb->length += length;
    sb->data[sb->length] = '\0';

    return 1;
}

static int sb_append_char(string_builder_t* sb, char c) {
    return sb_append(sb, &c, 1);
}

static int sb_append_string(string_builder_t* sb, const char* str) {
    return sb_append(sb, str, strlen(str));
}

/* Escape string for JSON */
static int sb_append_escaped_string(string_builder_t* sb, const char* str, size_t length) {
    if (!sb_append_char(sb, '"')) return 0;

    for (size_t i = 0; i < length; i++) {
        char c = str[i];

        switch (c) {
        case '"':  if (!sb_append_string(sb, "\\\"")) return 0; break;
        case '\\': if (!sb_append_string(sb, "\\\\")) return 0; break;
        case '\b': if (!sb_append_string(sb, "\\b")) return 0; break;
        case '\f': if (!sb_append_string(sb, "\\f")) return 0; break;
        case '\n': if (!sb_append_string(sb, "\\n")) return 0; break;
        case '\r': if (!sb_append_string(sb, "\\r")) return 0; break;
        case '\t': if (!sb_append_string(sb, "\\t")) return 0; break;
        default:
            if ((unsigned char)c < 32) {
                /* Control character - escape as \uXXXX */
                char buf[7];
                snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
                if (!sb_append_string(sb, buf)) return 0;
            }
            else {
                if (!sb_append_char(sb, c)) return 0;
            }
            break;
        }
    }

    return sb_append_char(sb, '"');
}

/* Forward declaration */
static int stringify_value(string_builder_t* sb, const edge_json_value_t* value,
    const char* indent, int depth);

/* Stringify with indentation */
static int stringify_indent(string_builder_t* sb, const char* indent, int depth) {
    if (!indent) return 1;

    for (int i = 0; i < depth; i++) {
        if (!sb_append_string(sb, indent)) return 0;
    }

    return 1;
}

static int stringify_value(string_builder_t* sb, const edge_json_value_t* value,
    const char* indent, int depth) {
    if (!value) return 0;

    switch (value->type) {
    case EDGE_JSON_TYPE_NULL:
        return sb_append_string(sb, "null");

    case EDGE_JSON_TYPE_BOOL:
        return sb_append_string(sb, value->as.bool_value ? "true" : "false");

    case EDGE_JSON_TYPE_NUMBER: {
        char buf[64];
        double num = value->as.number_value;

        /* Check if it's an integer */
        if (floor(num) == num && fabs(num) < 1e15) {
            snprintf(buf, sizeof(buf), "%.0f", num);
        }
        else {
            snprintf(buf, sizeof(buf), "%.17g", num);
        }

        return sb_append_string(sb, buf);
    }

    case EDGE_JSON_TYPE_STRING:
        return sb_append_escaped_string(sb, value->as.string_value.data,
            value->as.string_value.length);

    case EDGE_JSON_TYPE_ARRAY: {
        if (!sb_append_char(sb, '[')) return 0;

        size_t size = value->as.array_value.size;

        if (size > 0 && indent) {
            if (!sb_append_char(sb, '\n')) return 0;
        }

        for (size_t i = 0; i < size; i++) {
            if (indent) {
                if (!stringify_indent(sb, indent, depth + 1)) return 0;
            }

            if (!stringify_value(sb, value->as.array_value.elements[i], indent, depth + 1)) {
                return 0;
            }

            if (i < size - 1) {
                if (!sb_append_char(sb, ',')) return 0;
            }

            if (indent) {
                if (!sb_append_char(sb, '\n')) return 0;
            }
        }

        if (size > 0 && indent) {
            if (!stringify_indent(sb, indent, depth)) return 0;
        }

        return sb_append_char(sb, ']');
    }

    case EDGE_JSON_TYPE_OBJECT: {
        if (!sb_append_char(sb, '{')) return 0;

        size_t size = value->as.object_value.size;

        if (size > 0 && indent) {
            if (!sb_append_char(sb, '\n')) return 0;
        }

        for (size_t i = 0; i < size; i++) {
            if (indent) {
                if (!stringify_indent(sb, indent, depth + 1)) return 0;
            }

            if (!sb_append_escaped_string(sb, value->as.object_value.entries[i].key,
                strlen(value->as.object_value.entries[i].key))) {
                return 0;
            }

            if (!sb_append_char(sb, ':')) return 0;

            if (indent) {
                if (!sb_append_char(sb, ' ')) return 0;
            }

            if (!stringify_value(sb, value->as.object_value.entries[i].value, indent, depth + 1)) {
                return 0;
            }

            if (i < size - 1) {
                if (!sb_append_char(sb, ',')) return 0;
            }

            if (indent) {
                if (!sb_append_char(sb, '\n')) return 0;
            }
        }

        if (size > 0 && indent) {
            if (!stringify_indent(sb, indent, depth)) return 0;
        }

        return sb_append_char(sb, '}');
    }
    }

    return 0;
}

char* edge_json_stringify(const edge_json_value_t* value) {
    return edge_json_stringify_pretty(value, NULL);
}

char* edge_json_stringify_pretty(const edge_json_value_t* value, const char* indent) {
    if (!value) return NULL;

    string_builder_t* sb = sb_create();
    if (!sb) return NULL;

    if (!stringify_value(sb, value, indent, 0)) {
        sb_free(sb);
        return NULL;
    }

    char* result = sb->data;
    edge_json_free(sb); /* Don't free data */

    return result;
}

void edge_json_free_string(char* str) {
    edge_json_free(str);
}

/* Clone */
edge_json_value_t* edge_json_clone(const edge_json_value_t* value) {
    if (!value) return NULL;

    switch (value->type) {
    case EDGE_JSON_TYPE_NULL:
        return edge_json_null();

    case EDGE_JSON_TYPE_BOOL:
        return edge_json_bool(value->as.bool_value);

    case EDGE_JSON_TYPE_NUMBER:
        return edge_json_number(value->as.number_value);

    case EDGE_JSON_TYPE_STRING:
        return edge_json_string_len(value->as.string_value.data, value->as.string_value.length);

    case EDGE_JSON_TYPE_ARRAY: {
        edge_json_value_t* array = edge_json_array();
        if (!array) return NULL;

        for (size_t i = 0; i < value->as.array_value.size; i++) {
            edge_json_value_t* elem = edge_json_clone(value->as.array_value.elements[i]);
            if (!elem || !edge_json_array_append(array, elem)) {
                if (elem) edge_json_free(elem);
                edge_json_free(array);
                return NULL;
            }
        }

        return array;
    }

    case EDGE_JSON_TYPE_OBJECT: {
        edge_json_value_t* object = edge_json_object();
        if (!object) return NULL;

        for (size_t i = 0; i < value->as.object_value.size; i++) {
            const char* key = value->as.object_value.entries[i].key;
            edge_json_value_t* val = edge_json_clone(value->as.object_value.entries[i].value);

            if (!val || !edge_json_object_set(object, key, val)) {
                if (val) edge_json_free(val);
                edge_json_free(object);
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
            memcmp(a->as.string_value.data, b->as.string_value.data,
                a->as.string_value.length) == 0;

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
            if (value) edge_json_free(value);
            return 0;
        }
    }

    return 1;
}

/* Builder functions */
edge_json_value_t* edge_json_build_object(const char* key, ...) {
    edge_json_value_t* object = edge_json_object();
    if (!object) return NULL;

    va_list args;
    va_start(args, key);

    while (key != NULL) {
        edge_json_value_t* value = va_arg(args, edge_json_value_t*);
        if (!value || !edge_json_object_set(object, key, value)) {
            if (value) edge_json_free(value);
            edge_json_free(object);
            va_end(args);
            return NULL;
        }

        key = va_arg(args, const char*);
    }

    va_end(args);
    return object;
}

edge_json_value_t* edge_json_build_array(edge_json_value_t* value, ...) {
    edge_json_value_t* array = edge_json_array();
    if (!array) return NULL;

    va_list args;
    va_start(args, value);

    while (value != NULL) {
        if (!edge_json_array_append(array, value)) {
            edge_json_free(value);
            edge_json_free(array);
            va_end(args);
            return NULL;
        }

        value = va_arg(args, edge_json_value_t*);
    }

    va_end(args);
    return array;
}