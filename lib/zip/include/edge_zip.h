#ifndef EDGE_ZIP_H
#define EDGE_ZIP_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

	/* Version information */
#define EDGE_ZIP_VERSION_MAJOR 1
#define EDGE_ZIP_VERSION_MINOR 0
#define EDGE_ZIP_VERSION_PATCH 0

/* Forward declaration of base allocator */
typedef struct edge_allocator edge_allocator_t;

/* Error codes */
typedef enum {
    EDGE_ZIP_OK = 0,
    EDGE_ZIP_ERROR_INVALID_ARGUMENT = -1,
    EDGE_ZIP_ERROR_OUT_OF_MEMORY = -2,
    EDGE_ZIP_ERROR_IO = -3,
    EDGE_ZIP_ERROR_CORRUPT_ARCHIVE = -4,
    EDGE_ZIP_ERROR_NOT_FOUND = -5,
    EDGE_ZIP_ERROR_COMPRESSION = -6,
    EDGE_ZIP_ERROR_DECOMPRESSION = -7,
    EDGE_ZIP_ERROR_ENCRYPTION = -8,
    EDGE_ZIP_ERROR_DECRYPTION = -9,
    EDGE_ZIP_ERROR_UNSUPPORTED = -10,
    EDGE_ZIP_ERROR_CALLBACK = -11
} edge_zip_error_t;

/* Compression methods (stored in ZIP format) */
typedef enum {
    EDGE_ZIP_COMPRESSION_STORE = 0,         /* No compression */
    EDGE_ZIP_COMPRESSION_DEFLATE = 8,       /* DEFLATE compression */
    EDGE_ZIP_COMPRESSION_DEFLATE64 = 9,     /* DEFLATE64 compression */
    EDGE_ZIP_COMPRESSION_BZIP2 = 12,        /* BZIP2 compression */
    EDGE_ZIP_COMPRESSION_LZMA = 14,         /* LZMA compression */
    EDGE_ZIP_COMPRESSION_LZ77 = 19,         /* LZ77 compression */
    EDGE_ZIP_COMPRESSION_LZMA2 = 33,        /* LZMA2 compression */
    EDGE_ZIP_COMPRESSION_ZSTD = 93,         /* ZSTD compression */
    EDGE_ZIP_COMPRESSION_CUSTOM = 99
} edge_zip_compression_method_t;

/* Encryption methods */
typedef enum {
    EDGE_ZIP_ENCRYPTION_NONE = 0,
    EDGE_ZIP_ENCRYPTION_ZIPCRYPTO = 1,  /* Traditional PKWARE encryption */
    EDGE_ZIP_ENCRYPTION_AES128 = 2,
    EDGE_ZIP_ENCRYPTION_AES192 = 3,
    EDGE_ZIP_ENCRYPTION_AES256 = 4
} edge_zip_encryption_method_t;

/* Forward declarations */
typedef struct edge_zip_archive edge_zip_archive_t;
typedef struct edge_zip_entry edge_zip_entry_t;

/**
 * @brief Compression callback
 * @param output Buffer to write compressed data
 * @param output_size Size of output buffer, updated with actual bytes written
 * @param input Buffer containing uncompressed data
 * @param input_size Size of input data
 * @param user_data User-provided context pointer
 * @return EDGE_ZIP_OK on success, error code on failure
 */
typedef int (*edge_zip_compress_fn)(void* output, size_t* output_size, const void* input, size_t input_size, edge_zip_compression_method_t method, void* user_data);

/**
 * @brief Decompression callback
 * @param output Buffer to write decompressed data
 * @param output_size Expected size of decompressed data, verified on completion
 * @param input Buffer containing compressed data
 * @param input_size Size of compressed data
 * @param user_data User-provided context pointer
 * @return EDGE_ZIP_OK on success, error code on failure
 */
typedef int (*edge_zip_decompress_fn)(void* output, size_t output_size, const void* input, size_t input_size, edge_zip_compression_method_t method, void* user_data);

