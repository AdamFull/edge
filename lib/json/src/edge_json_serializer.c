#include "edge_json_internal.h"
#include <edge_allocator.h>

typedef struct {
    char* data;
    size_t length;
    size_t capacity;
    const edge_allocator_t* allocator;
} string_builder_t;

static string_builder_t* sb_create(const edge_allocator_t* allocator) {
    const edge_allocator_t* allocation_callbacks = edge_json_pick_allocator(allocator);

    string_builder_t* sb = (string_builder_t*)edge_allocator_malloc(allocation_callbacks, sizeof(string_builder_t));
    if (!sb) return NULL;

    sb->capacity = 256;
    sb->data = (char*)edge_allocator_malloc(allocation_callbacks, sb->capacity);
    if (!sb->data) {
        edge_allocator_free(allocation_callbacks, sb);
        return NULL;
    }

    sb->data[0] = '\0';
    sb->length = 0;

    sb->allocator = allocation_callbacks;

    return sb;
}

static void sb_free(string_builder_t* sb) {
    if (!sb) return;
    edge_allocator_free(sb->allocator, sb->data);
    edge_allocator_free(sb->allocator, sb);
}

static int sb_append(string_builder_t* sb, const char* str, size_t length) {
    if (sb->length + length >= sb->capacity) {
        size_t new_capacity = sb->capacity;
        while (sb->length + length >= new_capacity) {
            new_capacity *= 2;
        }

        char* new_data = (char*)edge_allocator_realloc(sb->allocator, sb->data, new_capacity);
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
        return sb_append_escaped_string(sb, value->as.string_value.data, value->as.string_value.length);

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

            if (!sb_append_escaped_string(sb, value->as.object_value.entries[i].key, strlen(value->as.object_value.entries[i].key))) {
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

    string_builder_t* sb = sb_create(value->allocator);
    if (!sb) return NULL;

    if (!stringify_value(sb, value, indent, 0)) {
        sb_free(sb);
        return NULL;
    }

    char* result = sb->data;
    edge_allocator_free(value->allocator, sb); /* Don't free data, data should be freed by user after usage */

    return result;
}