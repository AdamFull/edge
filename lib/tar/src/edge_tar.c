/**
 * @file edge_tar.c
 * @brief Main implementation of TAR library
 */

#include "edge_tar_internal.h"
#include <string.h>
#include <stdlib.h>

#include <edge_allocator.h>

void* tar_malloc(edge_tar_archive_t* archive, size_t size) {
    if (!archive) {
        return NULL;
    }
    return edge_allocator_malloc(archive->allocator, size);
}

void tar_free(edge_tar_archive_t* archive, void* ptr) {
    if (archive) {
        edge_allocator_free(archive->allocator, ptr);
    }
}

void* tar_realloc(edge_tar_archive_t* archive, void* ptr, size_t size) {
    if (!archive) {
        return NULL;
    }
    return edge_allocator_realloc(archive->allocator, ptr, size);
}

char* tar_strdup(edge_tar_archive_t* archive, const char* str) {
    if (!archive || !str) {
        return NULL;
    }
    return edge_allocator_strdup(archive->allocator, str);
}

static int g_default_allocator_initialized = 0;

const edge_allocator_t* edge_tar_pick_allocator(const edge_allocator_t* allocator) {
    static edge_allocator_t default_allocator;
    if (g_default_allocator_initialized == 0) {
        default_allocator = edge_allocator_create_default();
        g_default_allocator_initialized = 1;
    }
    return allocator ? allocator : &default_allocator;
}

int edge_tar_open(const char* filename, const edge_allocator_t* allocator, edge_tar_archive_t** archive) {
    if (!filename || !archive) {
        return EDGE_TAR_ERROR_INVALID_ARGUMENT;
    }

    /* Use default allocator if none provided */
    const edge_allocator_t* alloc = edge_tar_pick_allocator(allocator);

    /* Allocate archive structure */
    edge_tar_archive_t* ar = edge_allocator_malloc(alloc, sizeof(edge_tar_archive_t));
    if (!ar) {
        return EDGE_TAR_ERROR_OUT_OF_MEMORY;
    }

    memset(ar, 0, sizeof(edge_tar_archive_t));
    ar->allocator = alloc;
    ar->mode_write = false;
    ar->format = EDGE_TAR_FORMAT_USTAR;

    /* Open file for reading */
    ar->file = fopen(filename, "rb");
    if (!ar->file) {
        edge_allocator_free(alloc, ar);
        return EDGE_TAR_ERROR_IO;
    }

    /* Store filename */
    size_t name_len = strlen(filename);
    ar->filename = tar_malloc(ar, name_len + 1);
    if (!ar->filename) {
        fclose(ar->file);
        edge_allocator_free(alloc, ar);
        return EDGE_TAR_ERROR_OUT_OF_MEMORY;
    }
    strcpy(ar->filename, filename);

    /* Initialize entries array */
    ar->entries_capacity = 16;
    ar->entries = tar_malloc(ar, sizeof(edge_tar_entry_t) * ar->entries_capacity);
    if (!ar->entries) {
        tar_free(ar, ar->filename);
        fclose(ar->file);
        edge_allocator_free(alloc, ar);
        return EDGE_TAR_ERROR_OUT_OF_MEMORY;
    }

    /* Read all entries */
    int result = read_all_entries(ar);
    if (result != EDGE_TAR_OK) {
        tar_free(ar, ar->entries);
        tar_free(ar, ar->filename);
        fclose(ar->file);
        edge_allocator_free(alloc, ar);
        return result;
    }

    *archive = ar;
    return EDGE_TAR_OK;
}

int edge_tar_create(const char* filename, edge_tar_format_t format, const edge_allocator_t* allocator, edge_tar_archive_t** archive) {
    if (!filename || !archive) {
        return EDGE_TAR_ERROR_INVALID_ARGUMENT;
    }

    /* Use default allocator if none provided */
    const edge_allocator_t* alloc = edge_tar_pick_allocator(allocator);

    /* Allocate archive structure */
    edge_tar_archive_t* ar = edge_allocator_malloc(alloc, sizeof(edge_tar_archive_t));
    if (!ar) {
        return EDGE_TAR_ERROR_OUT_OF_MEMORY;
    }

    memset(ar, 0, sizeof(edge_tar_archive_t));
    ar->allocator = alloc;
    ar->mode_write = true;
    ar->format = format;

    /* Open file for writing */
    ar->file = fopen(filename, "wb");
    if (!ar->file) {
        edge_allocator_free(alloc, ar);
        return EDGE_TAR_ERROR_IO;
    }

    /* Store filename */
    size_t name_len = strlen(filename);
    ar->filename = tar_malloc(ar, name_len + 1);
    if (!ar->filename) {
        fclose(ar->file);
        edge_allocator_free(alloc, ar);
        return EDGE_TAR_ERROR_OUT_OF_MEMORY;
    }
    strcpy(ar->filename, filename);

    *archive = ar;
    return EDGE_TAR_OK;
}

