/**
 * @file edge_zip.c
 * @brief Main implementation of ZIP library
 */

#include "edge_zip_internal.h"
#include <string.h>
#include <stdlib.h>

#include <edge_allocator.h>

void* zip_malloc(edge_zip_archive_t* archive, size_t size) {
    if (!archive) {
        return NULL;
    }
    return edge_allocator_malloc(archive->allocator, size);
}

void zip_free(edge_zip_archive_t* archive, void* ptr) {
    if (archive) {
        edge_allocator_free(archive->allocator, ptr);
    }
}

void* zip_realloc(edge_zip_archive_t* archive, void* ptr, size_t size) {
    if (!archive) {
        return NULL;
    }

    return edge_allocator_realloc(archive->allocator, ptr, size);
}

static int g_default_allocator_initialized = 0;

const edge_allocator_t* edge_zip_pick_allocator(const edge_allocator_t* allocator) {
    static edge_allocator_t default_allocator;
    if (g_default_allocator_initialized == 0) {
        default_allocator = edge_allocator_create_default();
        g_default_allocator_initialized = 1;
    }
    return allocator ? allocator : &default_allocator;
}

int edge_zip_open(const char* filename, const edge_allocator_t* allocator, edge_zip_archive_t** archive) {
    if (!filename || !archive) {
        return EDGE_ZIP_ERROR_INVALID_ARGUMENT;
    }

    /* Use default allocator if none provided */
    const edge_allocator_t* alloc = edge_zip_pick_allocator(allocator);

    /* Allocate archive structure */
    edge_zip_archive_t* ar = edge_allocator_malloc(alloc, sizeof(edge_zip_archive_t));
    if (!ar) {
        return EDGE_ZIP_ERROR_OUT_OF_MEMORY;
    }

    memset(ar, 0, sizeof(edge_zip_archive_t));
    ar->allocator = alloc;
    ar->mode_write = false;

    /* Open file for reading */
    ar->file = fopen(filename, "rb");
    if (!ar->file) {
        edge_allocator_free(alloc, ar);
        return EDGE_ZIP_ERROR_IO;
    }

    /* Store filename */
    size_t name_len = strlen(filename);
    ar->filename = zip_malloc(ar, name_len + 1);
    if (!ar->filename) {
        fclose(ar->file);
        edge_allocator_free(alloc, ar);
        return EDGE_ZIP_ERROR_OUT_OF_MEMORY;
    }
    strcpy(ar->filename, filename);

    /* Read central directory */
    int result = read_central_directory(ar);
    if (result != EDGE_ZIP_OK) {
        zip_free(ar, ar->filename);
        fclose(ar->file);
        edge_allocator_free(alloc, ar);
        return result;
    }

    *archive = ar;
    return EDGE_ZIP_OK;
}

int edge_zip_create(const char* filename, const edge_allocator_t* allocator, edge_zip_archive_t** archive) {
    if (!filename || !archive) {
        return EDGE_ZIP_ERROR_INVALID_ARGUMENT;
    }

    /* Use default allocator if none provided */
    const edge_allocator_t* alloc = edge_zip_pick_allocator(allocator);

    /* Allocate archive structure */
    edge_zip_archive_t* ar = edge_allocator_malloc(alloc, sizeof(edge_zip_archive_t));
    if (!ar) {
        return EDGE_ZIP_ERROR_OUT_OF_MEMORY;
    }

    memset(ar, 0, sizeof(edge_zip_archive_t));
    ar->allocator = alloc;
    ar->mode_write = true;

    /* Open file for writing */
    ar->file = fopen(filename, "wb");
    if (!ar->file) {
        edge_allocator_free(alloc, ar);
        return EDGE_ZIP_ERROR_IO;
    }

    /* Store filename */
    size_t name_len = strlen(filename);
    ar->filename = zip_malloc(ar, name_len + 1);
    if (!ar->filename) {
        fclose(ar->file);
        edge_allocator_free(alloc, ar);
        return EDGE_ZIP_ERROR_OUT_OF_MEMORY;
    }
    strcpy(ar->filename, filename);

    /* Initialize writing state */
    ar->write_capacity = 16;
    ar->write_entries = zip_malloc(ar, sizeof(edge_zip_entry_t) * ar->write_capacity);
    if (!ar->write_entries) {
        zip_free(ar, ar->filename);
        fclose(ar->file);
        edge_allocator_free(alloc, ar);
        return EDGE_ZIP_ERROR_OUT_OF_MEMORY;
    }

    *archive = ar;
    return EDGE_ZIP_OK;
}