/**
 * @brief Encryption callback
 * @param output Buffer to write encrypted data
 * @param output_size Size of output buffer, updated with actual bytes written
 * @param input Buffer containing plaintext data
 * @param input_size Size of input data
 * @param user_data User-provided context pointer
 * @return EDGE_ZIP_OK on success, error code on failure
 */
typedef int (*edge_zip_encrypt_fn)(void* output, size_t* output_size, const void* input, size_t input_size, edge_zip_encryption_method_t method, void* user_data);

/**
 * @brief Decryption callback
 * @param output Buffer to write decrypted data
 * @param output_size Size of output buffer, updated with actual bytes written
 * @param input Buffer containing encrypted data
 * @param input_size Size of encrypted data
 * @param user_data User-provided context pointer
 * @return EDGE_ZIP_OK on success, error code on failure
 */
typedef int (*edge_zip_decrypt_fn)(void* output, size_t* output_size, const void* input, size_t input_size, edge_zip_encryption_method_t method, void* user_data);

/**
 * @brief Compression/decompression callbacks
 */
typedef struct {
    edge_zip_compress_fn compress_fn;     /* Optional */
    edge_zip_decompress_fn decompress_fn; /* Optional */
    void* user_data;                    /* Passed to compression callbacks */
} edge_zip_compressor_t;

/**
 * @brief Encryption/decryption callbacks
 */
typedef struct {
    edge_zip_encrypt_fn encrypt_fn;   /* Optional */
    edge_zip_decrypt_fn decrypt_fn;   /* Optional */
    void* user_data;                /* Passed to encryption callbacks */
} edge_zip_encryptor_t;

/**
 * @brief File entry metadata
 */
typedef struct {
    char* filename;                            /* Entry name/path */
    size_t filename_length;                    /* Length of filename */
    uint32_t uncompressed_size;                /* Original file size */
    uint32_t compressed_size;                  /* Compressed file size */
    uint32_t crc32;                            /* CRC-32 checksum */
    edge_zip_compression_method_t compression;   /* Compression method */
    edge_zip_encryption_method_t encryption;     /* Encryption method */
    time_t modified_time;                      /* Last modification time */
    bool is_directory;                         /* True if directory entry */
    uint16_t version_made_by;                  /* ZIP version */
    uint16_t version_needed;                   /* Minimum ZIP version needed */
    uint16_t flags;                            /* General purpose flags */
} edge_zip_entry_info_t;

/**
 * @brief Open an existing ZIP archive for reading
 * @param filename Path to ZIP file
 * @param allocator Memory allocator (NULL for default)
 * @param archive Output pointer to archive handle
 * @return EDGE_ZIP_OK on success, error code on failure
 */
int edge_zip_open(const char* filename, const edge_allocator_t* allocator, edge_zip_archive_t** archive);

/**
 * @brief Create a new ZIP archive for writing
 * @param filename Path to ZIP file to create
 * @param allocator Memory allocator (NULL for default)
 * @param archive Output pointer to archive handle
 * @return EDGE_ZIP_OK on success, error code on failure
 */
int edge_zip_create(const char* filename, const edge_allocator_t* allocator, edge_zip_archive_t** archive);

/**
 * @brief Close and finalize a ZIP archive
 * @param archive Archive handle
 * @return EDGE_ZIP_OK on success, error code on failure
 */
int edge_zip_close(edge_zip_archive_t* archive);

/**
 * @brief Get the number of entries in the archive
 * @param archive Archive handle
 * @return Number of entries, or -1 on error
 */
int edge_zip_get_num_entries(const edge_zip_archive_t* archive);

/**
 * @brief Set compression callbacks for the archive
 * @param archive Archive handle
 * @param compressor Compression callbacks
 * @return EDGE_ZIP_OK on success, error code on failure
 */
