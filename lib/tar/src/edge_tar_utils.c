/**
 * @file edge_tar_utils.c
 * @brief TAR utility functions for header parsing and formatting
 */

#include "edge_tar_internal.h"
#include <string.h>
#include <ctype.h>

uint64_t parse_octal(const char* str, size_t len) {
    uint64_t value = 0;

    /* Skip leading spaces */
    while (len > 0 && isspace((unsigned char)*str)) {
        str++;
        len--;
    }

    /* Parse octal digits */
    while (len > 0 && *str >= '0' && *str <= '7') {
        value = (value * 8) + (*str - '0');
        str++;
        len--;
    }

    return value;
}

void format_octal(char* str, size_t len, uint64_t value) {
    if (!str || len == 0) {
        return;
    }

    /* Format as octal with null terminator */
    snprintf(str, len, "%0*lo", (int)(len - 1), (unsigned long)value);
}

uint32_t calculate_checksum(const tar_header_t* header) {
    if (!header) {
        return 0;
    }

    uint32_t sum = 0;
    const uint8_t* ptr = (const uint8_t*)header;

    /* Calculate sum of all bytes, treating checksum field as spaces */
    for (size_t i = 0; i < TAR_BLOCK_SIZE; i++) {
        if (i >= 148 && i < 156) {
            /* Checksum field - count as spaces */
            sum += ' ';
        }
        else {
            sum += ptr[i];
        }
    }

    return sum;
}

bool verify_checksum(const tar_header_t* header) {
    if (!header) {
        return false;
    }

    uint32_t stored_checksum = (uint32_t)parse_octal(header->chksum, TAR_CHKSUM_SIZE);
    uint32_t calculated_checksum = calculate_checksum(header);

    return stored_checksum == calculated_checksum;
}

edge_tar_format_t detect_tar_format(const tar_header_t* header) {
    if (!header) {
        return EDGE_TAR_FORMAT_V7;
    }

    /* Check for ustar format */
    if (memcmp(header->magic, TAR_MAGIC_USTAR, 5) == 0) {
        if (memcmp(header->version, TAR_VERSION_USTAR, 2) == 0) {
            return EDGE_TAR_FORMAT_USTAR;
        }
        /* GNU tar uses different version field */
        return EDGE_TAR_FORMAT_GNU;
    }

    /* Check for PAX format (starts with 'x' or 'g' typeflag) */
    if (header->typeflag[0] == EDGE_TAR_TYPE_PAX_EXTENDED ||
        header->typeflag[0] == EDGE_TAR_TYPE_PAX_GLOBAL) {
        return EDGE_TAR_FORMAT_PAX;
    }

    /* Default to V7 format */
    return EDGE_TAR_FORMAT_V7;
}

bool is_zero_block(const tar_header_t* header) {
    if (!header) {
        return true;
    }

    const uint8_t* ptr = (const uint8_t*)header;
    for (size_t i = 0; i < TAR_BLOCK_SIZE; i++) {
        if (ptr[i] != 0) {
            return false;
        }
    }

    return true;
}

uint64_t round_up_to_block(uint64_t size) {
    return ((size + TAR_BLOCK_SIZE - 1) / TAR_BLOCK_SIZE) * TAR_BLOCK_SIZE;
}

int write_padding(edge_tar_archive_t* archive, uint64_t data_size) {
    if (!archive) {
        return EDGE_TAR_ERROR_INVALID_ARGUMENT;
    }

    /* Calculate padding needed */
    uint64_t remainder = data_size % TAR_BLOCK_SIZE;
    if (remainder == 0) {
        return EDGE_TAR_OK;
    }

    size_t padding_size = TAR_BLOCK_SIZE - remainder;
    uint8_t padding[TAR_BLOCK_SIZE] = { 0 };

    if (fwrite(padding, 1, padding_size, archive->file) != padding_size) {
        return EDGE_TAR_ERROR_IO;
    }

    archive->write_offset += padding_size;
    return EDGE_TAR_OK;
}

