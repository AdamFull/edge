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


struct edge_json_context {
    edge_json_malloc_func  malloc_fn;
    edge_json_free_func    free_fn;
    edge_json_realloc_func realloc_fn;

    char error_message[256];
};

static char* edge_json_strdup(edge_json_context_t* ctx, const char* str) {
    if (!ctx || !str) return NULL;
    size_t len = strlen(str);
    char* copy = (char*)ctx->malloc_fn(len + 1);
    if (copy) {
        memcpy(copy, str, len + 1);
    }
    return copy;
}

static char* edge_json_strndup(edge_json_context_t* ctx, const char* str, size_t n) {
    if (!ctx || !str) return NULL;
    size_t len = strlen(str);
    if (n < len) len = n;
    char* copy = (char*)ctx->malloc_fn(len + 1);
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

static void edge_json_set_error(edge_json_context_t* ctx, const char* format, ...) {
    if (!ctx) return;

    va_list args;
    va_start(args, format);
    vsnprintf(ctx->error_message, sizeof(ctx->error_message), format, args);
    va_end(args);
}

static void edge_json_clear_error(edge_json_context_t* ctx) {
    if (!ctx) return;
    ctx->error_message[0] = '\0';
}

const char* edge_json_get_error(edge_json_context_t* ctx) {
    if (!ctx) return NULL;
    return ctx->error_message[0] ? ctx->error_message : NULL;
}

edge_json_context_t* edge_json_create_default(void) {
    return edge_json_create_context(malloc, free, realloc);
}

edge_json_context_t* edge_json_create_context(edge_json_malloc_func pfn_malloc, edge_json_free_func pfn_free, edge_json_realloc_func pfn_realloc) {
    if (!pfn_malloc || !pfn_free || !pfn_realloc) return NULL;

    struct edge_json_context* ctx = (struct edge_json_context*)pfn_malloc(sizeof(struct edge_json_context));
    
    ctx->malloc_fn = pfn_malloc;
    ctx->free_fn = pfn_free;
    ctx->realloc_fn = pfn_realloc;
    ctx->error_message[0] = '\0';

    return ctx;
}

void edge_json_destroy_context(edge_json_context_t* pctx) {
    if (!pctx) return;
    pctx->free_fn(pctx);
}

const char* edge_json_version(void) {
    static char version[32];
    snprintf(version, sizeof(version), "%d.%d.%d", EDGE_JSON_VERSION_MAJOR, EDGE_JSON_VERSION_MINOR, EDGE_JSON_VERSION_PATCH);
    return version;
}

edge_json_value_t* edge_json_null(edge_json_context_t* ctx) {
    if (!ctx) return NULL;
    edge_json_value_t* value = (edge_json_value_t*)ctx->malloc_fn(sizeof(edge_json_value_t));
    if (!value) return NULL;
    value->type = EDGE_JSON_TYPE_NULL;
    return value;
}

edge_json_value_t* edge_json_bool(edge_json_context_t* ctx, int val) {
    if (!ctx) return NULL;
    edge_json_value_t* value = (edge_json_value_t*)ctx->malloc_fn(sizeof(edge_json_value_t));
    if (!value) return NULL;
    value->type = EDGE_JSON_TYPE_BOOL;
    value->as.bool_value = val ? 1 : 0;
    return value;
}

edge_json_value_t* edge_json_number(edge_json_context_t* ctx, double val) {
    if (!ctx) return NULL;
    edge_json_value_t* value = (edge_json_value_t*)ctx->malloc_fn(sizeof(edge_json_value_t));
    if (!value) return NULL;
    value->type = EDGE_JSON_TYPE_NUMBER;
    value->as.number_value = val;
    return value;
}

edge_json_value_t* edge_json_int(edge_json_context_t* ctx, int64_t val) {
    if (!ctx) return NULL;
    return edge_json_number(ctx, (double)val);
}

edge_json_value_t* edge_json_string(edge_json_context_t* ctx, const char* str) {
    if (!ctx || !str) return edge_json_null(ctx);
    return edge_json_string_len(ctx, str, strlen(str));
}

edge_json_value_t* edge_json_string_len(edge_json_context_t* ctx, const char* str, size_t length) {
    if (!ctx || !str) return edge_json_null(ctx);

    edge_json_value_t* value = (edge_json_value_t*)ctx->malloc_fn(sizeof(edge_json_value_t));
    if (!value) return NULL;

    value->type = EDGE_JSON_TYPE_STRING;
    value->as.string_value.data = (char*)ctx->malloc_fn(length + 1);
    if (!value->as.string_value.data) {
        ctx->free_fn(value);
        return NULL;
    }

    memcpy(value->as.string_value.data, str, length);
    value->as.string_value.data[length] = '\0';
    value->as.string_value.length = length;

    return value;
}

edge_json_value_t* edge_json_array(edge_json_context_t* ctx) {
    if (!ctx) return NULL;
    edge_json_value_t* value = (edge_json_value_t*)ctx->malloc_fn(sizeof(edge_json_value_t));
    if (!value) return NULL;

    value->type = EDGE_JSON_TYPE_ARRAY;
    value->as.array_value.elements = NULL;
    value->as.array_value.size = 0;
    value->as.array_value.capacity = 0;

    return value;
}

edge_json_value_t* edge_json_object(edge_json_context_t* ctx) {
    if (!ctx) return NULL;
    edge_json_value_t* value = (edge_json_value_t*)ctx->malloc_fn(sizeof(edge_json_value_t));
    if (!value) return NULL;

    value->type = EDGE_JSON_TYPE_OBJECT;
    value->as.object_value.entries = NULL;
    value->as.object_value.size = 0;
    value->as.object_value.capacity = 0;

    return value;
}

void edge_json_free_value(edge_json_context_t* ctx, edge_json_value_t* value) {
    if (!value) return;

    switch (value->type) {
    case EDGE_JSON_TYPE_STRING:
        ctx->free_fn(value->as.string_value.data);
        break;

    case EDGE_JSON_TYPE_ARRAY:
        for (size_t i = 0; i < value->as.array_value.size; i++) {
            ctx->free_fn(value->as.array_value.elements[i]);
        }
        ctx->free_fn(value->as.array_value.elements);
        break;

    case EDGE_JSON_TYPE_OBJECT:
        for (size_t i = 0; i < value->as.object_value.size; i++) {
            ctx->free_fn(value->as.object_value.entries[i].key);
            ctx->free_fn(value->as.object_value.entries[i].value);
        }
        ctx->free_fn(value->as.object_value.entries);
        break;

    default:
        break;
    }

    ctx->free_fn(value);
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

int edge_json_array_append(edge_json_context_t* ctx, edge_json_value_t* array, edge_json_value_t* value) {
    if (!ctx || !array || array->type != EDGE_JSON_TYPE_ARRAY || !value) {
        return 0;
    }

    /* Resize if needed */
    if (array->as.array_value.size >= array->as.array_value.capacity) {
        size_t new_capacity = array->as.array_value.capacity == 0 ? 8 : array->as.array_value.capacity * 2;
        edge_json_value_t** new_elements = (edge_json_value_t**)ctx->realloc_fn(
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

int edge_json_array_insert(edge_json_context_t* ctx, edge_json_value_t* array, size_t index, edge_json_value_t* value) {
    if (!ctx || !array || array->type != EDGE_JSON_TYPE_ARRAY || !value) {
        return 0;
    }

    if (index > array->as.array_value.size) {
        return 0;
    }

    if (index == array->as.array_value.size) {
        return edge_json_array_append(ctx, array, value);
    }

    /* Ensure capacity */
    if (array->as.array_value.size >= array->as.array_value.capacity) {
        size_t new_capacity = array->as.array_value.capacity == 0 ? 8 : array->as.array_value.capacity * 2;
        edge_json_value_t** new_elements = (edge_json_value_t**)ctx->realloc_fn(
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

int edge_json_array_remove(edge_json_context_t* ctx, edge_json_value_t* array, size_t index) {
    if (!ctx || !array || array->type != EDGE_JSON_TYPE_ARRAY) {
        return 0;
    }

    if (index >= array->as.array_value.size) {
        return 0;
    }

    ctx->free_fn(array->as.array_value.elements[index]);

    /* Shift elements */
    if (index < array->as.array_value.size - 1) {
        memmove(&array->as.array_value.elements[index],
            &array->as.array_value.elements[index + 1],
            (array->as.array_value.size - index - 1) * sizeof(edge_json_value_t*));
    }

    array->as.array_value.size--;
    return 1;
}

void edge_json_array_clear(edge_json_context_t* ctx, edge_json_value_t* array) {
    if (!ctx || !array || array->type != EDGE_JSON_TYPE_ARRAY) {
        return;
    }

    for (size_t i = 0; i < array->as.array_value.size; i++) {
        ctx->free_fn(array->as.array_value.elements[i]);
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

int edge_json_object_set(edge_json_context_t* ctx, edge_json_value_t* object, const char* key, edge_json_value_t* value) {
    if (!ctx || !object || object->type != EDGE_JSON_TYPE_OBJECT || !key || !value) {
        return 0;
    }

    /* Check if key exists */
    for (size_t i = 0; i < object->as.object_value.size; i++) {
        if (strcmp(object->as.object_value.entries[i].key, key) == 0) {
            /* Replace existing value */
            ctx->free_fn(object->as.object_value.entries[i].value);
            object->as.object_value.entries[i].value = value;
            return 1;
        }
    }

    /* Add new entry */
    if (object->as.object_value.size >= object->as.object_value.capacity) {
        size_t new_capacity = object->as.object_value.capacity == 0 ? 8 : object->as.object_value.capacity * 2;
        edge_json_object_entry_t* new_entries = (edge_json_object_entry_t*)ctx->realloc_fn(
            object->as.object_value.entries,
            new_capacity * sizeof(edge_json_object_entry_t)
        );
        if (!new_entries) return 0;

        object->as.object_value.entries = new_entries;
        object->as.object_value.capacity = new_capacity;
    }

    char* key_copy = edge_json_strdup(ctx, key);
    if (!key_copy) return 0;

    object->as.object_value.entries[object->as.object_value.size].key = key_copy;
    object->as.object_value.entries[object->as.object_value.size].value = value;
    object->as.object_value.size++;

    return 1;
}

int edge_json_object_remove(edge_json_context_t* ctx, edge_json_value_t* object, const char* key) {
    if (!ctx || !object || object->type != EDGE_JSON_TYPE_OBJECT || !key) {
        return 0;
    }

    for (size_t i = 0; i < object->as.object_value.size; i++) {
        if (strcmp(object->as.object_value.entries[i].key, key) == 0) {
            ctx->free_fn(object->as.object_value.entries[i].key);
            ctx->free_fn(object->as.object_value.entries[i].value);

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

void edge_json_object_clear(edge_json_context_t* ctx, edge_json_value_t* object) {
    if (!ctx || !object || object->type != EDGE_JSON_TYPE_OBJECT) {
        return;
    }

    for (size_t i = 0; i < object->as.object_value.size; i++) {
        ctx->free_fn(object->as.object_value.entries[i].key);
        ctx->free_fn(object->as.object_value.entries[i].value);
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

static edge_json_value_t* parse_value(edge_json_context_t* ctx, edge_json_parser_t* parser);

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
static char* parse_string_content(edge_json_context_t* ctx, edge_json_parser_t* parser, size_t* out_length) {
    if (!ctx || parser->pos >= parser->length || parser->json[parser->pos] != '"') {
        return NULL;
    }

    parser->pos++; /* Skip opening quote */

    size_t start = parser->pos;
    size_t capacity = 32;
    size_t length = 0;
    char* result = (char*)ctx->malloc_fn(capacity);
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
                ctx->free_fn(result);
                edge_json_set_error(ctx, "Unexpected end of string");
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
                    ctx->free_fn(result);
                    edge_json_set_error(ctx, "Invalid unicode escape");
                    return NULL;
                }
                /* For simplicity, just copy the escape sequence */
                unescaped = '?';
                parser->pos += 3;
                break;
            }
            default:
                ctx->free_fn(result);
                edge_json_set_error(ctx, "Invalid escape sequence");
                return NULL;
            }

            c = unescaped;
        }

        /* Resize if needed */
        if (length >= capacity - 1) {
            capacity *= 2;
            char* new_result = (char*)ctx->realloc_fn(result, capacity);
            if (!new_result) {
                ctx->free_fn(result);
                return NULL;
            }
            result = new_result;
        }

        result[length++] = c;
        parser->pos++;
    }

    ctx->free_fn(result);
    edge_json_set_error(ctx, "Unterminated string");
    return NULL;
}

/* Parse number */
static edge_json_value_t* parse_number(edge_json_context_t* ctx, edge_json_parser_t* parser) {
    if (!ctx) return NULL;

    size_t start = parser->pos;

    /* Sign */
    if (parser->json[parser->pos] == '-') {
        parser->pos++;
    }

    /* Integer part */
    if (parser->pos >= parser->length || !isdigit(parser->json[parser->pos])) {
        edge_json_set_error(ctx, "Invalid number");
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
            edge_json_set_error(ctx, "Invalid number");
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
            edge_json_set_error(ctx, "Invalid number");
            return NULL;
        }
        while (parser->pos < parser->length && isdigit(parser->json[parser->pos])) {
            parser->pos++;
        }
    }

    /* Convert to number */
    char* num_str = edge_json_strndup(ctx, parser->json + start, parser->pos - start);
    if (!num_str) return NULL;

    double value = strtod(num_str, NULL);
    ctx->free_fn(num_str);

    return edge_json_number(ctx, value);
}

/* Parse array */
static edge_json_value_t* parse_array(edge_json_context_t* ctx, edge_json_parser_t* parser) {
    if (!ctx || parser->json[parser->pos] != '[') {
        return NULL;
    }

    parser->pos++; /* Skip '[' */
    skip_whitespace(parser);

    edge_json_value_t* array = edge_json_array(ctx);
    if (!array) return NULL;

    /* Empty array */
    if (parser->pos < parser->length && parser->json[parser->pos] == ']') {
        parser->pos++;
        return array;
    }

    while (parser->pos < parser->length) {
        edge_json_value_t* element = parse_value(ctx, parser);
        if (!element) {
            ctx->free_fn(array);
            return NULL;
        }

        if (!edge_json_array_append(ctx, array, element)) {
            ctx->free_fn(element);
            ctx->free_fn(array);
            return NULL;
        }

        skip_whitespace(parser);

        if (parser->pos >= parser->length) {
            ctx->free_fn(array);
            edge_json_set_error(ctx, "Unexpected end of array");
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
            ctx->free_fn(array);
            edge_json_set_error(ctx, "Expected ',' or ']' in array");
            return NULL;
        }
    }

    ctx->free_fn(array);
    edge_json_set_error(ctx, "Unexpected end of array");
    return NULL;
}

/* Parse object */
static edge_json_value_t* parse_object(edge_json_context_t* ctx, edge_json_parser_t* parser) {
    if (!ctx || parser->json[parser->pos] != '{') {
        return NULL;
    }

    parser->pos++; /* Skip '{' */
    skip_whitespace(parser);

    edge_json_value_t* object = edge_json_object(ctx);
    if (!object) return NULL;

    /* Empty object */
    if (parser->pos < parser->length && parser->json[parser->pos] == '}') {
        parser->pos++;
        return object;
    }

    while (parser->pos < parser->length) {
        /* Parse key */
        if (parser->json[parser->pos] != '"') {
            ctx->free_fn(object);
            edge_json_set_error(ctx, "Expected string key in object");
            return NULL;
        }

        size_t key_length;
        char* key = parse_string_content(ctx, parser, &key_length);
        if (!key) {
            ctx->free_fn(object);
            return NULL;
        }

        skip_whitespace(parser);

        /* Expect ':' */
        if (parser->pos >= parser->length || parser->json[parser->pos] != ':') {
            ctx->free_fn(key);
            ctx->free_fn(object);
            edge_json_set_error(ctx, "Expected ':' after key");
            return NULL;
        }

        parser->pos++;
        skip_whitespace(parser);

        /* Parse value */
        edge_json_value_t* value = parse_value(ctx, parser);
        if (!value) {
            ctx->free_fn(key);
            ctx->free_fn(object);
            return NULL;
        }

        if (!edge_json_object_set(ctx, object, key, value)) {
            ctx->free_fn(key);
            ctx->free_fn(value);
            ctx->free_fn(object);
            return NULL;
        }

        ctx->free_fn(key);
        skip_whitespace(parser);

        if (parser->pos >= parser->length) {
            ctx->free_fn(object);
            edge_json_set_error(ctx, "Unexpected end of object");
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
            ctx->free_fn(object);
            edge_json_set_error(ctx, "Expected ',' or '}' in object");
            return NULL;
        }
    }

    ctx->free_fn(object);
    edge_json_set_error(ctx, "Unexpected end of object");
    return NULL;
}

/* Parse value */
static edge_json_value_t* parse_value(edge_json_context_t* ctx, edge_json_parser_t* parser) {
    skip_whitespace(parser);

    if (parser->pos >= parser->length) {
        edge_json_set_error(ctx, "Unexpected end of input");
        return NULL;
    }

    char c = parser->json[parser->pos];

    /* null */
    if (c == 'n') {
        if (parser->pos + 4 <= parser->length &&
            memcmp(parser->json + parser->pos, "null", 4) == 0) {
            parser->pos += 4;
            return edge_json_null(ctx);
        }
        edge_json_set_error(ctx, "Invalid literal");
        return NULL;
    }

    /* true */
    if (c == 't') {
        if (parser->pos + 4 <= parser->length &&
            memcmp(parser->json + parser->pos, "true", 4) == 0) {
            parser->pos += 4;
            return edge_json_bool(ctx, 1);
        }
        edge_json_set_error(ctx, "Invalid literal");
        return NULL;
    }

    /* false */
    if (c == 'f') {
        if (parser->pos + 5 <= parser->length &&
            memcmp(parser->json + parser->pos, "false", 5) == 0) {
            parser->pos += 5;
            return edge_json_bool(ctx, 0);
        }
        edge_json_set_error(ctx, "Invalid literal");
        return NULL;
    }

    /* string */
    if (c == '"') {
        size_t length;
        char* str = parse_string_content(ctx, parser, &length);
        if (!str) return NULL;
        edge_json_value_t* value = edge_json_string_len(ctx, str, length);
        ctx->free_fn(str);
        return value;
    }

    /* number */
    if (c == '-' || isdigit(c)) {
        return parse_number(ctx, parser);
    }

    /* array */
    if (c == '[') {
        return parse_array(ctx, parser);
    }

    /* object */
    if (c == '{') {
        return parse_object(ctx, parser);
    }

    edge_json_set_error(ctx, "Unexpected character");
    return NULL;
}

/* Public parsing functions */
edge_json_value_t* edge_json_parse(edge_json_context_t* ctx, const char* json) {
    if (!ctx || !json) return NULL;
    return edge_json_parse_len(ctx, json, strlen(json));
}

edge_json_value_t* edge_json_parse_len(edge_json_context_t* ctx, const char* json, size_t length) {
    if (!json || length == 0) {
        edge_json_set_error(ctx, "Empty input");
        return NULL;
    }

    edge_json_clear_error(ctx);

    edge_json_parser_t parser = {
        .json = json,
        .pos = 0,
        .length = length
    };

    edge_json_value_t* value = parse_value(ctx, &parser);

    if (value) {
        skip_whitespace(&parser);
        if (parser.pos < parser.length) {
            ctx->free_fn(value);
            edge_json_set_error(ctx, "Unexpected data after JSON value");
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

static string_builder_t* sb_create(edge_json_context_t* ctx) {
    string_builder_t* sb = (string_builder_t*)ctx->malloc_fn(sizeof(string_builder_t));
    if (!sb) return NULL;

    sb->capacity = 256;
    sb->data = (char*)ctx->malloc_fn(sb->capacity);
    if (!sb->data) {
        ctx->free_fn(sb);
        return NULL;
    }

    sb->data[0] = '\0';
    sb->length = 0;

    return sb;
}

static void sb_free(edge_json_context_t* ctx, string_builder_t* sb) {
    if (!sb) return;
    ctx->free_fn(sb->data);
    ctx->free_fn(sb);
}

static int sb_append(edge_json_context_t* ctx, string_builder_t* sb, const char* str, size_t length) {
    if (sb->length + length >= sb->capacity) {
        size_t new_capacity = sb->capacity;
        while (sb->length + length >= new_capacity) {
            new_capacity *= 2;
        }

        char* new_data = (char*)ctx->realloc_fn(sb->data, new_capacity);
        if (!new_data) return 0;

        sb->data = new_data;
        sb->capacity = new_capacity;
    }

    memcpy(sb->data + sb->length, str, length);
    sb->length += length;
    sb->data[sb->length] = '\0';

    return 1;
}

static int sb_append_char(edge_json_context_t* ctx, string_builder_t* sb, char c) {
    return sb_append(ctx, sb, &c, 1);
}

static int sb_append_string(edge_json_context_t* ctx, string_builder_t* sb, const char* str) {
    return sb_append(ctx, sb, str, strlen(str));
}

/* Escape string for JSON */
static int sb_append_escaped_string(edge_json_context_t* ctx, string_builder_t* sb, const char* str, size_t length) {
    if (!sb_append_char(ctx, sb, '"')) return 0;

    for (size_t i = 0; i < length; i++) {
        char c = str[i];

        switch (c) {
        case '"':  if (!sb_append_string(ctx, sb, "\\\"")) return 0; break;
        case '\\': if (!sb_append_string(ctx, sb, "\\\\")) return 0; break;
        case '\b': if (!sb_append_string(ctx, sb, "\\b")) return 0; break;
        case '\f': if (!sb_append_string(ctx, sb, "\\f")) return 0; break;
        case '\n': if (!sb_append_string(ctx, sb, "\\n")) return 0; break;
        case '\r': if (!sb_append_string(ctx, sb, "\\r")) return 0; break;
        case '\t': if (!sb_append_string(ctx, sb, "\\t")) return 0; break;
        default:
            if ((unsigned char)c < 32) {
                /* Control character - escape as \uXXXX */
                char buf[7];
                snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
                if (!sb_append_string(ctx, sb, buf)) return 0;
            }
            else {
                if (!sb_append_char(ctx, sb, c)) return 0;
            }
            break;
        }
    }

    return sb_append_char(ctx, sb, '"');
}

/* Forward declaration */
static int stringify_value(edge_json_context_t* ctx, string_builder_t* sb, const edge_json_value_t* value,
    const char* indent, int depth);

/* Stringify with indentation */
static int stringify_indent(edge_json_context_t* ctx, string_builder_t* sb, const char* indent, int depth) {
    if (!indent) return 1;

    for (int i = 0; i < depth; i++) {
        if (!sb_append_string(ctx, sb, indent)) return 0;
    }

    return 1;
}

static int stringify_value(edge_json_context_t* ctx, string_builder_t* sb, const edge_json_value_t* value,
    const char* indent, int depth) {
    if (!value) return 0;

    switch (value->type) {
    case EDGE_JSON_TYPE_NULL:
        return sb_append_string(ctx, sb, "null");

    case EDGE_JSON_TYPE_BOOL:
        return sb_append_string(ctx, sb, value->as.bool_value ? "true" : "false");

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

        return sb_append_string(ctx, sb, buf);
    }

    case EDGE_JSON_TYPE_STRING:
        return sb_append_escaped_string(ctx, sb, value->as.string_value.data, value->as.string_value.length);

    case EDGE_JSON_TYPE_ARRAY: {
        if (!sb_append_char(ctx, sb, '[')) return 0;

        size_t size = value->as.array_value.size;

        if (size > 0 && indent) {
            if (!sb_append_char(ctx, sb, '\n')) return 0;
        }

        for (size_t i = 0; i < size; i++) {
            if (indent) {
                if (!stringify_indent(ctx, sb, indent, depth + 1)) return 0;
            }

            if (!stringify_value(ctx, sb, value->as.array_value.elements[i], indent, depth + 1)) {
                return 0;
            }

            if (i < size - 1) {
                if (!sb_append_char(ctx, sb, ',')) return 0;
            }

            if (indent) {
                if (!sb_append_char(ctx, sb, '\n')) return 0;
            }
        }

        if (size > 0 && indent) {
            if (!stringify_indent(ctx, sb, indent, depth)) return 0;
        }

        return sb_append_char(ctx, sb, ']');
    }

    case EDGE_JSON_TYPE_OBJECT: {
        if (!sb_append_char(ctx, sb, '{')) return 0;

        size_t size = value->as.object_value.size;

        if (size > 0 && indent) {
            if (!sb_append_char(ctx, sb, '\n')) return 0;
        }

        for (size_t i = 0; i < size; i++) {
            if (indent) {
                if (!stringify_indent(ctx, sb, indent, depth + 1)) return 0;
            }

            if (!sb_append_escaped_string(ctx, sb, value->as.object_value.entries[i].key, strlen(value->as.object_value.entries[i].key))) {
                return 0;
            }

            if (!sb_append_char(ctx, sb, ':')) return 0;

            if (indent) {
                if (!sb_append_char(ctx, sb, ' ')) return 0;
            }

            if (!stringify_value(ctx, sb, value->as.object_value.entries[i].value, indent, depth + 1)) {
                return 0;
            }

            if (i < size - 1) {
                if (!sb_append_char(ctx, sb, ',')) return 0;
            }

            if (indent) {
                if (!sb_append_char(ctx, sb, '\n')) return 0;
            }
        }

        if (size > 0 && indent) {
            if (!stringify_indent(ctx, sb, indent, depth)) return 0;
        }

        return sb_append_char(ctx, sb, '}');
    }
    }

    return 0;
}

char* edge_json_stringify(edge_json_context_t* ctx, const edge_json_value_t* value) {
    return edge_json_stringify_pretty(ctx, value, NULL);
}

char* edge_json_stringify_pretty(edge_json_context_t* ctx, const edge_json_value_t* value, const char* indent) {
    if (!ctx || !value) return NULL;

    string_builder_t* sb = sb_create(ctx);
    if (!sb) return NULL;

    if (!stringify_value(ctx, sb, value, indent, 0)) {
        sb_free(ctx, sb);
        return NULL;
    }

    char* result = sb->data;
    ctx->free_fn(sb); /* Don't free data */

    return result;
}

void edge_json_free_string(edge_json_context_t* ctx, char* str) {
    if (!ctx) return;
    ctx->free_fn(str);
}

/* Clone */
edge_json_value_t* edge_json_clone(edge_json_context_t* ctx, const edge_json_value_t* value) {
    if (!value) return NULL;

    switch (value->type) {
    case EDGE_JSON_TYPE_NULL:
        return edge_json_null(ctx);

    case EDGE_JSON_TYPE_BOOL:
        return edge_json_bool(ctx, value->as.bool_value);

    case EDGE_JSON_TYPE_NUMBER:
        return edge_json_number(ctx, value->as.number_value);

    case EDGE_JSON_TYPE_STRING:
        return edge_json_string_len(ctx, value->as.string_value.data, value->as.string_value.length);

    case EDGE_JSON_TYPE_ARRAY: {
        edge_json_value_t* array = edge_json_array(ctx);
        if (!array) return NULL;

        for (size_t i = 0; i < value->as.array_value.size; i++) {
            edge_json_value_t* elem = edge_json_clone(ctx, value->as.array_value.elements[i]);
            if (!elem || !edge_json_array_append(ctx, array, elem)) {
                if (elem) ctx->free_fn(elem);
                ctx->free_fn(array);
                return NULL;
            }
        }

        return array;
    }

    case EDGE_JSON_TYPE_OBJECT: {
        edge_json_value_t* object = edge_json_object(ctx);
        if (!object) return NULL;

        for (size_t i = 0; i < value->as.object_value.size; i++) {
            const char* key = value->as.object_value.entries[i].key;
            edge_json_value_t* val = edge_json_clone(ctx, value->as.object_value.entries[i].value);

            if (!val || !edge_json_object_set(ctx, object, key, val)) {
                if (val) ctx->free_fn(val);
                ctx->free_fn(object);
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
int edge_json_object_merge(edge_json_context_t* ctx, edge_json_value_t* dest, const edge_json_value_t* source) {
    if (!ctx || !dest || !source || dest->type != EDGE_JSON_TYPE_OBJECT || source->type != EDGE_JSON_TYPE_OBJECT) {
        return 0;
    }

    for (size_t i = 0; i < source->as.object_value.size; i++) {
        const char* key = source->as.object_value.entries[i].key;
        edge_json_value_t* value = edge_json_clone(ctx, source->as.object_value.entries[i].value);

        if (!value || !edge_json_object_set(ctx, dest, key, value)) {
            if (value) ctx->free_fn(value);
            return 0;
        }
    }

    return 1;
}

/* Builder functions */
edge_json_value_t* edge_json_build_object(edge_json_context_t* ctx, const char* key, ...) {
    if (!ctx) return NULL;
    edge_json_value_t* object = edge_json_object(ctx);
    if (!object) return NULL;

    va_list args;
    va_start(args, key);

    while (key != NULL) {
        edge_json_value_t* value = va_arg(args, edge_json_value_t*);
        if (!value || !edge_json_object_set(ctx, object, key, value)) {
            if (value) ctx->free_fn(value);
            ctx->free_fn(object);
            va_end(args);
            return NULL;
        }

        key = va_arg(args, const char*);
    }

    va_end(args);
    return object;
}

edge_json_value_t* edge_json_build_array(edge_json_context_t* ctx, edge_json_value_t* value, ...) {
    if (!ctx) return NULL;
    edge_json_value_t* array = edge_json_array(ctx);
    if (!array) return NULL;

    va_list args;
    va_start(args, value);

    while (value != NULL) {
        if (!edge_json_array_append(ctx, array, value)) {
            ctx->free_fn(value);
            ctx->free_fn(array);
            va_end(args);
            return NULL;
        }

        value = va_arg(args, edge_json_value_t*);
    }

    va_end(args);
    return array;
}