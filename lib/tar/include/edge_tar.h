#ifndef EDGE_TAR_H
#define EDGE_TAR_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

	/* Version information */
#define EDGE_TAR_VERSION_MAJOR 1
#define EDGE_TAR_VERSION_MINOR 0
#define EDGE_TAR_VERSION_PATCH 0

/* Forward declaration of base allocator */
typedef struct edge_allocator edge_allocator_t;

/* Error codes */
typedef enum {
    EDGE_TAR_OK = 0,
    EDGE_TAR_ERROR_INVALID_ARGUMENT = -1,
    EDGE_TAR_ERROR_OUT_OF_MEMORY = -2,
    EDGE_TAR_ERROR_IO = -3,
    EDGE_TAR_ERROR_CORRUPT_ARCHIVE = -4,
    EDGE_TAR_ERROR_NOT_FOUND = -5,
    EDGE_TAR_ERROR_UNSUPPORTED = -6,
    EDGE_TAR_ERROR_CALLBACK = -7
} edge_tar_error_t;

/* TAR format types */
typedef enum {
    EDGE_TAR_FORMAT_USTAR = 0,      /* POSIX ustar format */
    EDGE_TAR_FORMAT_PAX = 1,        /* POSIX.1-2001 pax format */
    EDGE_TAR_FORMAT_GNU = 2,        /* GNU tar format */
    EDGE_TAR_FORMAT_V7 = 3          /* Old V7 tar format */
} edge_tar_format_t;

/* Entry type flags (from TAR header typeflag) */
typedef enum {
    EDGE_TAR_TYPE_REGULAR = '0',        /* Regular file */
    EDGE_TAR_TYPE_REGULAR_ALT = '\0',   /* Regular file (alternative) */
    EDGE_TAR_TYPE_LINK = '1',           /* Hard link */
    EDGE_TAR_TYPE_SYMLINK = '2',        /* Symbolic link */
    EDGE_TAR_TYPE_CHAR = '3',           /* Character device */
    EDGE_TAR_TYPE_BLOCK = '4',          /* Block device */
    EDGE_TAR_TYPE_DIRECTORY = '5',      /* Directory */
    EDGE_TAR_TYPE_FIFO = '6',           /* FIFO (named pipe) */
    EDGE_TAR_TYPE_CONTIGUOUS = '7',     /* Contiguous file */
    EDGE_TAR_TYPE_PAX_GLOBAL = 'g',     /* PAX global extended header */
    EDGE_TAR_TYPE_PAX_EXTENDED = 'x',   /* PAX extended header */
    EDGE_TAR_TYPE_GNU_LONGNAME = 'L',   /* GNU long filename */
    EDGE_TAR_TYPE_GNU_LONGLINK = 'K'    /* GNU long link name */
} edge_tar_entry_type_t;

/* Forward declarations */
typedef struct edge_tar_archive edge_tar_archive_t;
typedef struct edge_tar_entry edge_tar_entry_t;

/**
 * @brief File entry metadata
 */
typedef struct {
    char* filename;                          /* Entry name/path */
    size_t filename_length;                  /* Length of filename */
    char* linkname;                          /* Link target (for links) */
    size_t linkname_length;                  /* Length of linkname */
    uint64_t size;                           /* File size in bytes */
    time_t modified_time;                    /* Last modification time */
    uint32_t mode;                           /* File permissions */
    uint32_t uid;                            /* User ID */
    uint32_t gid;                            /* Group ID */
    char uname[32];                          /* User name */
    char gname[32];                          /* Group name */
    edge_tar_entry_type_t type;              /* Entry type */
    uint32_t devmajor;                       /* Device major number */
    uint32_t devminor;                       /* Device minor number */
    edge_tar_format_t format;                /* TAR format */
    uint32_t checksum;                       /* Header checksum */
} edge_tar_entry_info_t;

/**
 * @brief Open an existing TAR archive for reading
 * @param filename Path to TAR file
 * @param allocator Memory allocator (NULL for default)
 * @param archive Output pointer to archive handle
 * @return EDGE_TAR_OK on success, error code on failure
 */
int edge_tar_open(const char* filename, const edge_allocator_t* allocator, edge_tar_archive_t** archive);

