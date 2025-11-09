/**
 * @file edge_tar_write.c
 * @brief TAR archive writing implementation
 */

#include "edge_tar_internal.h"
#include <string.h>
#include <sys/stat.h>

#ifndef _WIN32
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#endif

int create_tar_header(const edge_tar_entry_t* entry, tar_header_t* header) {
    if (!entry || !header) {
        return EDGE_TAR_ERROR_INVALID_ARGUMENT;
    }

    memset(header, 0, sizeof(tar_header_t));

    /* Split filename into name and prefix for ustar format */
    int result = split_filename(entry->filename, header->name, header->prefix);
    if (result != EDGE_TAR_OK) {
        return result;
    }

    /* Format numeric fields in octal */
    format_octal(header->mode, TAR_MODE_SIZE, entry->mode);
    format_octal(header->uid, TAR_UID_SIZE, entry->uid);
    format_octal(header->gid, TAR_GID_SIZE, entry->gid);
    format_octal(header->size, TAR_SIZE_SIZE, entry->size);
    format_octal(header->mtime, TAR_MTIME_SIZE, entry->modified_time);
    format_octal(header->devmajor, TAR_DEVMAJOR_SIZE, entry->devmajor);
    format_octal(header->devminor, TAR_DEVMINOR_SIZE, entry->devminor);

    /* Set type flag */
    header->typeflag[0] = entry->type;

    /* Set linkname if present */
    if (entry->linkname) {
        strncpy(header->linkname, entry->linkname, TAR_LINKNAME_SIZE);
    }

    /* Set format-specific fields */
    if (entry->format == EDGE_TAR_FORMAT_USTAR || entry->format == EDGE_TAR_FORMAT_PAX) {
        memcpy(header->magic, TAR_MAGIC_USTAR, 6);
        memcpy(header->version, TAR_VERSION_USTAR, 2);

        /* Set user/group names */
        strncpy(header->uname, entry->uname, TAR_UNAME_SIZE);
        strncpy(header->gname, entry->gname, TAR_GNAME_SIZE);
    }
    else if (entry->format == EDGE_TAR_FORMAT_GNU) {
        memcpy(header->magic, TAR_MAGIC_GNU, 8);
    }

    /* Calculate and set checksum (must be done last) */
    /* First, fill checksum field with spaces */
    memset(header->chksum, ' ', TAR_CHKSUM_SIZE);

    /* Calculate checksum */
    uint32_t checksum = calculate_checksum(header);

    /* Format checksum (6 octal digits + null + space) */
    snprintf(header->chksum, TAR_CHKSUM_SIZE, "%06o", checksum);
    header->chksum[6] = '\0';
    header->chksum[7] = ' ';

    return EDGE_TAR_OK;
}

static void get_current_user_group(char* uname, char* gname) {
#ifndef _WIN32
    struct passwd* pw = getpwuid(getuid());
    struct group* gr = getgrgid(getgid());

    if (pw && pw->pw_name) {
        strncpy(uname, pw->pw_name, 31);
        uname[31] = '\0';
    }
    else {
        strcpy(uname, "user");
    }

    if (gr && gr->gr_name) {
        strncpy(gname, gr->gr_name, 31);
        gname[31] = '\0';
    }
    else {
        strcpy(gname, "group");
    }
#else
    strcpy(uname, "user");
    strcpy(gname, "group");
#endif
}

