#include "json.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <errno.h>

namespace edge {
    static usize string_hash(const String& key) {
        return hash_string_64(key.m_data);
    }

    static i32 string_compare(const String& key1, const String& key2) {
        return strcmp(key1.m_data, key2.m_data);
    }

    const char* json_version() {
        static char version[32];
        snprintf(version, sizeof(version), "%d.%d.%d",
            JSON_VERSION_MAJOR, JSON_VERSION_MINOR, JSON_VERSION_PATCH);
        return version;
    }

    JsonValue* json_null(const Allocator* alloc) {
        if (!alloc) {
            return nullptr;
        }

        JsonValue* value = allocate_zeroed<JsonValue>(alloc);
        if (!value) {
            return nullptr;
        }

        value->m_type = JsonType::Null;
        value->m_allocator = alloc;
        return value;
    }

    JsonValue* json_bool(const Allocator* alloc, bool val) {
        if (!alloc) {
            return nullptr;
        }

        JsonValue* value = allocate_zeroed<JsonValue>(alloc);
        if (!value) {
            return nullptr;
        }

        value->m_type = JsonType::Bool;
        value->as.bool_value = val ? 1 : 0;
        value->m_allocator = alloc;
        return value;
    }

    JsonValue* json_number(const Allocator* alloc, f64 val) {
        if (!alloc) {
            return nullptr;
        }

        JsonValue* value = allocate_zeroed<JsonValue>(alloc);
        if (!value) {
            return nullptr;
        }

        value->m_type = JsonType::Number;
        value->as.number_value = val;
        value->m_allocator = alloc;
        return value;
    }

    JsonValue* json_int(const Allocator* alloc, i64 val) {
        return json_number(alloc, (f64)val);
    }

    JsonValue* json_string(const Allocator* alloc, const char* str) {
        if (!alloc) {
            return nullptr;
        }

        if (!str) {
            return json_null(alloc);
        }
        return json_string_len(alloc, str, strlen(str));
    }

    JsonValue* json_string_len(const Allocator* alloc, const char* str, size_t length) {
        if (!alloc) {
            return nullptr;
        }

        if (!str) {
            return json_null(alloc);
        }

        JsonValue* value = allocate_zeroed<JsonValue>(alloc);
        if (!value) {
            return nullptr;
        }

        value->m_type = JsonType::String;
        if (!string_create_from_buffer(alloc, &value->as.string_value, str, length)) {
            deallocate(alloc, value);
            return nullptr;
        }

        value->m_allocator = alloc;

        return value;
    }

    JsonValue* json_array(const Allocator* alloc) {
        if (!alloc) {
            return nullptr;
        }

        JsonValue* value = allocate_zeroed<JsonValue>(alloc);
        if (!value) {
            return nullptr;
        }

        value->m_type = JsonType::Array;
        if (!value->as.array_value.reserve(alloc, 8)) {
            deallocate(alloc, value);
            return nullptr;
        }

        value->m_allocator = alloc;

        return value;
    }

    JsonValue* json_object(const Allocator* alloc) {
        if (!alloc) {
            return nullptr;
        }

        JsonValue* value = allocate_zeroed<JsonValue>(alloc);
        if (!value) {
            return nullptr;
        }

        value->m_type = JsonType::Object;
        if (!hashmap_create_custom(
            alloc,
            &value->as.object_value,
            8,
            string_hash,
            string_compare)) {
            deallocate(alloc, value);
            return nullptr;
        }

        value->m_allocator = alloc;

        return value;
    }

    void json_free_value(JsonValue* value) {
        if (!value) {
            return;
        }

        switch (value->m_type) {
        case JsonType::String: {
            string_destroy(&value->as.string_value);
            break;
        }
        case JsonType::Array: {
            for (size_t i = 0; i < value->as.array_value.m_size; i++) {
                JsonValue** elem_ptr = value->as.array_value.get(i);
                json_free_value(*elem_ptr);
            }
            value->as.array_value.destroy(value->m_allocator);
            break;
        }
        case JsonType::Object: {
            for (auto& entry : value->as.object_value) {
                string_destroy(&entry.key);
                json_free_value(entry.value);
            }
            hashmap_destroy(&value->as.object_value);
            break;
        }
        default:
            break;
        }

        deallocate(value->m_allocator, value);
    }

