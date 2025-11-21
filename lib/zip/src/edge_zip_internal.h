/**
 * @file edge_zip_internal.h
 * @brief Internal structures and constants for ZIP file format
 */

#ifndef EDGE_ZIP_INTERNAL_H
#define EDGE_ZIP_INTERNAL_H

#include "edge_zip.h"
#include <stdio.h>

 /* ZIP file format signatures */
#define ZIP_LOCAL_FILE_HEADER_SIG   0x04034b50
#define ZIP_CENTRAL_DIR_HEADER_SIG  0x02014b50
#define ZIP_END_CENTRAL_DIR_SIG     0x06054b50
#define ZIP_DATA_DESCRIPTOR_SIG     0x08074b50

/* ZIP file format sizes */
#define ZIP_LOCAL_FILE_HEADER_SIZE  30
#define ZIP_CENTRAL_DIR_HEADER_SIZE 46
#define ZIP_END_CENTRAL_DIR_SIZE    22
#define ZIP_DATA_DESCRIPTOR_SIZE    16

/* General purpose bit flags */
#define ZIP_FLAG_ENCRYPTED          0x0001
#define ZIP_FLAG_DATA_DESCRIPTOR    0x0008
#define ZIP_FLAG_UTF8               0x0800

/* Maximum sizes */
#define ZIP_MAX_FILENAME_LENGTH     65535
#define ZIP_MAX_EXTRA_LENGTH        65535
#define ZIP_MAX_COMMENT_LENGTH      65535

/* ============================================================================
 * Internal structures
 * ============================================================================ */

 /**
  * @brief ZIP local file header
  */
typedef struct {
    uint32_t signature;             /* 0x04034b50 */
    uint16_t version_needed;        /* Version needed to extract */
    uint16_t flags;                 /* General purpose bit flag */
    uint16_t compression_method;    /* Compression method */
    uint16_t last_mod_time;         /* Last modification time */
    uint16_t last_mod_date;         /* Last modification date */
    uint32_t crc32;                 /* CRC-32 */
    uint32_t compressed_size;       /* Compressed size */
    uint32_t uncompressed_size;     /* Uncompressed size */
    uint16_t filename_length;       /* Filename length */
    uint16_t extra_length;          /* Extra field length */
} zip_local_file_header_t;

/**
 * @brief ZIP central directory header
 */
typedef struct {
    uint32_t signature;             /* 0x02014b50 */
    uint16_t version_made_by;       /* Version made by */
    uint16_t version_needed;        /* Version needed to extract */
    uint16_t flags;                 /* General purpose bit flag */
    uint16_t compression_method;    /* Compression method */
    uint16_t last_mod_time;         /* Last modification time */
    uint16_t last_mod_date;         /* Last modification date */
    uint32_t crc32;                 /* CRC-32 */
    uint32_t compressed_size;       /* Compressed size */
    uint32_t uncompressed_size;     /* Uncompressed size */
    uint16_t filename_length;       /* Filename length */
    uint16_t extra_length;          /* Extra field length */
    uint16_t comment_length;        /* File comment length */
    uint16_t disk_number;           /* Disk number start */
    uint16_t internal_attrs;        /* Internal file attributes */
    uint32_t external_attrs;        /* External file attributes */
    uint32_t local_header_offset;   /* Offset of local header */
} zip_central_dir_header_t;

/**
 * @brief ZIP end of central directory record
 */
typedef struct {
    uint32_t signature;             /* 0x06054b50 */
    uint16_t disk_number;           /* Number of this disk */
    uint16_t central_dir_disk;      /* Disk with central directory */
    uint16_t num_entries_disk;      /* Entries on this disk */
    uint16_t num_entries_total;     /* Total entries */
    uint32_t central_dir_size;      /* Size of central directory */
    uint32_t central_dir_offset;    /* Offset of central directory */
    uint16_t comment_length;        /* Comment length */
} zip_end_central_dir_t;

/**
 * @brief Entry structure (internal)
 */
struct edge_zip_entry {
    edge_zip_archive_t* archive;      /* Parent archive */
    char* filename;                 /* Entry filename */
    size_t filename_length;         /* Filename length */
    uint32_t crc32;                 /* CRC-32 checksum */
    uint32_t compressed_size;       /* Compressed size */
    uint32_t uncompressed_size;     /* Uncompressed size */
    uint32_t local_header_offset;   /* Offset in file */
    uint16_t compression_method;    /* Compression method */
    uint16_t encryption_method;     /* Encryption method (custom) */
    uint16_t flags;                 /* General purpose flags */
    uint16_t last_mod_time;         /* DOS time */
    uint16_t last_mod_date;         /* DOS date */
    bool is_directory;              /* Directory flag */
};

/**
 * @brief Archive structure (internal)
 */
struct edge_zip_archive {
    FILE* file;                     /* File handle */
    char* filename;                 /* Archive filename */
    bool mode_write;                /* Write mode flag */
    const edge_allocator_t* allocator;     /* Memory allocator */
    edge_zip_compressor_t compressor; /* Compression callbacks */
    edge_zip_encryptor_t encryptor;   /* Encryption callbacks */

    /* Reading state */
    edge_zip_entry_t* entries;        /* Array of entries */
    int num_entries;                /* Number of entries */
    uint32_t central_dir_offset;    /* Central directory offset */

    /* Writing state */
    uint32_t current_offset;        /* Current write position */
    edge_zip_entry_t* write_entries;  /* Entries being written */
    int write_num_entries;          /* Number of entries written */
    int write_capacity;             /* Capacity of write_entries */
};

/* ============================================================================
 * Internal utility functions
 * ============================================================================ */

 /**
  * @brief Allocate memory using archive allocator
  */
void* zip_malloc(edge_zip_archive_t* archive, size_t size);

/**
 * @brief Free memory using archive allocator
 */
void zip_free(edge_zip_archive_t* archive, void* ptr);

/**
 * @brief Reallocate memory using archive allocator
 */
void* zip_realloc(edge_zip_archive_t* archive, void* ptr, size_t size);

/**
 * @brief Read little-endian uint16
 */
uint16_t read_uint16_le(const uint8_t* buf);

/**
 * @brief Read little-endian uint32
 */
uint32_t read_uint32_le(const uint8_t* buf);

/**
 * @brief Write little-endian uint16
 */
void write_uint16_le(uint8_t* buf, uint16_t value);

/**
 * @brief Write little-endian uint32
 */
void write_uint32_le(uint8_t* buf, uint32_t value);

/**
 * @brief Convert DOS date/time to time_t
 */
time_t dos_to_time_t(uint16_t dos_date, uint16_t dos_time);

/**
 * @brief Convert time_t to DOS date/time
 */
void time_t_to_dos(time_t t, uint16_t* dos_date, uint16_t* dos_time);

/**
 * @brief Find end of central directory record
 */
int find_end_central_dir(FILE* file, zip_end_central_dir_t* eocd);

/**
 * @brief Read central directory
 */
int read_central_directory(edge_zip_archive_t* archive);

/**
 * @brief Write local file header
 */
int write_local_file_header(edge_zip_archive_t* archive, const edge_zip_entry_t* entry, uint32_t* offset);

/**
 * @brief Write central directory
 */
int write_central_directory(edge_zip_archive_t* archive);

#endif /* EDGE_ZIP_INTERNAL_H */
