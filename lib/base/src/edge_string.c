#include "edge_string.h"
#include "edge_allocator.h"

#include <string.h>
#include <stdio.h>

#define EDGE_STRING_DEFAULT_CAPACITY 16

edge_string_t* edge_string_create(const edge_allocator_t* allocator, size_t initial_capacity) {
    if (!allocator) {
        return NULL;
    }

    edge_string_t* str = (edge_string_t*)edge_allocator_malloc(allocator, sizeof(edge_string_t));
    if (!str) {
        return NULL;
    }

    if (initial_capacity < EDGE_STRING_DEFAULT_CAPACITY) {
        initial_capacity = EDGE_STRING_DEFAULT_CAPACITY;
    }

    str->data = (char*)edge_allocator_malloc(allocator, initial_capacity);
    if (!str->data) {
        edge_allocator_free(allocator, str);
        return NULL;
    }

    str->data[0] = '\0';
    str->length = 0;
    str->capacity = initial_capacity;
    str->allocator = allocator;

    return str;
}

edge_string_t* edge_string_create_from(const edge_allocator_t* allocator, const char* cstr) {
    if (!cstr) {
        return edge_string_create(allocator, EDGE_STRING_DEFAULT_CAPACITY);
    }

    size_t len = strlen(cstr);
    edge_string_t* str = edge_string_create(allocator, len + 1);
    if (!str) return NULL;

    memcpy(str->data, cstr, len + 1);
    str->length = len;

    return str;
}

edge_string_t* edge_string_create_from_buffer(const edge_allocator_t* allocator, const char* buffer, size_t length) {
    if (!buffer) {
        return edge_string_create(allocator, EDGE_STRING_DEFAULT_CAPACITY);
    }

    edge_string_t* str = edge_string_create(allocator, length + 1);
    if (!str) {
        return NULL;
    }

    memcpy(str->data, buffer, length);
    str->data[length] = '\0';
    str->length = length;

    return str;
}

void edge_string_destroy(edge_string_t* str) {
    if (!str) {
        return;
    }

    if (str->data) {
        edge_allocator_free(str->allocator, str->data);
    }
    edge_allocator_free(str->allocator, str);
}

void edge_string_clear(edge_string_t* str) {
    if (!str) {
        return;
    }

    str->length = 0;
    if (str->data) {
        str->data[0] = '\0';
    }
}

bool edge_string_reserve(edge_string_t* str, size_t capacity) {
    if (!str) {
        return false;
    }

    if (capacity <= str->capacity) {
        return true;
    }

    char* new_data = (char*)edge_allocator_realloc(str->allocator, str->data, capacity);
    if (!new_data) {
        return false;
    }

    str->data = new_data;
    str->capacity = capacity;

    return true;
}

bool edge_string_append(edge_string_t* str, const char* text) {
    if (!str || !text) {
        return false;
    }

    size_t text_len = strlen(text);
    if (text_len == 0) {
        return true;
    }

    size_t required = str->length + text_len + 1;
    if (required > str->capacity) {
        size_t new_capacity = str->capacity * 2;
        while (new_capacity < required) {
            new_capacity *= 2;
        }
        if (!edge_string_reserve(str, new_capacity)) {
            return false;
        }
    }

    memcpy(str->data + str->length, text, text_len + 1);
    str->length += text_len;

    return true;
}

bool edge_string_append_buffer(edge_string_t* str, const char* buffer, size_t length) {
    if (!str || !buffer || length == 0) {
        return false;
    }

    size_t required = str->length + length + 1;
    if (required > str->capacity) {
        size_t new_capacity = str->capacity * 2;
        while (new_capacity < required) {
            new_capacity *= 2;
        }
        if (!edge_string_reserve(str, new_capacity)) {
            return false;
        }
    }

    memcpy(str->data + str->length, buffer, length);
    str->length += length;
    str->data[str->length] = '\0';

    return true;
}

bool edge_string_append_char(edge_string_t* str, char c) {
    if (!str) {
        return false;
    }

    size_t required = str->length + 2;
    if (required > str->capacity) {
        if (!edge_string_reserve(str, str->capacity * 2)) {
            return false;
        }
    }

    str->data[str->length] = c;
    str->length++;
    str->data[str->length] = '\0';

    return true;
}

bool edge_string_append_string(edge_string_t* dest, const edge_string_t* src) {
    if (!dest || !src) {
        return false;
    }
    return edge_string_append_buffer(dest, src->data, src->length);
}

bool edge_string_insert(edge_string_t* str, size_t pos, const char* text) {
    if (!str || !text || pos > str->length) {
        return false;
    }

    size_t text_len = strlen(text);
    if (text_len == 0) {
        return true;
    }

    size_t required = str->length + text_len + 1;
    if (required > str->capacity) {
        size_t new_capacity = str->capacity * 2;
        while (new_capacity < required) {
            new_capacity *= 2;
        }
        if (!edge_string_reserve(str, new_capacity)) {
            return false;
        }
    }

    /* Move existing data to make room */
    memmove(str->data + pos + text_len, str->data + pos, str->length - pos + 1);

    /* Insert new text */
    memcpy(str->data + pos, text, text_len);
    str->length += text_len;

    return true;
}

bool edge_string_remove(edge_string_t* str, size_t pos, size_t length) {
    if (!str || pos >= str->length) {
        return false;
    }

    if (pos + length > str->length) {
        length = str->length - pos;
    }

    memmove(str->data + pos, str->data + pos + length, str->length - pos - length + 1);
    str->length -= length;

    return true;
}

const char* edge_string_cstr(const edge_string_t* str) {
    return str ? str->data : NULL;
}

int edge_string_compare(const edge_string_t* str, const char* other) {
    if (!str || !str->data) {
        return other ? -1 : 0;
    }

    if (!other) {
        return 1;
    }

    return strcmp(str->data, other);
}

int edge_string_compare_string(const edge_string_t* str1, const edge_string_t* str2) {
    if (!str1 || !str1->data) {
        return (str2 && str2->data) ? -1 : 0;
    }

    if (!str2 || !str2->data) {
        return 1;
    }

    return strcmp(str1->data, str2->data);
}

int edge_string_find(const edge_string_t* str, const char* needle) {
    if (!str || !str->data || !needle) {
        return -1;
    }

    char* found = strstr(str->data, needle);
    if (!found) {
        return -1;
    }

    return (int)(found - str->data);
}

bool edge_string_shrink_to_fit(edge_string_t* str) {
    if (!str) {
        return false;
    }

    size_t required = str->length + 1;
    if (required >= str->capacity) {
        return true;
    }

    char* new_data = (char*)edge_allocator_realloc(str->allocator, str->data, required);
    if (!new_data) {
        return false;
    }

    str->data = new_data;
    str->capacity = required;

    return true;
}

edge_string_t* edge_string_duplicate(const edge_string_t* str) {
    if (!str) {
        return NULL;
    }
    return edge_string_create_from_buffer(str->allocator, str->data, str->length);
}