    bool json_get_bool(const JsonValue* value, bool default_value) {
        if (!value || value->m_type != JsonType::Bool) {
            return default_value;
        }
        return value->as.bool_value;
    }

    f64 json_get_number(const JsonValue* value, f64 default_value) {
        if (!value || value->m_type != JsonType::Number) {
            return default_value;
        }
        return value->as.number_value;
    }

    i64 json_get_int(const JsonValue* value, i64 default_value) {
        if (!value || value->m_type != JsonType::Number) {
            return default_value;
        }
        return (i64)value->as.number_value;
    }

    const char* json_get_string(const JsonValue* value, const char* default_value) {
        if (!value || value->m_type != JsonType::String) {
            return default_value;
        }
        return value->as.string_value.m_data;
    }

    JsonValue* json_array_get(const JsonValue* array, size_t index) {
        if (!array || array->m_type != JsonType::Array) {
            return nullptr;
        }

        JsonValue* const* elem_ptr = array->as.array_value.get(index);
        return elem_ptr ? *elem_ptr : nullptr;
    }

    bool json_array_append(JsonValue* array, JsonValue* value) {
        if (!array || array->m_type != JsonType::Array || !value) {
            return false;
        }
        return array->as.array_value.push_back(array->m_allocator, value);
    }

    bool json_array_insert(JsonValue* array, size_t index, JsonValue* value) {
        if (!array || array->m_type != JsonType::Array || !value) {
            return false;
        }

        return array->as.array_value.insert(array->m_allocator, index, value);
    }

    bool json_array_remove(JsonValue* array, size_t index) {
        if (!array || array->m_type != JsonType::Array) {
            return false;
        }

        JsonValue* removed_value = nullptr;
        if (!array->as.array_value.remove(index, &removed_value)) {
            return false;
        }

        json_free_value(removed_value);
        return true;
    }

    void json_array_clear(JsonValue* array) {
        if (!array || array->m_type != JsonType::Array) {
            return;
        }

        for (size_t i = 0; i < array->as.array_value.m_size; i++) {
            JsonValue** elem_ptr = array->as.array_value.get(i);
            json_free_value(*elem_ptr);
        }

        array->as.array_value.clear();
    }

    size_t json_object_size(const JsonValue* object) {
        if (!object || object->m_type != JsonType::Object) {
            return 0;
        }
        return hashmap_size(&object->as.object_value);
    }

    JsonValue* json_object_get(const JsonValue* object, const char* key) {
        if (!object || object->m_type != JsonType::Object || !key) {
            return nullptr;
        }

        String key_str;
        if (!string_create_from(object->m_allocator, &key_str, key)) {
            return nullptr;
        }

        JsonValue** val_ptr = hashmap_get(&object->as.object_value, key_str);
        string_destroy(&key_str);
        return val_ptr ? *val_ptr : nullptr;
    }

    bool json_object_set(JsonValue* object, const char* key, JsonValue* value) {
        if (!object || object->m_type != JsonType::Object || !key || !value) {
            return false;
        }

        String key_str;
        if (!string_create_from(object->m_allocator, &key_str, key)) {
            return false;
        }

        JsonValue** old_value = hashmap_get(&object->as.object_value, key_str);
        if (old_value) {
            json_free_value(*old_value);
        }

        bool result = hashmap_insert(&object->as.object_value, key_str, value);
        if (!result) {
            string_destroy(&key_str);
        }

        return result;
    }

    bool json_object_remove(JsonValue* object, const char* key) {
        if (!object || object->m_type != JsonType::Object || !key) {
            return false;
        }

        String key_str;
        if (!string_create_from(object->m_allocator, &key_str, key)) {
            return false;
        }

        JsonValue* removed_value;
        bool result = hashmap_remove(&object->as.object_value, key_str, &removed_value);
        string_destroy(&key_str);

        if (result) {
            json_free_value(removed_value);
        }

        return result;
    }

    bool json_object_has(const JsonValue* object, const char* key) {
        if (!object || object->m_type != JsonType::Object || !key) {
            return false;
        }

        String key_str;
        if (!string_create_from(object->m_allocator, &key_str, key)) {
            return false;
        }

        bool result = hashmap_contains(&object->as.object_value, key_str);
        string_destroy(&key_str);

        return result;
    }