int edge_tar_close(edge_tar_archive_t* archive) {
    if (!archive) {
        return EDGE_TAR_ERROR_INVALID_ARGUMENT;
    }

    int result = EDGE_TAR_OK;

    /* Write end-of-archive marker if in write mode */
    if (archive->mode_write) {
        result = write_end_marker(archive);
    }

    /* Clean up entries */
    if (archive->entries) {
        for (int i = 0; i < archive->num_entries; i++) {
            tar_free(archive, archive->entries[i].filename);
            tar_free(archive, archive->entries[i].linkname);
        }
        tar_free(archive, archive->entries);
    }

    /* Close file */
    if (archive->file) {
        fclose(archive->file);
    }

    /* Free filename */
    tar_free(archive, archive->filename);

    /* Free archive structure */
    const edge_allocator_t* alloc = archive->allocator;
    edge_allocator_free(alloc, archive);

    return result;
}

int edge_tar_get_num_entries(const edge_tar_archive_t* archive) {
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

int edge_tar_get_entry_by_index(edge_tar_archive_t* archive, int index, edge_tar_entry_t** entry) {
    if (!archive || !entry || index < 0) {
        return EDGE_TAR_ERROR_INVALID_ARGUMENT;
    }

    if (index >= archive->num_entries) {
        return EDGE_TAR_ERROR_NOT_FOUND;
    }

    *entry = &archive->entries[index];
    return EDGE_TAR_OK;
}

int edge_tar_find_entry(edge_tar_archive_t* archive, const char* name, edge_tar_entry_t** entry) {
    if (!archive || !name || !entry) {
        return EDGE_TAR_ERROR_INVALID_ARGUMENT;
    }

    for (int i = 0; i < archive->num_entries; i++) {
        if (strcmp(archive->entries[i].filename, name) == 0) {
            *entry = &archive->entries[i];
            return EDGE_TAR_OK;
        }
    }

    return EDGE_TAR_ERROR_NOT_FOUND;
}

int edge_tar_entry_get_info(const edge_tar_entry_t* entry, edge_tar_entry_info_t* info) {
    if (!entry || !info) {
        return EDGE_TAR_ERROR_INVALID_ARGUMENT;
    }

    info->filename = entry->filename;
    info->filename_length = entry->filename_length;
    info->linkname = entry->linkname;
    info->linkname_length = entry->linkname_length;
    info->size = entry->size;
    info->modified_time = entry->modified_time;
    info->mode = entry->mode;
    info->uid = entry->uid;
    info->gid = entry->gid;
    memcpy(info->uname, entry->uname, sizeof(info->uname));
    memcpy(info->gname, entry->gname, sizeof(info->gname));
    info->type = entry->type;
    info->devmajor = entry->devmajor;
    info->devminor = entry->devminor;
    info->format = entry->format;
    info->checksum = entry->checksum;

    return EDGE_TAR_OK;
}

const char* edge_tar_error_string(edge_tar_error_t error) {
    switch (error) {
    case EDGE_TAR_OK:
        return "Success";
    case EDGE_TAR_ERROR_INVALID_ARGUMENT:
        return "Invalid argument";
    case EDGE_TAR_ERROR_OUT_OF_MEMORY:
        return "Out of memory";
    case EDGE_TAR_ERROR_IO:
        return "I/O error";
    case EDGE_TAR_ERROR_CORRUPT_ARCHIVE:
        return "Corrupt archive";
    case EDGE_TAR_ERROR_NOT_FOUND:
        return "Entry not found";
    case EDGE_TAR_ERROR_UNSUPPORTED:
        return "Unsupported feature";
    case EDGE_TAR_ERROR_CALLBACK:
        return "Callback error";
    default:
        return "Unknown error";
    }
}

const char* edge_tar_version(void) {
    static char version[32];
    snprintf(version, sizeof(version), "%d.%d.%d",
        EDGE_TAR_VERSION_MAJOR,
        EDGE_TAR_VERSION_MINOR,
        EDGE_TAR_VERSION_PATCH);
    return version;
}

uint32_t edge_tar_checksum(const void* header) {
    if (!header) {
        return 0;
    }
    return calculate_checksum((const tar_header_t*)header);
}
