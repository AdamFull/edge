/**
 * @file edge_zip_read.c
 * @brief Entry reading and extraction functions
 */

#include "edge_zip_internal.h"
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

int edge_zip_get_entry_by_index(edge_zip_archive_t* archive, int index, edge_zip_entry_t** entry) {
    if (!archive || !entry) {
        return EDGE_ZIP_ERROR_INVALID_ARGUMENT;
    }

    if (archive->mode_write) {
        return EDGE_ZIP_ERROR_UNSUPPORTED;  /* Can't read from write-mode archive */
    }

    if (index < 0 || index >= archive->num_entries) {
        return EDGE_ZIP_ERROR_NOT_FOUND;
    }

    *entry = &archive->entries[index];
    return EDGE_ZIP_OK;
}

int edge_zip_find_entry(edge_zip_archive_t* archive, const char* name, edge_zip_entry_t** entry) {
    if (!archive || !name || !entry) {
        return EDGE_ZIP_ERROR_INVALID_ARGUMENT;
    }

    if (archive->mode_write) {
        return EDGE_ZIP_ERROR_UNSUPPORTED;
    }

    for (int i = 0; i < archive->num_entries; i++) {
        if (strcmp(archive->entries[i].filename, name) == 0) {
            *entry = &archive->entries[i];
            return EDGE_ZIP_OK;
        }
    }

    return EDGE_ZIP_ERROR_NOT_FOUND;
}

int edge_zip_entry_get_info(const edge_zip_entry_t* entry, edge_zip_entry_info_t* info) {
    if (!entry || !info) {
        return EDGE_ZIP_ERROR_INVALID_ARGUMENT;
    }

    info->filename = entry->filename;
    info->filename_length = entry->filename_length;
    info->uncompressed_size = entry->uncompressed_size;
    info->compressed_size = entry->compressed_size;
    info->crc32 = entry->crc32;
    info->compression = (edge_zip_compression_method_t)entry->compression_method;
    info->encryption = (edge_zip_encryption_method_t)entry->encryption_method;
    info->modified_time = dos_to_time_t(entry->last_mod_date, entry->last_mod_time);
    info->is_directory = entry->is_directory;
    info->flags = entry->flags;
    info->version_made_by = 0;  /* Not stored in entry structure */
    info->version_needed = 0;   /* Not stored in entry structure */

    return EDGE_ZIP_OK;
}