    void json_object_clear(JsonValue* object) {
        if (!object || object->m_type != JsonType::Object) {
            return;
        }

        for (auto& entry : object->as.object_value) {
            string_destroy(&entry.key);
            json_free_value(entry.value);
        }
        hashmap_clear(&object->as.object_value);
    }

    const char* json_object_get_key(const JsonValue* object, size_t index) {
        if (!object || object->m_type != JsonType::Object) {
            return nullptr;
        }

        usize current = 0;
        for (const auto& entry : object->as.object_value) {
            if (current == index) {
                return string_cstr(&entry.key);
            }
            current++;
        }

        return nullptr;
    }

    JsonValue* json_object_get_value_at(const JsonValue* object, usize index) {
        if (!object || object->m_type != JsonType::Object) {
            return nullptr;
        }

        usize current = 0;
        for (const auto& entry : object->as.object_value) {
            if (current == index) {
                return entry.value;
            }
            current++;
        }

        return nullptr;
    }

    JsonValue* json_clone(const JsonValue* value) {
        if (!value) {
            return nullptr;
        }

        switch (value->m_type) {
        case JsonType::Null:
            return json_null(value->m_allocator);

        case JsonType::Bool:
            return json_bool(value->m_allocator, value->as.bool_value);

        case JsonType::Number:
            return json_number(value->m_allocator, value->as.number_value);

        case JsonType::String:
            return json_string_len(
                value->m_allocator,
                string_cstr(&value->as.string_value),
                string_length(&value->as.string_value));

        case JsonType::Array: {
            JsonValue* array = json_array(value->m_allocator);
            if (!array) {
                return nullptr;
            }

            for (usize i = 0; i < value->as.array_value.m_size; i++) {
                JsonValue* const* elem_ptr = value->as.array_value.get(i);
                JsonValue* elem = json_clone(*elem_ptr);
                if (!elem || !json_array_append(array, elem)) {
                    if (elem) json_free_value(elem);
                    json_free_value(array);
                    return nullptr;
                }
            }

            return array;
        }

        case JsonType::Object: {
            JsonValue* object = json_object(value->m_allocator);
            if (!object) return nullptr;

            for (const auto& entry : value->as.object_value) {
                const char* key = string_cstr(&entry.key);
                JsonValue* val = json_clone(entry.value);

                if (!val || !json_object_set(object, key, val)) {
                    if (val) json_free_value(val);
                    json_free_value(object);
                    return nullptr;
                }
            }

            return object;
        }
        }

        return nullptr;
    }

    bool json_equals(const JsonValue* a, const JsonValue* b) {
        if (a == b) {
            return true;
        }

        if (!a || !b) {
            return false;
        }

        if (a->m_type != b->m_type) {
            return false;
        }

        switch (a->m_type) {
        case JsonType::Null:
            return true;

        case JsonType::Bool:
            return a->as.bool_value == b->as.bool_value;

        case JsonType::Number:
            return a->as.number_value == b->as.number_value;

        case JsonType::String:
            return string_compare_string(&a->as.string_value, &b->as.string_value) == 0;

        case JsonType::Array: {
            if (a->as.array_value.m_size != b->as.array_value.m_size) {
                return false;
            }

            for (usize i = 0; i < a->as.array_value.m_size; i++) {
                JsonValue* const* elem_a = a->as.array_value.get(i);
                JsonValue* const* elem_b = b->as.array_value.get(i);
                if (!json_equals(*elem_a, *elem_b)) {
                    return false;
                }
            }

            return true;
        }

        case JsonType::Object: {
            usize size_a = hashmap_size(&a->as.object_value);
            usize size_b = hashmap_size(&b->as.object_value);
            if (size_a != size_b) {
                return false;
            }

            for (const auto& entry : a->as.object_value) {
                const char* key = string_cstr(&entry.key);
                JsonValue* val_b = json_object_get(b, key);

                if (!val_b || !json_equals(entry.value, val_b)) {
                    return false;
                }
            }

            return true;
        }
        }

        return false;
    }

    bool json_object_merge(JsonValue* dest, const JsonValue* source) {
        if (!dest || !source || dest->m_type != JsonType::Object || source->m_type != JsonType::Object) {
            return false;
        }

        for (const auto& entry : source->as.object_value) {
            const char* key = string_cstr(&entry.key);
            JsonValue* value = json_clone(entry.value);

            if (!value || !json_object_set(dest, key, value)) {
                if (value) json_free_value(value);
                return false;
            }
        }

        return true;
    }
}