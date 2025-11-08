/**
 * @file edge_zip_write.c
 * @brief Entry writing and archive creation functions
 */

#include "edge_zip_internal.h"
#include <string.h>

int write_local_file_header(edge_zip_archive_t* archive, const edge_zip_entry_t* entry, uint32_t* offset) {
    if (!archive || !archive->file || !entry) {
        return EDGE_ZIP_ERROR_INVALID_ARGUMENT;
    }

    *offset = ftell(archive->file);

    uint8_t header[ZIP_LOCAL_FILE_HEADER_SIZE];

    write_uint32_le(header, ZIP_LOCAL_FILE_HEADER_SIG);
    write_uint16_le(header + 4, 20);  /* Version needed: 2.0 */
    write_uint16_le(header + 6, entry->flags);
    write_uint16_le(header + 8, entry->compression_method);
    write_uint16_le(header + 10, entry->last_mod_time);
    write_uint16_le(header + 12, entry->last_mod_date);
    write_uint32_le(header + 14, entry->crc32);
    write_uint32_le(header + 18, entry->compressed_size);
    write_uint32_le(header + 22, entry->uncompressed_size);
    write_uint16_le(header + 26, entry->filename_length);
    write_uint16_le(header + 28, 0);  /* Extra field length */

    if (fwrite(header, 1, ZIP_LOCAL_FILE_HEADER_SIZE, archive->file) != ZIP_LOCAL_FILE_HEADER_SIZE) {
        return EDGE_ZIP_ERROR_IO;
    }

    /* Write filename */
    if (fwrite(entry->filename, 1, entry->filename_length, archive->file) != entry->filename_length) {
        return EDGE_ZIP_ERROR_IO;
    }

    return EDGE_ZIP_OK;
}

int write_central_directory(edge_zip_archive_t* archive) {
    if (!archive || !archive->file) {
        return EDGE_ZIP_ERROR_INVALID_ARGUMENT;
    }

    if (!archive->mode_write) {
        return EDGE_ZIP_ERROR_UNSUPPORTED;
    }

    uint32_t central_dir_offset = ftell(archive->file);
    uint32_t central_dir_size = 0;

    /* Write each central directory header */
    for (int i = 0; i < archive->write_num_entries; i++) {
        edge_zip_entry_t* entry = &archive->write_entries[i];

        uint8_t cd_header[ZIP_CENTRAL_DIR_HEADER_SIZE];

        write_uint32_le(cd_header, ZIP_CENTRAL_DIR_HEADER_SIG);
        write_uint16_le(cd_header + 4, 20);  /* Version made by: 2.0 */
        write_uint16_le(cd_header + 6, 20);  /* Version needed: 2.0 */
        write_uint16_le(cd_header + 8, entry->flags);
        write_uint16_le(cd_header + 10, entry->compression_method);
        write_uint16_le(cd_header + 12, entry->last_mod_time);
        write_uint16_le(cd_header + 14, entry->last_mod_date);
        write_uint32_le(cd_header + 16, entry->crc32);
        write_uint32_le(cd_header + 20, entry->compressed_size);
        write_uint32_le(cd_header + 24, entry->uncompressed_size);
        write_uint16_le(cd_header + 28, entry->filename_length);
        write_uint16_le(cd_header + 30, 0);  /* Extra field length */
        write_uint16_le(cd_header + 32, 0);  /* Comment length */
        write_uint16_le(cd_header + 34, 0);  /* Disk number start */
        write_uint16_le(cd_header + 36, 0);  /* Internal file attributes */
        write_uint32_le(cd_header + 38, entry->is_directory ? 0x10 : 0);  /* External attrs */
        write_uint32_le(cd_header + 42, entry->local_header_offset);

        if (fwrite(cd_header, 1, ZIP_CENTRAL_DIR_HEADER_SIZE, archive->file) != ZIP_CENTRAL_DIR_HEADER_SIZE) {
            return EDGE_ZIP_ERROR_IO;
        }

        if (fwrite(entry->filename, 1, entry->filename_length, archive->file) != entry->filename_length) {
            return EDGE_ZIP_ERROR_IO;
        }

        central_dir_size += ZIP_CENTRAL_DIR_HEADER_SIZE + entry->filename_length;
    }

    /* Write end of central directory record */
    uint8_t eocd[ZIP_END_CENTRAL_DIR_SIZE];

    write_uint32_le(eocd, ZIP_END_CENTRAL_DIR_SIG);
    write_uint16_le(eocd + 4, 0);   /* Disk number */
    write_uint16_le(eocd + 6, 0);   /* Central dir disk */
    write_uint16_le(eocd + 8, archive->write_num_entries);   /* Entries on disk */
    write_uint16_le(eocd + 10, archive->write_num_entries);  /* Total entries */
    write_uint32_le(eocd + 12, central_dir_size);
    write_uint32_le(eocd + 16, central_dir_offset);
    write_uint16_le(eocd + 20, 0);  /* Comment length */

    if (fwrite(eocd, 1, ZIP_END_CENTRAL_DIR_SIZE, archive->file) != ZIP_END_CENTRAL_DIR_SIZE) {
        return EDGE_ZIP_ERROR_IO;
    }

    return EDGE_ZIP_OK;
}

