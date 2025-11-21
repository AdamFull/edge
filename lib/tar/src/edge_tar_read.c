/**
 * @file edge_tar_read.c
 * @brief TAR archive reading implementation
 */

#include "edge_tar_internal.h"
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <unistd.h>
#endif

int parse_tar_header(edge_tar_archive_t* archive, const tar_header_t* header, edge_tar_entry_t* entry) {
    if (!archive || !header || !entry) {
        return EDGE_TAR_ERROR_INVALID_ARGUMENT;
    }

    memset(entry, 0, sizeof(edge_tar_entry_t));
    entry->archive = archive;

    /* Build full filename from name and prefix */
    char fullname[TAR_NAME_SIZE + TAR_PREFIX_SIZE + 2];
    if (header->prefix[0] != '\0') {
        size_t prefix_len = strnlen(header->prefix, TAR_PREFIX_SIZE);
        size_t name_len = strnlen(header->name, TAR_NAME_SIZE);

        memcpy(fullname, header->prefix, prefix_len);
        fullname[prefix_len] = '/';
        memcpy(fullname + prefix_len + 1, header->name, name_len);
        fullname[prefix_len + 1 + name_len] = '\0';
    }
    else {
        strncpy(fullname, header->name, TAR_NAME_SIZE);
        fullname[TAR_NAME_SIZE] = '\0';
    }

    entry->filename_length = strlen(fullname);
    entry->filename = tar_strdup(archive, fullname);
    if (!entry->filename) {
        return EDGE_TAR_ERROR_OUT_OF_MEMORY;
    }

    /* Parse linkname if present */
    if (header->linkname[0] != '\0') {
        char linkname[TAR_LINKNAME_SIZE + 1];
        strncpy(linkname, header->linkname, TAR_LINKNAME_SIZE);
        linkname[TAR_LINKNAME_SIZE] = '\0';
        entry->linkname_length = strlen(linkname);
        entry->linkname = tar_strdup(archive, linkname);
        if (!entry->linkname) {
            tar_free(archive, entry->filename);
            return EDGE_TAR_ERROR_OUT_OF_MEMORY;
        }
    }

    /* Parse numeric fields */
    entry->mode = (uint32_t)parse_octal(header->mode, TAR_MODE_SIZE);
    entry->uid = (uint32_t)parse_octal(header->uid, TAR_UID_SIZE);
    entry->gid = (uint32_t)parse_octal(header->gid, TAR_GID_SIZE);
    entry->size = parse_octal(header->size, TAR_SIZE_SIZE);
    entry->modified_time = (time_t)parse_octal(header->mtime, TAR_MTIME_SIZE);
    entry->checksum = (uint32_t)parse_octal(header->chksum, TAR_CHKSUM_SIZE);

    /* Parse type flag */
    entry->type = (edge_tar_entry_type_t)header->typeflag[0];
    if (entry->type == '\0') {
        entry->type = EDGE_TAR_TYPE_REGULAR_ALT;
    }

    /* Parse user/group names */
    strncpy(entry->uname, header->uname, TAR_UNAME_SIZE);
    entry->uname[TAR_UNAME_SIZE - 1] = '\0';
    strncpy(entry->gname, header->gname, TAR_GNAME_SIZE);
    entry->gname[TAR_GNAME_SIZE - 1] = '\0';

    /* Parse device numbers */
    entry->devmajor = (uint32_t)parse_octal(header->devmajor, TAR_DEVMAJOR_SIZE);
    entry->devminor = (uint32_t)parse_octal(header->devminor, TAR_DEVMINOR_SIZE);

    /* Detect format */
    entry->format = detect_tar_format(header);

    return EDGE_TAR_OK;
}