int edge_tar_add_entry(edge_tar_archive_t* archive, const char* entry_name, const void* data,
    size_t data_size, uint32_t mode) {
    if (!archive || !entry_name || (!data && data_size > 0)) {
        return EDGE_TAR_ERROR_INVALID_ARGUMENT;
    }

    if (!archive->mode_write) {
        return EDGE_TAR_ERROR_INVALID_ARGUMENT;
    }

    /* Create entry structure */
    edge_tar_entry_t entry;
    memset(&entry, 0, sizeof(edge_tar_entry_t));

    entry.filename = (char*)entry_name;
    entry.filename_length = strlen(entry_name);
    entry.size = data_size;
    entry.mode = mode ? mode : TAR_DEFAULT_FILE_MODE;
    entry.modified_time = time(NULL);
    entry.type = EDGE_TAR_TYPE_REGULAR;
    entry.format = archive->format;

    /* Set user/group information */
#ifndef _WIN32
    entry.uid = getuid();
    entry.gid = getgid();
#else
    entry.uid = 0;
    entry.gid = 0;
#endif

    get_current_user_group(entry.uname, entry.gname);

    /* Create and write header */
    tar_header_t header;
    int result = create_tar_header(&entry, &header);
    if (result != EDGE_TAR_OK) {
        return result;
    }

    result = write_tar_header(archive, &header);
    if (result != EDGE_TAR_OK) {
        return result;
    }

    /* Write data if present */
    if (data_size > 0 && data) {
        if (fwrite(data, 1, data_size, archive->file) != data_size) {
            return EDGE_TAR_ERROR_IO;
        }
        archive->write_offset += data_size;

        /* Write padding to block boundary */
        result = write_padding(archive, data_size);
        if (result != EDGE_TAR_OK) {
            return result;
        }
    }

    archive->write_num_entries++;
    return EDGE_TAR_OK;
}

int edge_tar_add_file(edge_tar_archive_t* archive, const char* entry_name, const char* file_path) {
    if (!archive || !entry_name || !file_path) {
        return EDGE_TAR_ERROR_INVALID_ARGUMENT;
    }

    if (!archive->mode_write) {
        return EDGE_TAR_ERROR_INVALID_ARGUMENT;
    }

    /* Get file information */
    struct stat st;
    if (stat(file_path, &st) != 0) {
        return EDGE_TAR_ERROR_IO;
    }

    /* Open input file */
    FILE* input = fopen(file_path, "rb");
    if (!input) {
        return EDGE_TAR_ERROR_IO;
    }

    /* Create entry structure */
    edge_tar_entry_t entry;
    memset(&entry, 0, sizeof(edge_tar_entry_t));

    entry.filename = (char*)entry_name;
    entry.filename_length = strlen(entry_name);
    entry.size = st.st_size;
    entry.mode = st.st_mode & 0777;
    entry.modified_time = st.st_mtime;
    entry.type = EDGE_TAR_TYPE_REGULAR;
    entry.format = archive->format;

    /* Set user/group information */
#ifndef _WIN32
    entry.uid = st.st_uid;
    entry.gid = st.st_gid;

    struct passwd* pw = getpwuid(st.st_uid);
    struct group* gr = getgrgid(st.st_gid);

    if (pw && pw->pw_name) {
        strncpy(entry.uname, pw->pw_name, 31);
        entry.uname[31] = '\0';
    }
    else {
        strcpy(entry.uname, "user");
    }

    if (gr && gr->gr_name) {
        strncpy(entry.gname, gr->gr_name, 31);
        entry.gname[31] = '\0';
    }
    else {
        strcpy(entry.gname, "group");
    }
#else
    entry.uid = 0;
    entry.gid = 0;
    strcpy(entry.uname, "user");
    strcpy(entry.gname, "group");
#endif

    /* Create and write header */
    tar_header_t header;
    int result = create_tar_header(&entry, &header);
    if (result != EDGE_TAR_OK) {
        fclose(input);
        return result;
    }

    result = write_tar_header(archive, &header);
    if (result != EDGE_TAR_OK) {
        fclose(input);
        return result;
    }

    /* Copy file data in chunks */
    uint8_t buffer[8192];
    uint64_t remaining = entry.size;

    while (remaining > 0) {
        size_t chunk_size = (remaining < sizeof(buffer)) ? (size_t)remaining : sizeof(buffer);

        size_t bytes_read = fread(buffer, 1, chunk_size, input);
        if (bytes_read != chunk_size) {
            fclose(input);
            return EDGE_TAR_ERROR_IO;
        }

        size_t bytes_written = fwrite(buffer, 1, bytes_read, archive->file);
        if (bytes_written != bytes_read) {
            fclose(input);
            return EDGE_TAR_ERROR_IO;
        }

        archive->write_offset += bytes_read;
        remaining -= bytes_read;
    }

    fclose(input);

    /* Write padding to block boundary */
    result = write_padding(archive, entry.size);
    if (result != EDGE_TAR_OK) {
        return result;
    }

    archive->write_num_entries++;
    return EDGE_TAR_OK;
}