/**
 * @brief Create a new TAR archive for writing
 * @param filename Path to TAR file to create
 * @param format TAR format to use
 * @param allocator Memory allocator (NULL for default)
 * @param archive Output pointer to archive handle
 * @return EDGE_TAR_OK on success, error code on failure
 */
int edge_tar_create(const char* filename, edge_tar_format_t format, const edge_allocator_t* allocator, edge_tar_archive_t** archive);

/**
 * @brief Close and finalize a TAR archive
 * @param archive Archive handle
 * @return EDGE_TAR_OK on success, error code on failure
 */
int edge_tar_close(edge_tar_archive_t* archive);

/**
 * @brief Get the number of entries in the archive
 * @param archive Archive handle
 * @return Number of entries, or -1 on error
 */
int edge_tar_get_num_entries(const edge_tar_archive_t* archive);

/**
 * @brief Get entry by index
 * @param archive Archive handle
 * @param index Entry index (0-based)
 * @param entry Output pointer to entry handle
 * @return EDGE_TAR_OK on success, error code on failure
 */
int edge_tar_get_entry_by_index(edge_tar_archive_t* archive, int index, edge_tar_entry_t** entry);

/**
 * @brief Find entry by name
 * @param archive Archive handle
 * @param name Entry name/path
 * @param entry Output pointer to entry handle
 * @return EDGE_TAR_OK on success, error code on failure
 */
int edge_tar_find_entry(edge_tar_archive_t* archive, const char* name, edge_tar_entry_t** entry);

/**
 * @brief Get entry metadata
 * @param entry Entry handle
 * @param info Output pointer to metadata structure
 * @return EDGE_TAR_OK on success, error code on failure
 */
int edge_tar_entry_get_info(const edge_tar_entry_t* entry, edge_tar_entry_info_t* info);

/**
 * @brief Read entry data into buffer
 * @param entry Entry handle
 * @param buffer Output buffer
 * @param buffer_size Size of output buffer
 * @param bytes_read Output pointer for actual bytes read
 * @return EDGE_TAR_OK on success, error code on failure
 */
int edge_tar_entry_read(edge_tar_entry_t* entry, void* buffer, size_t buffer_size, size_t* bytes_read);

/**
 * @brief Extract entry to file
 * @param entry Entry handle
 * @param output_path Path to output file
 * @return EDGE_TAR_OK on success, error code on failure
 */
int edge_tar_entry_extract(edge_tar_entry_t* entry, const char* output_path);

/**
 * @brief Add a file to the archive
 * @param archive Archive handle
 * @param entry_name Name/path within archive
 * @param data File data
 * @param data_size Size of file data
 * @param mode File permissions (e.g., 0644)
 * @return EDGE_TAR_OK on success, error code on failure
 */
int edge_tar_add_entry(edge_tar_archive_t* archive, const char* entry_name, const void* data,
    size_t data_size, uint32_t mode);

/**
 * @brief Add a file from disk to the archive
 * @param archive Archive handle
 * @param entry_name Name/path within archive
 * @param file_path Path to file on disk
 * @return EDGE_TAR_OK on success, error code on failure
 */
int edge_tar_add_file(edge_tar_archive_t* archive, const char* entry_name, const char* file_path);

/**
 * @brief Add a directory entry
 * @param archive Archive handle
 * @param directory_name Directory name/path (should end with /)
 * @param mode Directory permissions (e.g., 0755)
 * @return EDGE_TAR_OK on success, error code on failure
 */
int edge_tar_add_directory(edge_tar_archive_t* archive, const char* directory_name, uint32_t mode);

/**
 * @brief Add a symbolic link entry
 * @param archive Archive handle
 * @param link_name Name/path of the symlink within archive
 * @param target_path Path that the symlink points to
 * @return EDGE_TAR_OK on success, error code on failure
 */
int edge_tar_add_symlink(edge_tar_archive_t* archive, const char* link_name, const char* target_path);

/**
 * @brief Get error message for error code
 * @param error Error code
 * @return Human-readable error message
 */
const char* edge_tar_error_string(edge_tar_error_t error);

/**
 * @brief Calculate TAR header checksum
 * @param header TAR header block (512 bytes)
 * @return Checksum value
 */
uint32_t edge_tar_checksum(const void* header);

/**
 * @brief Get library version string
 * @return Version string (e.g., "1.0.0")
 */
const char* edge_tar_version(void);

#ifdef __cplusplus
}
#endif

#endif /* EDGE_TAR_H */