int edge_zip_set_compressor(edge_zip_archive_t* archive, const edge_zip_compressor_t* compressor);

/**
 * @brief Set encryption callbacks for the archive
 * @param archive Archive handle
 * @param encryptor Encryption callbacks
 * @return EDGE_ZIP_OK on success, error code on failure
 */
int edge_zip_set_encryptor(edge_zip_archive_t* archive, const edge_zip_encryptor_t* encryptor);

/**
 * @brief Get entry by index
 * @param archive Archive handle
 * @param index Entry index (0-based)
 * @param entry Output pointer to entry handle
 * @return EDGE_ZIP_OK on success, error code on failure
 */
int edge_zip_get_entry_by_index(edge_zip_archive_t* archive, int index, edge_zip_entry_t** entry);

/**
 * @brief Find entry by name
 * @param archive Archive handle
 * @param name Entry name/path
 * @param entry Output pointer to entry handle
 * @return EDGE_ZIP_OK on success, error code on failure
 */
int edge_zip_find_entry(edge_zip_archive_t* archive, const char* name, edge_zip_entry_t** entry);

/**
 * @brief Get entry metadata
 * @param entry Entry handle
 * @param info Output pointer to metadata structure
 * @return EDGE_ZIP_OK on success, error code on failure
 */
int edge_zip_entry_get_info(const edge_zip_entry_t* entry, edge_zip_entry_info_t* info);

/**
 * @brief Read entry data into buffer
 * @param entry Entry handle
 * @param buffer Output buffer
 * @param buffer_size Size of output buffer
 * @param bytes_read Output pointer for actual bytes read
 * @return EDGE_ZIP_OK on success, error code on failure
 */
int edge_zip_entry_read(edge_zip_entry_t* entry, void* buffer, size_t buffer_size, size_t* bytes_read);

/**
 * @brief Extract entry to file
 * @param entry Entry handle
 * @param output_path Path to output file
 * @return EDGE_ZIP_OK on success, error code on failure
 */
int edge_zip_entry_extract(edge_zip_entry_t* entry, const char* output_path);

/**
 * @brief Add a file to the archive
 * @param archive Archive handle
 * @param entry_name Name/path within archive
 * @param data File data
 * @param data_size Size of file data
 * @param compression Compression method
 * @param encryption Encryption method
 * @return EDGE_ZIP_OK on success, error code on failure
 */
int edge_zip_add_entry(edge_zip_archive_t* archive, const char* entry_name, const void* data,
    size_t data_size, edge_zip_compression_method_t compression, edge_zip_encryption_method_t encryption);

/**
 * @brief Add a file from disk to the archive
 * @param archive Archive handle
 * @param entry_name Name/path within archive
 * @param file_path Path to file on disk
 * @param compression Compression method
 * @param encryption Encryption method
 * @return EDGE_ZIP_OK on success, error code on failure
 */
int edge_zip_add_file(edge_zip_archive_t* archive, const char* entry_name, const char* file_path, edge_zip_compression_method_t compression, 
    edge_zip_encryption_method_t encryption);

/**
 * @brief Add a directory entry
 * @param archive Archive handle
 * @param directory_name Directory name/path (should end with /)
 * @return EDGE_ZIP_OK on success, error code on failure
 */
int edge_zip_add_directory(edge_zip_archive_t* archive, const char* directory_name);

/**
 * @brief Get error message for error code
 * @param error Error code
 * @return Human-readable error message
 */
const char* edge_zip_error_string(edge_zip_error_t error);

/**
 * @brief Calculate CRC-32 checksum
 * @param data Input data
 * @param size Size of data
 * @return CRC-32 checksum
 */
uint32_t edge_zip_crc32(const void* data, size_t size);

/**
 * @brief Get library version string
 * @return Version string (e.g., "1.0.0")
 */
const char* edge_zip_version(void);

#ifdef __cplusplus
}
#endif

#endif /* EDGE_ZIP_H */
