#include "edge_json_internal.h"
#include <edge_allocator.h>

#include <math.h>
#include <string.h>
#include <stdio.h>

/* Forward declaration */
static bool stringify_value(edge_string_t* sb, const edge_json_value_t* value, const char* indent, int depth);

static bool append_escaped_string(edge_string_t* sb, const char* str, size_t length) {
    if (!edge_string_append_char(sb, '"')) {
        return false;
    }

    for (size_t i = 0; i < length; i++) {
        char c = str[i];

        switch (c) {
        case '"':  if (!edge_string_append(sb, "\\\"")) return 0; break;
        case '\\': if (!edge_string_append(sb, "\\\\")) return 0; break;
        case '\b': if (!edge_string_append(sb, "\\b")) return 0; break;
        case '\f': if (!edge_string_append(sb, "\\f")) return 0; break;
        case '\n': if (!edge_string_append(sb, "\\n")) return 0; break;
        case '\r': if (!edge_string_append(sb, "\\r")) return 0; break;
        case '\t': if (!edge_string_append(sb, "\\t")) return 0; break;
        default:
            if ((unsigned char)c < 32) {
                /* Control character - escape as \uXXXX */
                char buf[7];
                snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
                if (!edge_string_append(sb, buf)) {
                    return false;
                }
            }
            else {
                if (!edge_string_append_char(sb, c)) {
                    return false;
                }
            }
            break;
        }
    }

    return edge_string_append_char(sb, '"');
}

/* Stringify with indentation */
static bool stringify_indent(edge_string_t* sb, const char* indent, int depth) {
    if (!indent) {
        return true;
    }

    for (int i = 0; i < depth; i++) {
        if (!edge_string_append(sb, indent)) {
            return false;
        }
    }

    return true;
}

static bool stringify_value(edge_string_t* sb, const edge_json_value_t* value, const char* indent, int depth) {
    if (!value) {
        return false;
    }

    switch (value->type) {
    case EDGE_JSON_TYPE_NULL:
        return edge_string_append(sb, "null");
    case EDGE_JSON_TYPE_BOOL:
        return edge_string_append(sb, value->as.bool_value ? "true" : "false");
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

        return edge_string_append(sb, buf);
    }

    case EDGE_JSON_TYPE_STRING:
        return append_escaped_string(sb, edge_string_cstr(value->as.string_value),
            value->as.string_value->length);

    case EDGE_JSON_TYPE_ARRAY: {
        if (!edge_string_append_char(sb, '[')) {
            return false;
        }

        size_t size = edge_vector_size(value->as.array_value);

        if (size > 0 && indent) {
            if (!edge_string_append_char(sb, '\n')) {
                return false;
            }
        }

        for (size_t i = 0; i < size; i++) {
            if (indent) {
                if (!stringify_indent(sb, indent, depth + 1)) {
                    return false;
                }
            }

            edge_json_value_t** elem_ptr = (edge_json_value_t**)edge_vector_at(value->as.array_value, i);
            if (!stringify_value(sb, *elem_ptr, indent, depth + 1)) {
                return false;
            }

            if (i < size - 1) {
                if (!edge_string_append_char(sb, ',')) {
                    return false;
                }
            }

            if (indent) {
                if (!edge_string_append_char(sb, '\n')) {
                    return false;
                }
            }
        }

        if (size > 0 && indent) {
            if (!stringify_indent(sb, indent, depth)) {
                return false;
            }
        }

        return edge_string_append_char(sb, ']');
    }

    case EDGE_JSON_TYPE_OBJECT: {
        if (!edge_string_append_char(sb, '{')) {
            return false;
        }

        size_t size = edge_vector_size(value->as.object_value);

        if (size > 0 && indent) {
            if (!edge_string_append_char(sb, '\n')) {
                return false;
            }
        }

        for (size_t i = 0; i < size; i++) {
            if (indent) {
                if (!stringify_indent(sb, indent, depth + 1)) {
                    return false;
                }
            }

            edge_json_object_entry_t* entry = (edge_json_object_entry_t*)edge_vector_at(value->as.object_value, i);

            if (!append_escaped_string(sb, edge_string_cstr(entry->key), entry->key->length)) {
                return false;
            }

            if (!edge_string_append_char(sb, ':')) {
                return false;
            }

            if (indent) {
                if (!edge_string_append_char(sb, ' ')) {
                    return false;
                }
            }

            if (!stringify_value(sb, entry->value, indent, depth + 1)) {
                return false;
            }

            if (i < size - 1) {
                if (!edge_string_append_char(sb, ',')) {
                    return false;
                }
            }

            if (indent) {
                if (!edge_string_append_char(sb, '\n')) {
                    return false;
                }
            }
        }

        if (size > 0 && indent) {
            if (!stringify_indent(sb, indent, depth)) {
                return false;
            }
        }

        return edge_string_append_char(sb, '}');
    }
    }

    return false;
}

char* edge_json_stringify(const edge_json_value_t* value) {
    return edge_json_stringify_pretty(value, NULL);
}

char* edge_json_stringify_pretty(const edge_json_value_t* value, const char* indent) {
    if (!value) return NULL;

    edge_string_t* sb = edge_string_create(value->allocator, 256);
    if (!sb) return NULL;

    if (!stringify_value(sb, value, indent, 0)) {
        edge_string_destroy(sb);
        return NULL;
    }

    /* Duplicate the string data before destroying the string object */
    char* result = edge_allocator_strdup(value->allocator, edge_string_cstr(sb));
    edge_string_destroy(sb);

    return result;
}
