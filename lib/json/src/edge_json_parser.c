#include "edge_json_internal.h"
#include <edge_allocator.h>

#include <ctype.h>
#include <string.h>
#include <stdlib.h>

/* Parser state */
typedef struct {
    const char* json;
    size_t pos;
    size_t length;

    const edge_allocator_t* allocator;
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
    char* result = (char*)edge_allocator_malloc(parser->allocator, capacity);
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
                edge_allocator_free(parser->allocator, result);
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
                    edge_allocator_free(parser->allocator, result);
                    edge_json_set_error("Invalid unicode escape");
                    return NULL;
                }
                /* For simplicity, just copy the escape sequence */
                unescaped = '?';
                parser->pos += 3;
                break;
            }
            default:
                edge_allocator_free(parser->allocator, result);
                edge_json_set_error("Invalid escape sequence");
                return NULL;
            }

            c = unescaped;
        }

        /* Resize if needed */
        if (length >= capacity - 1) {
            capacity *= 2;
            char* new_result = (char*)edge_allocator_realloc(parser->allocator, result, capacity);
            if (!new_result) {
                edge_allocator_free(parser->allocator, result);
                return NULL;
            }
            result = new_result;
        }

        result[length++] = c;
        parser->pos++;
    }

    edge_allocator_free(parser->allocator, result);
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
    char* num_str = edge_allocator_strndup(parser->allocator, parser->json + start, parser->pos - start);
    if (!num_str) return NULL;

    double value = strtod(num_str, NULL);
    edge_allocator_free(parser->allocator, num_str);

    return edge_json_number(value, parser->allocator);
}

/* Parse array */
static edge_json_value_t* parse_array(edge_json_parser_t* parser) {
    if (parser->json[parser->pos] != '[') {
        return NULL;
    }

    parser->pos++; /* Skip '[' */
    skip_whitespace(parser);

    edge_json_value_t* array = edge_json_array(parser->allocator);
    if (!array) return NULL;

    /* Empty array */
    if (parser->pos < parser->length && parser->json[parser->pos] == ']') {
        parser->pos++;
        return array;
    }

    while (parser->pos < parser->length) {
        edge_json_value_t* element = parse_value(parser);
        if (!element) {
            edge_json_free_value(array);
            return NULL;
        }

        if (!edge_json_array_append(array, element)) {
            edge_json_free_value(element);
            edge_json_free_value(array);
            return NULL;
        }

        skip_whitespace(parser);

        if (parser->pos >= parser->length) {
            edge_json_free_value(array);
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
            edge_json_free_value(array);
            edge_json_set_error("Expected ',' or ']' in array");
            return NULL;
        }
    }

    edge_json_free_value(array);
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

    edge_json_value_t* object = edge_json_object(parser->allocator);
    if (!object) return NULL;

    /* Empty object */
    if (parser->pos < parser->length && parser->json[parser->pos] == '}') {
        parser->pos++;
        return object;
    }

    while (parser->pos < parser->length) {
        /* Parse key */
        if (parser->json[parser->pos] != '"') {
            edge_json_free_value(object);
            edge_json_set_error("Expected string key in object");
            return NULL;
        }

        size_t key_length;
        char* key = parse_string_content(parser, &key_length);
        if (!key) {
            edge_json_free_value(object);
            return NULL;
        }

        skip_whitespace(parser);

        /* Expect ':' */
        if (parser->pos >= parser->length || parser->json[parser->pos] != ':') {
            edge_allocator_free(parser->allocator, key);
            edge_json_free_value(object);
            edge_json_set_error("Expected ':' after key");
            return NULL;
        }

        parser->pos++;
        skip_whitespace(parser);

        /* Parse value */
        edge_json_value_t* value = parse_value(parser);
        if (!value) {
            edge_allocator_free(parser->allocator, key);
            edge_json_free_value(object);
            return NULL;
        }

        if (!edge_json_object_set(object, key, value)) {
            edge_allocator_free(parser->allocator, key);
            edge_json_free_value(value);
            edge_json_free_value(object);
            return NULL;
        }

        edge_allocator_free(parser->allocator, key);
        skip_whitespace(parser);

        if (parser->pos >= parser->length) {
            edge_json_free_value(object);
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
            edge_json_free_value(object);
            edge_json_set_error("Expected ',' or '}' in object");
            return NULL;
        }
    }

    edge_json_free_value(object);
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
            return edge_json_null(parser->allocator);
        }
        edge_json_set_error("Invalid literal");
        return NULL;
    }

    /* true */
    if (c == 't') {
        if (parser->pos + 4 <= parser->length &&
            memcmp(parser->json + parser->pos, "true", 4) == 0) {
            parser->pos += 4;
            return edge_json_bool(1, parser->allocator);
        }
        edge_json_set_error("Invalid literal");
        return NULL;
    }

    /* false */
    if (c == 'f') {
        if (parser->pos + 5 <= parser->length &&
            memcmp(parser->json + parser->pos, "false", 5) == 0) {
            parser->pos += 5;
            return edge_json_bool(0, parser->allocator);
        }
        edge_json_set_error("Invalid literal");
        return NULL;
    }

    /* string */
    if (c == '"') {
        size_t length;
        char* str = parse_string_content(parser, &length);
        if (!str) return NULL;
        edge_json_value_t* value = edge_json_string_len(str, length, parser->allocator);
        edge_allocator_free(parser->allocator, str);
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
edge_json_value_t* edge_json_parse(const char* json, const edge_allocator_t* allocator) {
    if (!json) return NULL;
    const edge_allocator_t* allocation_callbacks = edge_json_pick_allocator(allocator);
    return edge_json_parse_len(json, strlen(json), allocation_callbacks);
}

edge_json_value_t* edge_json_parse_len(const char* json, size_t length, const edge_allocator_t* allocator) {
    const edge_allocator_t* allocation_callbacks = edge_json_pick_allocator(allocator);

    if (!json || length == 0) {
        edge_json_set_error("Empty input");
        return NULL;
    }

    edge_json_clear_error();

    edge_json_parser_t parser = {
        .json = json,
        .pos = 0,
        .length = length,
        .allocator = allocation_callbacks
    };

    edge_json_value_t* value = parse_value(&parser);

    if (value) {
        skip_whitespace(&parser);
        if (parser.pos < parser.length) {
            edge_json_free_value(value);
            edge_json_set_error("Unexpected data after JSON value");
            return NULL;
        }
    }

    return value;
}