int read_all_entries(edge_tar_archive_t* archive) {
    if (!archive) {
        return EDGE_TAR_ERROR_INVALID_ARGUMENT;
    }

    tar_header_t header;
    int zero_blocks = 0;

    while (1) {
        /* Read header block */
        int result = read_tar_header(archive, &header);
        if (result != EDGE_TAR_OK) {
            /* End of file is expected */
            if (result == EDGE_TAR_ERROR_CORRUPT_ARCHIVE && feof(archive->file)) {
                break;
            }
            return result;
        }

        /* Check for end-of-archive marker (two consecutive zero blocks) */
        if (is_zero_block(&header)) {
            zero_blocks++;
            if (zero_blocks >= 2) {
                break;
            }
            continue;
        }
        zero_blocks = 0;

        /* Verify checksum */
        if (!verify_checksum(&header)) {
            return EDGE_TAR_ERROR_CORRUPT_ARCHIVE;
        }

        /* Expand entries array if needed */
        if (archive->num_entries >= archive->entries_capacity) {
            archive->entries_capacity *= 2;
            edge_tar_entry_t* new_entries = tar_realloc(archive, archive->entries,
                sizeof(edge_tar_entry_t) * archive->entries_capacity);
            if (!new_entries) {
                return EDGE_TAR_ERROR_OUT_OF_MEMORY;
            }
            archive->entries = new_entries;
        }

        /* Parse header into entry */
        edge_tar_entry_t* entry = &archive->entries[archive->num_entries];
        result = parse_tar_header(archive, &header, entry);
        if (result != EDGE_TAR_OK) {
            return result;
        }

        /* Store file offset (after header) */
        entry->offset = archive->current_pos;

        /* Skip file data and padding */
        if (entry->size > 0) {
            uint64_t skip_size = round_up_to_block(entry->size);
            if (fseek(archive->file, (long)skip_size, SEEK_CUR) != 0) {
                return EDGE_TAR_ERROR_IO;
            }
            archive->current_pos += skip_size;
        }

        archive->num_entries++;
    }

    return EDGE_TAR_OK;
}

int edge_tar_entry_read(edge_tar_entry_t* entry, void* buffer, size_t buffer_size, size_t* bytes_read) {
    if (!entry || !buffer || !bytes_read) {
        return EDGE_TAR_ERROR_INVALID_ARGUMENT;
    }

    edge_tar_archive_t* archive = entry->archive;
    if (!archive || archive->mode_write) {
        return EDGE_TAR_ERROR_INVALID_ARGUMENT;
    }

    /* Calculate actual bytes to read */
    size_t read_size = (size_t)(entry->size < buffer_size ? entry->size : buffer_size);

    /* Seek to entry data */
    if (fseek(archive->file, (long)entry->offset, SEEK_SET) != 0) {
        return EDGE_TAR_ERROR_IO;
    }

    /* Read data */
    size_t actual_read = fread(buffer, 1, read_size, archive->file);
    if (actual_read != read_size) {
        return EDGE_TAR_ERROR_IO;
    }

    *bytes_read = actual_read;
    return EDGE_TAR_OK;
}

int edge_tar_entry_extract(edge_tar_entry_t* entry, const char* output_path) {
    if (!entry || !output_path) {
        return EDGE_TAR_ERROR_INVALID_ARGUMENT;
    }

    edge_tar_archive_t* archive = entry->archive;
    if (!archive || archive->mode_write) {
        return EDGE_TAR_ERROR_INVALID_ARGUMENT;
    }

    /* Handle directories */
    if (entry->type == EDGE_TAR_TYPE_DIRECTORY) {
        if (mkdir(output_path, entry->mode) != 0) {
            /* Ignore error if directory already exists */
        }
        return EDGE_TAR_OK;
    }

    /* Handle symbolic links */
    if (entry->type == EDGE_TAR_TYPE_SYMLINK) {
#ifndef _WIN32
        if (symlink(entry->linkname, output_path) != 0) {
            return EDGE_TAR_ERROR_IO;
        }
#endif
        return EDGE_TAR_OK;
    }

    /* Handle regular files */
    if (entry->type != EDGE_TAR_TYPE_REGULAR && entry->type != EDGE_TAR_TYPE_REGULAR_ALT) {
        return EDGE_TAR_ERROR_UNSUPPORTED;
    }

    /* Open output file */
    FILE* output = fopen(output_path, "wb");
    if (!output) {
        return EDGE_TAR_ERROR_IO;
    }

    /* Seek to entry data */
    if (fseek(archive->file, (long)entry->offset, SEEK_SET) != 0) {
        fclose(output);
        return EDGE_TAR_ERROR_IO;
    }

    /* Copy data in chunks */
    uint8_t buffer[8192];
    uint64_t remaining = entry->size;

    while (remaining > 0) {
        size_t chunk_size = (remaining < sizeof(buffer)) ? (size_t)remaining : sizeof(buffer);

        size_t bytes_read = fread(buffer, 1, chunk_size, archive->file);
        if (bytes_read != chunk_size) {
            fclose(output);
            return EDGE_TAR_ERROR_IO;
        }

        size_t bytes_written = fwrite(buffer, 1, bytes_read, output);
        if (bytes_written != bytes_read) {
            fclose(output);
            return EDGE_TAR_ERROR_IO;
        }

        remaining -= bytes_read;
    }

    fclose(output);

    /* Set file permissions */
#ifndef _WIN32
    chmod(output_path, entry->mode);
#endif

    return EDGE_TAR_OK;
}
