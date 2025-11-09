/**
 * @file edge_tar_internal.h
 * @brief Internal structures and constants for TAR file format
 */

#ifndef EDGE_TAR_INTERNAL_H
#define EDGE_TAR_INTERNAL_H

#include "edge_tar.h"
#include <stdio.h>

 /* TAR block size */
#define TAR_BLOCK_SIZE 512

/* TAR header field sizes */
#define TAR_NAME_SIZE       100
#define TAR_MODE_SIZE       8
#define TAR_UID_SIZE        8
#define TAR_GID_SIZE        8
#define TAR_SIZE_SIZE       12
#define TAR_MTIME_SIZE      12
#define TAR_CHKSUM_SIZE     8
#define TAR_TYPEFLAG_SIZE   1
#define TAR_LINKNAME_SIZE   100
#define TAR_MAGIC_SIZE      6
#define TAR_VERSION_SIZE    2
#define TAR_UNAME_SIZE      32
#define TAR_GNAME_SIZE      32
#define TAR_DEVMAJOR_SIZE   8
#define TAR_DEVMINOR_SIZE   8
#define TAR_PREFIX_SIZE     155

/* TAR magic values */
#define TAR_MAGIC_USTAR     "ustar"
#define TAR_MAGIC_USTAR_00  "ustar\0"
#define TAR_VERSION_USTAR   "00"
#define TAR_MAGIC_GNU       "ustar  "

/* Default permissions */
#define TAR_DEFAULT_FILE_MODE 0644
#define TAR_DEFAULT_DIR_MODE  0755

/**
 * @brief TAR header structure (POSIX ustar format)
 * This is a 512-byte block
 */
typedef struct {
    char name[TAR_NAME_SIZE];           /* File name */
    char mode[TAR_MODE_SIZE];           /* File mode (octal) */
    char uid[TAR_UID_SIZE];             /* User ID (octal) */
    char gid[TAR_GID_SIZE];             /* Group ID (octal) */
    char size[TAR_SIZE_SIZE];           /* File size (octal) */
    char mtime[TAR_MTIME_SIZE];         /* Modification time (octal) */
    char chksum[TAR_CHKSUM_SIZE];       /* Header checksum (octal) */
    char typeflag[TAR_TYPEFLAG_SIZE];   /* Entry type */
    char linkname[TAR_LINKNAME_SIZE];   /* Link name */
    char magic[TAR_MAGIC_SIZE];         /* "ustar" */
    char version[TAR_VERSION_SIZE];     /* "00" */
    char uname[TAR_UNAME_SIZE];         /* User name */
    char gname[TAR_GNAME_SIZE];         /* Group name */
    char devmajor[TAR_DEVMAJOR_SIZE];   /* Device major number (octal) */
    char devminor[TAR_DEVMINOR_SIZE];   /* Device minor number (octal) */
    char prefix[TAR_PREFIX_SIZE];       /* Filename prefix */
    char padding[12];                   /* Padding to 512 bytes */
} tar_header_t;

/**
 * @brief Entry structure (internal)
 */
struct edge_tar_entry {
    edge_tar_archive_t* archive;      /* Parent archive */
    char* filename;                   /* Entry filename */
    size_t filename_length;           /* Filename length */
    char* linkname;                   /* Link target name */
    size_t linkname_length;           /* Link name length */
    uint64_t size;                    /* File size */
    uint64_t offset;                  /* Offset in archive */
    time_t modified_time;             /* Modification time */
    uint32_t mode;                    /* File permissions */
    uint32_t uid;                     /* User ID */
    uint32_t gid;                     /* Group ID */
    char uname[32];                   /* User name */
    char gname[32];                   /* Group name */
    edge_tar_entry_type_t type;       /* Entry type */
    uint32_t devmajor;                /* Device major number */
    uint32_t devminor;                /* Device minor number */
    edge_tar_format_t format;         /* TAR format */
    uint32_t checksum;                /* Header checksum */
};

/**
 * @brief Archive structure (internal)
 */
struct edge_tar_archive {
    FILE* file;                       /* File handle */
    char* filename;                   /* Archive filename */
    bool mode_write;                  /* Write mode flag */
    edge_tar_format_t format;         /* TAR format */
    const edge_allocator_t* allocator;       /* Memory allocator */

    /* Reading state */
    edge_tar_entry_t* entries;        /* Array of entries */
    int num_entries;                  /* Number of entries */
    int entries_capacity;             /* Capacity of entries array */
    uint64_t current_pos;             /* Current position in file */

    /* Writing state */
    uint64_t write_offset;            /* Current write position */
    int write_num_entries;            /* Number of entries written */
};

/**
 * @brief Allocate memory using archive allocator
 */
void* tar_malloc(edge_tar_archive_t* archive, size_t size);

/**
 * @brief Free memory using archive allocator
 */
void tar_free(edge_tar_archive_t* archive, void* ptr);

/**
 * @brief Reallocate memory using archive allocator
 */
void* tar_realloc(edge_tar_archive_t* archive, void* ptr, size_t size);

/**
 * @brief Duplicate string using archive allocator
 */
char* tar_strdup(edge_tar_archive_t* archive, const char* str);

/**
 * @brief Parse octal string from TAR header
 */
uint64_t parse_octal(const char* str, size_t len);

/**
 * @brief Format octal string for TAR header
 */
void format_octal(char* str, size_t len, uint64_t value);

/**
 * @brief Calculate TAR header checksum
 */
uint32_t calculate_checksum(const tar_header_t* header);

/**
 * @brief Verify TAR header checksum
 */
bool verify_checksum(const tar_header_t* header);

/**
 * @brief Read TAR header from file
 */
int read_tar_header(edge_tar_archive_t* archive, tar_header_t* header);

/**
 * @brief Write TAR header to file
 */
int write_tar_header(edge_tar_archive_t* archive, const tar_header_t* header);

/**
 * @brief Parse TAR header into entry structure
 */
int parse_tar_header(edge_tar_archive_t* archive, const tar_header_t* header, edge_tar_entry_t* entry);

/**
 * @brief Create TAR header from entry information
 */
int create_tar_header(const edge_tar_entry_t* entry, tar_header_t* header);

/**
 * @brief Detect TAR format from header
 */
edge_tar_format_t detect_tar_format(const tar_header_t* header);

/**
 * @brief Check if header block is all zeros (end of archive)
 */
bool is_zero_block(const tar_header_t* header);

/**
 * @brief Round up to next block boundary
 */
uint64_t round_up_to_block(uint64_t size);

/**
 * @brief Write padding to align to block boundary
 */
int write_padding(edge_tar_archive_t* archive, uint64_t data_size);

/**
 * @brief Skip to next block boundary
 */
int skip_to_next_block(edge_tar_archive_t* archive, uint64_t data_size);

/**
 * @brief Read all entries from archive
 */
int read_all_entries(edge_tar_archive_t* archive);

/**
 * @brief Write end-of-archive marker (two zero blocks)
 */
int write_end_marker(edge_tar_archive_t* archive);

/**
 * @brief Split long filename into name and prefix for ustar format
 */
int split_filename(const char* filename, char* name, char* prefix);

#endif /* EDGE_TAR_INTERNAL_H */