int edge_zip_add_entry(edge_zip_archive_t* archive, const char* entry_name, const void* data,
    size_t data_size, edge_zip_compression_method_t compression, edge_zip_encryption_method_t encryption) {
    if (!archive || !entry_name || (!data && data_size > 0)) {
        return EDGE_ZIP_ERROR_INVALID_ARGUMENT;
    }

    if (!archive->mode_write) {
        return EDGE_ZIP_ERROR_UNSUPPORTED;
    }

    /* Expand entries array if needed */
    if (archive->write_num_entries >= archive->write_capacity) {
        int new_capacity = archive->write_capacity * 2;
        edge_zip_entry_t* new_entries = zip_realloc(
            archive,
            archive->write_entries,
            sizeof(edge_zip_entry_t) * new_capacity
        );
        if (!new_entries) {
            return EDGE_ZIP_ERROR_OUT_OF_MEMORY;
        }
        archive->write_entries = new_entries;
        archive->write_capacity = new_capacity;
    }

    edge_zip_entry_t* entry = &archive->write_entries[archive->write_num_entries];
    memset(entry, 0, sizeof(edge_zip_entry_t));

    /* Set entry metadata */
    entry->archive = archive;
    entry->filename_length = strlen(entry_name);
    entry->filename = zip_malloc(archive, entry->filename_length + 1);
    if (!entry->filename) {
        return EDGE_ZIP_ERROR_OUT_OF_MEMORY;
    }
    strcpy(entry->filename, entry_name);

    entry->uncompressed_size = data_size;
    entry->compression_method = compression;
    entry->encryption_method = encryption;
    entry->is_directory = false;
    entry->flags = 0;

    if (encryption != EDGE_ZIP_ENCRYPTION_NONE) {
        entry->flags |= ZIP_FLAG_ENCRYPTED;
    }

    /* Set modification time to current time */
    time_t now = time(NULL);
    time_t_to_dos(now, &entry->last_mod_date, &entry->last_mod_time);

    /* Calculate CRC-32 */
    entry->crc32 = edge_zip_crc32(data, data_size);

    /* Compress data if needed */
    void* compressed_data = NULL;
    size_t compressed_size = data_size;

    if (compression == EDGE_ZIP_COMPRESSION_STORE) {
        /* No compression */
        compressed_data = (void*)data;
        compressed_size = data_size;
    }
    else {
        /* Compression required */
        if (!archive->compressor.compress_fn) {
            zip_free(archive, entry->filename);
            return EDGE_ZIP_ERROR_UNSUPPORTED;
        }

        /* Allocate buffer for compressed data (worst case: same size + overhead) */
        size_t max_compressed = data_size + data_size / 1000 + 64;
        compressed_data = zip_malloc(archive, max_compressed);
        if (!compressed_data) {
            zip_free(archive, entry->filename);
            return EDGE_ZIP_ERROR_OUT_OF_MEMORY;
        }

        compressed_size = max_compressed;
        int result = archive->compressor.compress_fn(
            compressed_data, &compressed_size,
            data, data_size,
            entry->compression_method,
            archive->compressor.user_data
        );

        if (result != EDGE_ZIP_OK) {
            zip_free(archive, compressed_data);
            zip_free(archive, entry->filename);
            return EDGE_ZIP_ERROR_COMPRESSION;
        }
    }

    /* Encrypt data if needed */
    void* final_data = compressed_data;
    size_t final_size = compressed_size;

    if (encryption != EDGE_ZIP_ENCRYPTION_NONE) {
        if (!archive->encryptor.encrypt_fn) {
            if (compression != EDGE_ZIP_COMPRESSION_STORE) {
                zip_free(archive, compressed_data);
            }
            zip_free(archive, entry->filename);
            return EDGE_ZIP_ERROR_UNSUPPORTED;
        }

        size_t max_encrypted = compressed_size + compressed_size / 1000 + 64;
        void* encrypted_data = zip_malloc(archive, max_encrypted);
        if (!encrypted_data) {
            if (compression != EDGE_ZIP_COMPRESSION_STORE) {
                zip_free(archive, compressed_data);
            }
            zip_free(archive, entry->filename);
            return EDGE_ZIP_ERROR_OUT_OF_MEMORY;
        }

        final_size = max_encrypted;
        int result = archive->encryptor.encrypt_fn(
            encrypted_data, &final_size,
            compressed_data, compressed_size,
            entry->encryption_method,
            archive->encryptor.user_data
        );

        if (compression != EDGE_ZIP_COMPRESSION_STORE) {
            zip_free(archive, compressed_data);
        }

        if (result != EDGE_ZIP_OK) {
            zip_free(archive, encrypted_data);
            zip_free(archive, entry->filename);
            return EDGE_ZIP_ERROR_ENCRYPTION;
        }

        final_data = encrypted_data;
    }

    entry->compressed_size = final_size;

    /* Write local file header */
    int result = write_local_file_header(archive, entry, &entry->local_header_offset);
    if (result != EDGE_ZIP_OK) {
        if (encryption != EDGE_ZIP_ENCRYPTION_NONE || compression != EDGE_ZIP_COMPRESSION_STORE) {
            zip_free(archive, final_data);
        }
        zip_free(archive, entry->filename);
        return result;
    }

    /* Write file data */
    if (fwrite(final_data, 1, final_size, archive->file) != final_size) {
        if (encryption != EDGE_ZIP_ENCRYPTION_NONE || compression != EDGE_ZIP_COMPRESSION_STORE) {
            zip_free(archive, final_data);
        }
        zip_free(archive, entry->filename);
        return EDGE_ZIP_ERROR_IO;
    }

    /* Clean up temporary buffers */
    if (encryption != EDGE_ZIP_ENCRYPTION_NONE || compression != EDGE_ZIP_COMPRESSION_STORE) {
        zip_free(archive, final_data);
    }

    archive->write_num_entries++;
    return EDGE_ZIP_OK;
}

