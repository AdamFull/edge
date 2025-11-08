/**
 * @file edge_zip_util.c
 * @brief Utility functions for ZIP library
 */

#include "edge_zip_internal.h"
#include <string.h>

 /* CRC-32 lookup table */
static uint32_t crc32_table[256];
static bool crc32_table_initialized = false;

static void init_crc32_table(void) {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            }
            else {
                crc >>= 1;
            }
        }
        crc32_table[i] = crc;
    }
    crc32_table_initialized = true;
}

uint32_t edge_zip_crc32(const void* data, size_t size) {
    if (!crc32_table_initialized) {
        init_crc32_table();
    }

    const uint8_t* bytes = (const uint8_t*)data;
    uint32_t crc = 0xFFFFFFFF;

    for (size_t i = 0; i < size; i++) {
        crc = crc32_table[(crc ^ bytes[i]) & 0xFF] ^ (crc >> 8);
    }

    return crc ^ 0xFFFFFFFF;
}

uint16_t read_uint16_le(const uint8_t* buf) {
    return (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
}

uint32_t read_uint32_le(const uint8_t* buf) {
    return (uint32_t)buf[0] |
        ((uint32_t)buf[1] << 8) |
        ((uint32_t)buf[2] << 16) |
        ((uint32_t)buf[3] << 24);
}

void write_uint16_le(uint8_t* buf, uint16_t value) {
    buf[0] = value & 0xFF;
    buf[1] = (value >> 8) & 0xFF;
}

void write_uint32_le(uint8_t* buf, uint32_t value) {
    buf[0] = value & 0xFF;
    buf[1] = (value >> 8) & 0xFF;
    buf[2] = (value >> 16) & 0xFF;
    buf[3] = (value >> 24) & 0xFF;
}

time_t dos_to_time_t(uint16_t dos_date, uint16_t dos_time) {
    struct tm tm_info = { 0 };

    /* DOS date format: bits 0-4 day, 5-8 month, 9-15 year (from 1980) */
    tm_info.tm_mday = dos_date & 0x1F;
    tm_info.tm_mon = ((dos_date >> 5) & 0x0F) - 1;
    tm_info.tm_year = ((dos_date >> 9) & 0x7F) + 80;  /* Years since 1900 */

    /* DOS time format: bits 0-4 seconds/2, 5-10 minutes, 11-15 hours */
    tm_info.tm_sec = (dos_time & 0x1F) * 2;
    tm_info.tm_min = (dos_time >> 5) & 0x3F;
    tm_info.tm_hour = (dos_time >> 11) & 0x1F;

    tm_info.tm_isdst = -1;  /* Let mktime determine DST */

    return mktime(&tm_info);
}

void time_t_to_dos(time_t t, uint16_t* dos_date, uint16_t* dos_time) {
    struct tm* tm_info = localtime(&t);

    if (!tm_info) {
        *dos_date = 0;
        *dos_time = 0;
        return;
    }

    /* Create DOS date */
    *dos_date = (tm_info->tm_mday & 0x1F) |
        (((tm_info->tm_mon + 1) & 0x0F) << 5) |
        (((tm_info->tm_year - 80) & 0x7F) << 9);

    /* Create DOS time */
    *dos_time = ((tm_info->tm_sec / 2) & 0x1F) |
        ((tm_info->tm_min & 0x3F) << 5) |
        ((tm_info->tm_hour & 0x1F) << 11);
}

int find_end_central_dir(FILE* file, zip_end_central_dir_t* eocd) {
    if (!file || !eocd) {
        return EDGE_ZIP_ERROR_INVALID_ARGUMENT;
    }

    /* Get file size */
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);

    if (file_size < ZIP_END_CENTRAL_DIR_SIZE) {
        return EDGE_ZIP_ERROR_CORRUPT_ARCHIVE;
    }

    /* Search for EOCD signature from end of file */
    /* Maximum comment size is 65535, so search up to that */
    long search_range = file_size < 65535 + ZIP_END_CENTRAL_DIR_SIZE ?
        file_size : 65535 + ZIP_END_CENTRAL_DIR_SIZE;

    uint8_t buffer[1024];
    long search_pos = file_size - ZIP_END_CENTRAL_DIR_SIZE;

    while (search_pos >= file_size - search_range) {
        long read_pos = search_pos > 0 ? search_pos : 0;
        long read_size = search_pos + sizeof(buffer) <= file_size ?
            sizeof(buffer) : file_size - read_pos;

        fseek(file, read_pos, SEEK_SET);
        if (fread(buffer, 1, read_size, file) != (size_t)read_size) {
            return EDGE_ZIP_ERROR_IO;
        }

        /* Search for signature in buffer */
        for (long i = read_size - 4; i >= 0; i--) {
            uint32_t sig = read_uint32_le(buffer + i);
            if (sig == ZIP_END_CENTRAL_DIR_SIG) {
                /* Found it! Read the full EOCD record */
                fseek(file, read_pos + i, SEEK_SET);
                uint8_t eocd_buf[ZIP_END_CENTRAL_DIR_SIZE];
                if (fread(eocd_buf, 1, ZIP_END_CENTRAL_DIR_SIZE, file) != ZIP_END_CENTRAL_DIR_SIZE) {
                    return EDGE_ZIP_ERROR_IO;
                }

                eocd->signature = read_uint32_le(eocd_buf);
                eocd->disk_number = read_uint16_le(eocd_buf + 4);
                eocd->central_dir_disk = read_uint16_le(eocd_buf + 6);
                eocd->num_entries_disk = read_uint16_le(eocd_buf + 8);
                eocd->num_entries_total = read_uint16_le(eocd_buf + 10);
                eocd->central_dir_size = read_uint32_le(eocd_buf + 12);
                eocd->central_dir_offset = read_uint32_le(eocd_buf + 16);
                eocd->comment_length = read_uint16_le(eocd_buf + 20);

                return EDGE_ZIP_OK;
            }
        }

        search_pos -= sizeof(buffer) - 3;  /* Overlap to not miss signature */
    }

    return EDGE_ZIP_ERROR_CORRUPT_ARCHIVE;
}

