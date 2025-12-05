#include "edge_uuid.h"
#include "edge_rng.h"

#include <stdio.h>
#include <string.h>

static inline u8 hex_to_nibble(char c) {
    if (c >= '0' && c <= '9') {
        return (u8)(c - '0');
    }
    else if (c >= 'a' && c <= 'f') {
        return (u8)(c - 'a' + 10);
    }
    else if (c >= 'A' && c <= 'F') {
        return (u8)(c - 'A' + 10);
    }
    return 255;
}

void edge_uuid_v4(edge_rng_t* rng, edge_uuid_t* uuid) {
    if (!rng || !uuid) {
        memset(&uuid, 0, sizeof(uuid));
        return;
    }

    edge_rng_bytes(rng, uuid->bytes, 16);

    uuid->bytes[6] = (uuid->bytes[6] & 0x0F) | 0x40;
    uuid->bytes[8] = (uuid->bytes[8] & 0x3F) | 0x80;
}

char* edge_uuid_to_string(const edge_uuid_t* uuid, char* out_str, int* out_size) {
    if (!uuid || !out_str || !out_size) {
        return NULL;
    }

    *out_size = sprintf(out_str,
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        uuid->bytes[0], uuid->bytes[1], uuid->bytes[2], uuid->bytes[3],
        uuid->bytes[4], uuid->bytes[5],
        uuid->bytes[6], uuid->bytes[7],
        uuid->bytes[8], uuid->bytes[9],
        uuid->bytes[10], uuid->bytes[11], uuid->bytes[12],
        uuid->bytes[13], uuid->bytes[14], uuid->bytes[15]);

    return out_str;
}

bool edge_uuid_parse(const char* str, edge_uuid_t* out_uuid) {
    if (!str || !out_uuid) {
        return false;
    }

    size_t len = strlen(str);
    int hyphen_count = 0;
    for (size_t i = 0; i < len; i++) {
        if (str[i] == '-') {
            hyphen_count++;
        }
    }

    if (hyphen_count == 4 && len == 36) {
        const char* p = str;

        for (int i = 0; i < 16; i++) {
            /* Skip hyphens at positions 8, 13, 18, 23 */
            if (i == 4 || i == 6 || i == 8 || i == 10) {
                if (*p != '-') return false;
                p++;
            }

            u8 high = hex_to_nibble(*p++);
            u8 low = hex_to_nibble(*p++);

            if (high == 255 || low == 255) return false;

            out_uuid->bytes[i] = (high << 4) | low;
        }

        return true;
    }
    else if (hyphen_count == 0 && len == 32) {
        const char* p = str;

        for (int i = 0; i < 16; i++) {
            u8 high = hex_to_nibble(*p++);
            u8 low = hex_to_nibble(*p++);

            if (high == 255 || low == 255) return false;

            out_uuid->bytes[i] = (high << 4) | low;
        }

        return true;
    }

    return false;
}

bool edge_uuid_equals(const edge_uuid_t* a, const edge_uuid_t* b) {
    if (!a || !b) {
        return false;
    }

    return memcmp(a->bytes, b->bytes, 16) == 0;
}

i32 edge_uuid_compare(const edge_uuid_t* a, const edge_uuid_t* b) {
    if (!a || !b) {
        return 0;
    }

    return memcmp(a->bytes, b->bytes, 16);
}

bool edge_uuid_is_nil(const edge_uuid_t* uuid) {
    if (!uuid) {
        return true;
    }

    for (int i = 0; i < 16; i++) {
        if (uuid->bytes[i] != 0) {
            return false;
        }
    }

    return true;
}

bool edge_uuid_is_valid_v4(const edge_uuid_t* uuid) {
    if (!uuid) {
        return false;
    }

    if ((uuid->bytes[6] & 0xF0) != 0x40) {
        return false;
    }

    if ((uuid->bytes[8] & 0xC0) != 0x80) {
        return false;
    }

    return true;
}

u8 edge_uuid_version(const edge_uuid_t* uuid) {
    if (!uuid) {
        return 0;
    }

    return (uuid->bytes[6] >> 4) & 0x0F;
}