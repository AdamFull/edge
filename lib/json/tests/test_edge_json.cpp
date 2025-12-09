#include "json.hpp"
#include <assert.h>
#include <stdlib.h>

int main(void) {
    const char* json_data =
        "{"
        "\"uuid\":\"f1653d1a-d08b-4b5a-9a15-740c715adea2\","
        "\"entities\":["
        "{"
        "\"name\":\"root\","
        "\"uuid\":\"ffbf290c-8bf5-41d9-bac6-9728eac41edd\","
        "\"parent\":null"
        "},"
        "{"
        "\"name\":\"first\","
        "\"uuid\":\"e1eb8d25-049c-4faf-83fb-3eaeef4d2707\","
        "\"parent\":\"ffbf290c-8bf5-41d9-bac6-9728eac41edd\","
        "\"position\":[0.0,10.0,20.0],"
        "\"components\":["
        "{"
        "\"uuid\":\"9724f262-46d0-4efe-9067-332adb1b3e0d\","
        "\"type\":\"camera\","
        "\"properties\":{"
        "\"type\":\"proj\""
        "}"
        "}"
        "]"
        "},"
        "{"
        "\"name\":\"second\","
        "\"uuid\":\"76ca6908-f8e3-4e01-a977-712df4e40140\","
        "\"parent\":\"ffbf290c-8bf5-41d9-bac6-9728eac41edd\","
        "\"position\":[0.0,20.0,20.0],"
        "\"rotation\":[0.1,0.5,0.2,1.0],"
        "\"scale\":[0.1,0.5,1.5],"
        "\"components\":["
        "{"
        "\"uuid\":\"82b8c593-e7a9-4e33-afa0-060699120cf3\","
        "\"type\":\"mesh\","
        "\"properties\":{"
        "\"asset\":\"38e3b340-60db-4a39-bc6a-695276001078\","
        "\"cast_shadows\":false"
        "}"
        "}"
        "]"
        "}"
        "]"
        "}";

    edge::Allocator allocator = edge::allocator_create_tracking();

    edge::JsonValue* json = edge::json_parse(&allocator, json_data);
    if (json) {
        if (edge::json_object_has(json, "uuid")) {
            edge::JsonValue* uuid_obj = edge::json_object_get(json, "uuid");
            const char* uuid_str = edge::json_get_string(uuid_obj, "");
            uuid_str = uuid_str;
        }
    }

    char* memory = edge::json_stringify_pretty(json, "  ");
    edge::deallocate(&allocator, memory);

    edge::json_free_value(json);

    size_t net_allocated = edge::allocator_get_net(&allocator);
    assert(net_allocated == 0 && "Memory leaked");

    return 0;
}