int read_central_directory(edge_zip_archive_t* archive) {
    if (!archive || !archive->file) {
        return EDGE_ZIP_ERROR_INVALID_ARGUMENT;
    }

    /* Find EOCD record */
    zip_end_central_dir_t eocd;
    int result = find_end_central_dir(archive->file, &eocd);
    if (result != EDGE_ZIP_OK) {
        return result;
    }

    /* Validate EOCD */
    if (eocd.disk_number != 0 || eocd.central_dir_disk != 0) {
        return EDGE_ZIP_ERROR_UNSUPPORTED;  /* Multi-disk archives not supported */
    }

    if (eocd.num_entries_disk != eocd.num_entries_total) {
        return EDGE_ZIP_ERROR_CORRUPT_ARCHIVE;
    }

    archive->num_entries = eocd.num_entries_total;
    archive->central_dir_offset = eocd.central_dir_offset;

    if (archive->num_entries == 0) {
        archive->entries = NULL;
        return EDGE_ZIP_OK;
    }

    /* Allocate entries array */
    archive->entries = zip_malloc(archive, sizeof(edge_zip_entry_t) * archive->num_entries);
    if (!archive->entries) {
        return EDGE_ZIP_ERROR_OUT_OF_MEMORY;
    }
    memset(archive->entries, 0, sizeof(edge_zip_entry_t) * archive->num_entries);

    /* Seek to central directory */
    fseek(archive->file, eocd.central_dir_offset, SEEK_SET);

    /* Read each central directory entry */
    for (int i = 0; i < archive->num_entries; i++) {
        uint8_t cd_header[ZIP_CENTRAL_DIR_HEADER_SIZE];
        if (fread(cd_header, 1, ZIP_CENTRAL_DIR_HEADER_SIZE, archive->file) != ZIP_CENTRAL_DIR_HEADER_SIZE) {
            return EDGE_ZIP_ERROR_IO;
        }

        uint32_t signature = read_uint32_le(cd_header);
        if (signature != ZIP_CENTRAL_DIR_HEADER_SIG) {
            return EDGE_ZIP_ERROR_CORRUPT_ARCHIVE;
        }

        edge_zip_entry_t* entry = &archive->entries[i];
        entry->archive = archive;

        uint16_t filename_len = read_uint16_le(cd_header + 28);
        uint16_t extra_len = read_uint16_le(cd_header + 30);
        uint16_t comment_len = read_uint16_le(cd_header + 32);

        entry->compression_method = read_uint16_le(cd_header + 10);
        entry->last_mod_time = read_uint16_le(cd_header + 12);
        entry->last_mod_date = read_uint16_le(cd_header + 14);
        entry->crc32 = read_uint32_le(cd_header + 16);
        entry->compressed_size = read_uint32_le(cd_header + 20);
        entry->uncompressed_size = read_uint32_le(cd_header + 24);
        entry->flags = read_uint16_le(cd_header + 8);
        entry->local_header_offset = read_uint32_le(cd_header + 42);

        /* Read filename */
        entry->filename_length = filename_len;
        entry->filename = zip_malloc(archive, filename_len + 1);
        if (!entry->filename) {
            return EDGE_ZIP_ERROR_OUT_OF_MEMORY;
        }

        if (fread(entry->filename, 1, filename_len, archive->file) != filename_len) {
            return EDGE_ZIP_ERROR_IO;
        }
        entry->filename[filename_len] = '\0';

        /* Check if directory */
        entry->is_directory = (filename_len > 0 && entry->filename[filename_len - 1] == '/');

        /* Skip extra field and comment */
        fseek(archive->file, extra_len + comment_len, SEEK_CUR);
    }

    return EDGE_ZIP_OK;
}