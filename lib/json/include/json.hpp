#ifndef EDGE_JSON_H
#define EDGE_JSON_H

#include <string.hpp>
#include <array.hpp>
#include <hashmap.hpp>

#include <stddef.h>
#include <stdint.h>

namespace edge {
    constexpr i32 JSON_VERSION_MAJOR = 0;
    constexpr i32 JSON_VERSION_MINOR = 1;
    constexpr i32 JSON_VERSION_PATCH = 0;

    enum class JsonType {
        Null,
        Bool,
        Number,
        String,
        Array,
        Object
    };

    struct JsonValue {
        JsonType m_type;
        union {
            bool bool_value;
            f64 number_value;
            String string_value;
            Array<JsonValue*> array_value;
            HashMap<String, JsonValue*> object_value;
        } as;

        const Allocator* m_allocator;
    };

    const char* json_version(void);

    JsonValue* json_null(const Allocator* alloc);
    JsonValue* json_bool(const Allocator* alloc, bool value);
    JsonValue* json_number(const Allocator* alloc, f64 value);
    JsonValue* json_int(const Allocator* alloc, i64 value);
    JsonValue* json_string(const Allocator* alloc, const char* value);
    JsonValue* json_string_len(const Allocator* alloc, const char* value, size_t length);
    JsonValue* json_array(const Allocator* alloc);
    JsonValue* json_object(const Allocator* alloc);
    void json_free_value(JsonValue* value);

    bool json_get_bool(const JsonValue* value, bool default_value);
    f64 json_get_number(const JsonValue* value, f64 default_value);
    i64 json_get_int(const JsonValue* value, i64 default_value);
    const char* json_get_string(const JsonValue* value, const char* default_value);

    JsonValue* json_array_get(const JsonValue* array, size_t index);
    bool json_array_append(JsonValue* array, JsonValue* value);
    bool json_array_insert(JsonValue* array, size_t index, JsonValue* value);
    bool json_array_remove(JsonValue* array, size_t index);
    void json_array_clear(JsonValue* array);

    size_t json_object_size(const JsonValue* object);
    JsonValue* json_object_get(const JsonValue* object, const char* key);
    bool json_object_set(JsonValue* object, const char* key, JsonValue* value);
    bool json_object_remove(JsonValue* object, const char* key);
    bool json_object_has(const JsonValue* object, const char* key);
    void json_object_clear(JsonValue* object);
    const char* json_object_get_key(const JsonValue* object, size_t index);
    JsonValue* json_object_get_value_at(const JsonValue* object, size_t index);

    JsonValue* json_parse(const Allocator* alloc, const char* json);
    JsonValue* json_parse_len(const Allocator* alloc, const char* json, size_t length);

    char* json_stringify(const JsonValue* value);
    char* json_stringify_pretty(const JsonValue* value, const char* indent);

    JsonValue* json_clone(const JsonValue* value);

    bool json_equals(const JsonValue* a, const JsonValue* b);
    bool json_object_merge(JsonValue* dest, const JsonValue* source);
}

#endif /* EDGE_JSON_H */