int edge_zip_close(edge_zip_archive_t* archive) {
    if (!archive) {
        return EDGE_ZIP_ERROR_INVALID_ARGUMENT;
    }

    int result = EDGE_ZIP_OK;

    /* Write central directory if in write mode */
    if (archive->mode_write) {
        result = write_central_directory(archive);
    }

    /* Clean up entries */
    if (archive->entries) {
        for (int i = 0; i < archive->num_entries; i++) {
            zip_free(archive, archive->entries[i].filename);
        }
        zip_free(archive, archive->entries);
    }

    /* Clean up write entries */
    if (archive->write_entries) {
        for (int i = 0; i < archive->write_num_entries; i++) {
            zip_free(archive, archive->write_entries[i].filename);
        }
        zip_free(archive, archive->write_entries);
    }

    /* Close file */
    if (archive->file) {
        fclose(archive->file);
    }

    /* Free filename */
    zip_free(archive, archive->filename);

    /* Free archive structure */
    const edge_allocator_t* alloc = archive->allocator;
    edge_allocator_free(alloc, archive);

    return result;
}

int edge_zip_get_num_entries(const edge_zip_archive_t* archive) {
    if (!archive) {
        return -1;
    }

    if (archive->mode_write) {
        return archive->write_num_entries;
    }
    else {
        return archive->num_entries;
    }
}

int edge_zip_set_compressor(edge_zip_archive_t* archive, const edge_zip_compressor_t* compressor) {
    if (!archive || !compressor) {
        return EDGE_ZIP_ERROR_INVALID_ARGUMENT;
    }

    archive->compressor = *compressor;
    return EDGE_ZIP_OK;
}

int edge_zip_set_encryptor(edge_zip_archive_t* archive, const edge_zip_encryptor_t* encryptor) {
    if (!archive || !encryptor) {
        return EDGE_ZIP_ERROR_INVALID_ARGUMENT;
    }

    archive->encryptor = *encryptor;
    return EDGE_ZIP_OK;
}

const char* edge_zip_error_string(edge_zip_error_t error) {
    switch (error) {
    case EDGE_ZIP_OK:
        return "Success";
    case EDGE_ZIP_ERROR_INVALID_ARGUMENT:
        return "Invalid argument";
    case EDGE_ZIP_ERROR_OUT_OF_MEMORY:
        return "Out of memory";
    case EDGE_ZIP_ERROR_IO:
        return "I/O error";
    case EDGE_ZIP_ERROR_CORRUPT_ARCHIVE:
        return "Corrupt archive";
    case EDGE_ZIP_ERROR_NOT_FOUND:
        return "Entry not found";
    case EDGE_ZIP_ERROR_COMPRESSION:
        return "Compression error";
    case EDGE_ZIP_ERROR_DECOMPRESSION:
        return "Decompression error";
    case EDGE_ZIP_ERROR_ENCRYPTION:
        return "Encryption error";
    case EDGE_ZIP_ERROR_DECRYPTION:
        return "Decryption error";
    case EDGE_ZIP_ERROR_UNSUPPORTED:
        return "Unsupported feature";
    case EDGE_ZIP_ERROR_CALLBACK:
        return "Callback error";
    default:
        return "Unknown error";
    }
}

const char* edge_zip_version(void) {
    static char version[32];
    snprintf(version, sizeof(version), "%d.%d.%d",
        EDGE_ZIP_VERSION_MAJOR,
        EDGE_ZIP_VERSION_MINOR,
        EDGE_ZIP_VERSION_PATCH);
    return version;
}