int edge_tar_add_directory(edge_tar_archive_t* archive, const char* directory_name, uint32_t mode) {
    if (!archive || !directory_name) {
        return EDGE_TAR_ERROR_INVALID_ARGUMENT;
    }

    if (!archive->mode_write) {
        return EDGE_TAR_ERROR_INVALID_ARGUMENT;
    }

    /* Ensure directory name ends with '/' */
    size_t name_len = strlen(directory_name);
    char* dir_name = tar_malloc(archive, name_len + 2);
    if (!dir_name) {
        return EDGE_TAR_ERROR_OUT_OF_MEMORY;
    }

    strcpy(dir_name, directory_name);
    if (dir_name[name_len - 1] != '/') {
        dir_name[name_len] = '/';
        dir_name[name_len + 1] = '\0';
    }

    /* Create entry structure */
    edge_tar_entry_t entry;
    memset(&entry, 0, sizeof(edge_tar_entry_t));

    entry.filename = dir_name;
    entry.filename_length = strlen(dir_name);
    entry.size = 0;
    entry.mode = mode ? mode : TAR_DEFAULT_DIR_MODE;
    entry.modified_time = time(NULL);
    entry.type = EDGE_TAR_TYPE_DIRECTORY;
    entry.format = archive->format;

    /* Set user/group information */
#ifndef _WIN32
    entry.uid = getuid();
    entry.gid = getgid();
#else
    entry.uid = 0;
    entry.gid = 0;
#endif

    get_current_user_group(entry.uname, entry.gname);

    /* Create and write header */
    tar_header_t header;
    int result = create_tar_header(&entry, &header);

    tar_free(archive, dir_name);

    if (result != EDGE_TAR_OK) {
        return result;
    }

    result = write_tar_header(archive, &header);
    if (result != EDGE_TAR_OK) {
        return result;
    }

    archive->write_num_entries++;
    return EDGE_TAR_OK;
}

int edge_tar_add_symlink(edge_tar_archive_t* archive, const char* link_name, const char* target_path) {
    if (!archive || !link_name || !target_path) {
        return EDGE_TAR_ERROR_INVALID_ARGUMENT;
    }

    if (!archive->mode_write) {
        return EDGE_TAR_ERROR_INVALID_ARGUMENT;
    }

    /* Check target path length */
    if (strlen(target_path) >= TAR_LINKNAME_SIZE) {
        return EDGE_TAR_ERROR_UNSUPPORTED;
    }

    /* Create entry structure */
    edge_tar_entry_t entry;
    memset(&entry, 0, sizeof(edge_tar_entry_t));

    entry.filename = (char*)link_name;
    entry.filename_length = strlen(link_name);
    entry.linkname = (char*)target_path;
    entry.linkname_length = strlen(target_path);
    entry.size = 0;
    entry.mode = 0777;
    entry.modified_time = time(NULL);
    entry.type = EDGE_TAR_TYPE_SYMLINK;
    entry.format = archive->format;

    /* Set user/group information */
#ifndef _WIN32
    entry.uid = getuid();
    entry.gid = getgid();
#else
    entry.uid = 0;
    entry.gid = 0;
#endif

    get_current_user_group(entry.uname, entry.gname);

    /* Create and write header */
    tar_header_t header;
    int result = create_tar_header(&entry, &header);
    if (result != EDGE_TAR_OK) {
        return result;
    }

    result = write_tar_header(archive, &header);
    if (result != EDGE_TAR_OK) {
        return result;
    }

    archive->write_num_entries++;
    return EDGE_TAR_OK;
}