int skip_to_next_block(edge_tar_archive_t* archive, uint64_t data_size) {
    if (!archive) {
        return EDGE_TAR_ERROR_INVALID_ARGUMENT;
    }

    /* Calculate padding to skip */
    uint64_t remainder = data_size % TAR_BLOCK_SIZE;
    if (remainder == 0) {
        return EDGE_TAR_OK;
    }

    size_t skip_size = TAR_BLOCK_SIZE - remainder;
    if (fseek(archive->file, (long)skip_size, SEEK_CUR) != 0) {
        return EDGE_TAR_ERROR_IO;
    }

    archive->current_pos += skip_size;
    return EDGE_TAR_OK;
}

int write_end_marker(edge_tar_archive_t* archive) {
    if (!archive) {
        return EDGE_TAR_ERROR_INVALID_ARGUMENT;
    }

    /* Write two zero blocks to mark end of archive */
    uint8_t zero_block[TAR_BLOCK_SIZE] = { 0 };

    if (fwrite(zero_block, 1, TAR_BLOCK_SIZE, archive->file) != TAR_BLOCK_SIZE) {
        return EDGE_TAR_ERROR_IO;
    }

    if (fwrite(zero_block, 1, TAR_BLOCK_SIZE, archive->file) != TAR_BLOCK_SIZE) {
        return EDGE_TAR_ERROR_IO;
    }

    return EDGE_TAR_OK;
}

int split_filename(const char* filename, char* name, char* prefix) {
    if (!filename || !name || !prefix) {
        return EDGE_TAR_ERROR_INVALID_ARGUMENT;
    }

    size_t len = strlen(filename);

    /* If it fits in name field, use it directly */
    if (len < TAR_NAME_SIZE) {
        strncpy(name, filename, TAR_NAME_SIZE);
        prefix[0] = '\0';
        return EDGE_TAR_OK;
    }

    /* Try to split at a path separator */
    const char* split_pos = NULL;
    for (size_t i = len - TAR_NAME_SIZE; i < len; i++) {
        if (filename[i] == '/') {
            split_pos = &filename[i + 1];
            break;
        }
    }

    if (split_pos && (split_pos - filename) <= TAR_PREFIX_SIZE) {
        size_t prefix_len = split_pos - filename - 1;
        size_t name_len = strlen(split_pos);

        if (name_len < TAR_NAME_SIZE) {
            strncpy(prefix, filename, prefix_len);
            prefix[prefix_len] = '\0';
            strncpy(name, split_pos, TAR_NAME_SIZE);
            return EDGE_TAR_OK;
        }
    }

    /* Filename is too long for ustar format */
    return EDGE_TAR_ERROR_UNSUPPORTED;
}

int read_tar_header(edge_tar_archive_t* archive, tar_header_t* header) {
    if (!archive || !header) {
        return EDGE_TAR_ERROR_INVALID_ARGUMENT;
    }

    size_t bytes_read = fread(header, 1, TAR_BLOCK_SIZE, archive->file);
    if (bytes_read != TAR_BLOCK_SIZE) {
        if (feof(archive->file)) {
            return EDGE_TAR_ERROR_CORRUPT_ARCHIVE;
        }
        return EDGE_TAR_ERROR_IO;
    }

    archive->current_pos += TAR_BLOCK_SIZE;
    return EDGE_TAR_OK;
}

int write_tar_header(edge_tar_archive_t* archive, const tar_header_t* header) {
    if (!archive || !header) {
        return EDGE_TAR_ERROR_INVALID_ARGUMENT;
    }

    if (fwrite(header, 1, TAR_BLOCK_SIZE, archive->file) != TAR_BLOCK_SIZE) {
        return EDGE_TAR_ERROR_IO;
    }

    archive->write_offset += TAR_BLOCK_SIZE;
    return EDGE_TAR_OK;
}