int edge_zip_add_file(edge_zip_archive_t* archive, const char* entry_name, 
    const char* file_path, edge_zip_compression_method_t compression, edge_zip_encryption_method_t encryption) {
    if (!archive || !entry_name || !file_path) {
        return EDGE_ZIP_ERROR_INVALID_ARGUMENT;
    }

    /* Open input file */
    FILE* input = fopen(file_path, "rb");
    if (!input) {
        return EDGE_ZIP_ERROR_IO;
    }

    /* Get file size */
    fseek(input, 0, SEEK_END);
    long file_size = ftell(input);
    fseek(input, 0, SEEK_SET);

    if (file_size < 0) {
        fclose(input);
        return EDGE_ZIP_ERROR_IO;
    }

    /* Read file data */
    void* data = zip_malloc(archive, file_size);
    if (!data) {
        fclose(input);
        return EDGE_ZIP_ERROR_OUT_OF_MEMORY;
    }

    if (fread(data, 1, file_size, input) != (size_t)file_size) {
        zip_free(archive, data);
        fclose(input);
        return EDGE_ZIP_ERROR_IO;
    }

    fclose(input);

    /* Add entry */
    int result = edge_zip_add_entry(archive, entry_name, data, file_size, compression, encryption);
    zip_free(archive, data);

    return result;
}

int edge_zip_add_directory(edge_zip_archive_t* archive, const char* directory_name) {
    if (!archive || !directory_name) {
        return EDGE_ZIP_ERROR_INVALID_ARGUMENT;
    }

    if (!archive->mode_write) {
        return EDGE_ZIP_ERROR_UNSUPPORTED;
    }

    /* Expand entries array if needed */
    if (archive->write_num_entries >= archive->write_capacity) {
        int new_capacity = archive->write_capacity * 2;
        edge_zip_entry_t* new_entries = zip_realloc(
            archive,
            archive->write_entries,
            sizeof(edge_zip_entry_t) * new_capacity
        );
        if (!new_entries) {
            return EDGE_ZIP_ERROR_OUT_OF_MEMORY;
        }
        archive->write_entries = new_entries;
        archive->write_capacity = new_capacity;
    }

    edge_zip_entry_t* entry = &archive->write_entries[archive->write_num_entries];
    memset(entry, 0, sizeof(edge_zip_entry_t));

    /* Set entry metadata */
    entry->archive = archive;
    entry->filename_length = strlen(directory_name);
    entry->filename = zip_malloc(archive, entry->filename_length + 1);
    if (!entry->filename) {
        return EDGE_ZIP_ERROR_OUT_OF_MEMORY;
    }
    strcpy(entry->filename, directory_name);

    entry->is_directory = true;
    entry->compression_method = EDGE_ZIP_COMPRESSION_STORE;
    entry->uncompressed_size = 0;
    entry->compressed_size = 0;
    entry->crc32 = 0;
    entry->flags = 0;

    /* Set modification time to current time */
    time_t now = time(NULL);
    time_t_to_dos(now, &entry->last_mod_date, &entry->last_mod_time);

    /* Write local file header */
    int result = write_local_file_header(archive, entry, &entry->local_header_offset);
    if (result != EDGE_ZIP_OK) {
        zip_free(archive, entry->filename);
        return result;
    }

    archive->write_num_entries++;
    return EDGE_ZIP_OK;
}