int edge_zip_entry_read(edge_zip_entry_t* entry, void* buffer, size_t buffer_size, size_t* bytes_read) {
    if (!entry || !buffer || !bytes_read) {
        return EDGE_ZIP_ERROR_INVALID_ARGUMENT;
    }

    edge_zip_archive_t* archive = entry->archive;

    if (!archive || !archive->file) {
        return EDGE_ZIP_ERROR_INVALID_ARGUMENT;
    }

    if (entry->is_directory) {
        *bytes_read = 0;
        return EDGE_ZIP_OK;
    }

    if (buffer_size < entry->uncompressed_size) {
        return EDGE_ZIP_ERROR_INVALID_ARGUMENT;
    }

    /* Seek to local file header */
    fseek(archive->file, entry->local_header_offset, SEEK_SET);

    /* Read local file header */
    uint8_t local_header[ZIP_LOCAL_FILE_HEADER_SIZE];
    if (fread(local_header, 1, ZIP_LOCAL_FILE_HEADER_SIZE, archive->file) != ZIP_LOCAL_FILE_HEADER_SIZE) {
        return EDGE_ZIP_ERROR_IO;
    }

    uint32_t signature = read_uint32_le(local_header);
    if (signature != ZIP_LOCAL_FILE_HEADER_SIG) {
        return EDGE_ZIP_ERROR_CORRUPT_ARCHIVE;
    }

    uint16_t filename_len = read_uint16_le(local_header + 26);
    uint16_t extra_len = read_uint16_le(local_header + 28);

    /* Skip filename and extra field to get to data */
    fseek(archive->file, filename_len + extra_len, SEEK_CUR);

    /* Read compressed data */
    void* compressed_data = zip_malloc(archive, entry->compressed_size);
    if (!compressed_data) {
        return EDGE_ZIP_ERROR_OUT_OF_MEMORY;
    }

    if (fread(compressed_data, 1, entry->compressed_size, archive->file) != entry->compressed_size) {
        zip_free(archive, compressed_data);
        return EDGE_ZIP_ERROR_IO;
    }

    /* Decrypt if needed */
    void* decrypted_data = compressed_data;
    size_t decrypted_size = entry->compressed_size;

    if (entry->flags & ZIP_FLAG_ENCRYPTED) {
        if (!archive->encryptor.decrypt_fn) {
            zip_free(archive, compressed_data);
            return EDGE_ZIP_ERROR_UNSUPPORTED;
        }

        decrypted_data = zip_malloc(archive, entry->compressed_size);
        if (!decrypted_data) {
            zip_free(archive, compressed_data);
            return EDGE_ZIP_ERROR_OUT_OF_MEMORY;
        }

        int result = archive->encryptor.decrypt_fn(
            decrypted_data, &decrypted_size,
            compressed_data, entry->compressed_size,
            entry->encryption_method,
            archive->encryptor.user_data
        );

        zip_free(archive, compressed_data);

        if (result != EDGE_ZIP_OK) {
            zip_free(archive, decrypted_data);
            return EDGE_ZIP_ERROR_DECRYPTION;
        }

        compressed_data = decrypted_data;
    }

    /* Decompress if needed */
    if (entry->compression_method == EDGE_ZIP_COMPRESSION_STORE) {
        /* No compression - just copy */
        if (decrypted_size != entry->uncompressed_size) {
            zip_free(archive, compressed_data);
            return EDGE_ZIP_ERROR_CORRUPT_ARCHIVE;
        }
        memcpy(buffer, compressed_data, entry->uncompressed_size);
        *bytes_read = entry->uncompressed_size;
    }
    else {
        /* Decompression required */
        if (!archive->compressor.decompress_fn) {
            zip_free(archive, compressed_data);
            return EDGE_ZIP_ERROR_UNSUPPORTED;
        }

        int result = archive->compressor.decompress_fn(
            buffer, entry->uncompressed_size,
            compressed_data, decrypted_size,
            entry->compression_method,
            archive->compressor.user_data
        );

        zip_free(archive, compressed_data);

        if (result != EDGE_ZIP_OK) {
            return EDGE_ZIP_ERROR_DECOMPRESSION;
        }

        *bytes_read = entry->uncompressed_size;
    }

    zip_free(archive, compressed_data);

    /* Verify CRC-32 */
    uint32_t calculated_crc = edge_zip_crc32(buffer, entry->uncompressed_size);
    if (calculated_crc != entry->crc32) {
        return EDGE_ZIP_ERROR_CORRUPT_ARCHIVE;
    }

    return EDGE_ZIP_OK;
}

int edge_zip_entry_extract(edge_zip_entry_t* entry, const char* output_path) {
    if (!entry || !output_path) {
        return EDGE_ZIP_ERROR_INVALID_ARGUMENT;
    }

    if (entry->is_directory) {
        int res;
#ifdef _WIN32
        res = _mkdir(output_path);
#else
        res = mkdir(output_path, S_IWUSR);
#endif
        if (res != 0) {
            /* Ignore error if directory already exists */
        }
        return EDGE_ZIP_OK;
    }

    edge_zip_archive_t* archive = entry->archive;

    /* Allocate buffer for decompressed data */
    void* buffer = zip_malloc(archive, entry->uncompressed_size);
    if (!buffer) {
        return EDGE_ZIP_ERROR_OUT_OF_MEMORY;
    }

    /* Read entry data */
    size_t bytes_read;
    int result = edge_zip_entry_read(entry, buffer, entry->uncompressed_size, &bytes_read);
    if (result != EDGE_ZIP_OK) {
        zip_free(archive, buffer);
        return result;
    }

    /* Write to file */
    FILE* output_file = fopen(output_path, "wb");
    if (!output_file) {
        zip_free(archive, buffer);
        return EDGE_ZIP_ERROR_IO;
    }

    size_t written = fwrite(buffer, 1, bytes_read, output_file);
    fclose(output_file);
    zip_free(archive, buffer);

    if (written != bytes_read) {
        return EDGE_ZIP_ERROR_IO;
    }

    return EDGE_ZIP_OK;
}
