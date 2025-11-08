# libedgezip - Lightweight ZIP Archive Library for C11

A lightweight, callback-based ZIP file library written in C11 with **no third-party dependencies**. Features customizable callbacks for memory allocation, compression, decompression, encryption, and decryption.

## Quick Start

### Building with CMake

```bash
mkdir build && cd build
cmake ..
make
```

### Basic Usage - Reading a ZIP Archive

```c
#include <edge_zip_.h>
#include <stdio.h>

int main(void) {
    edge_zip_archive_t* archive;
    
    // Open archive
    int result = edge_zip_open("archive.zip", NULL, &archive);
    if (result != EDGE_ZIP_OK) {
        fprintf(stderr, "Failed to open archive: %s\n", edge_zip_error_string(result));
        return 1;
    }
    
    // Get number of entries
    int num_entries = edge_zip_get_num_entries(archive);
    printf("Archive contains %d entries\n", num_entries);
    
    // Iterate through entries
    for (int i = 0; i < num_entries; i++) {
        edge_zip_entry_t* entry;
        edge_zip_get_entry_by_index(archive, i, &entry);
        
        edge_zip_entry_info_t info;
        edge_zip_entry_get_info(entry, &info);
        
        printf("Entry %d: %s (%u bytes)\n", i, info.filename, info.uncompressed_size);
    }
    
    // Close archive
    edge_zip_close(archive);
    return 0;
}
```

### Basic Usage - Creating a ZIP Archive

```c
#include <edge_zip_.h>

int main(void) {
    edge_zip_archive_t* archive;
    
    // Create new archive
    int result = edge_zip_create("new_archive.zip", NULL, &archive);
    if (result != EDGE_ZIP_OK) {
        return 1;
    }
    
    // Add text file
    const char* text = "Hello, World!";
    edge_zip_add_entry(archive, "hello.txt", text, strlen(text),
                     EDGE_ZIP_COMPRESSION_STORE, EDGE_ZIP_ENCRYPTION_NONE);
    
    // Add file from disk
    edge_zip_add_file(archive, "document.txt", "/path/to/document.txt",
                    EDGE_ZIP_COMPRESSION_STORE, EDGE_ZIP_ENCRYPTION_NONE);
    
    // Add directory
    edge_zip_add_directory(archive, "mydir/");
    
    // Close and finalize archive
    edge_zip_close(archive);
    return 0;
}
```

## Advanced Features

### Custom Memory Allocator

```c
void* my_malloc(size_t size, void* user_data) {
    printf("Allocating %zu bytes\n", size);
    return malloc(size);
}

void my_free(void* ptr, void* user_data) {
    printf("Freeing memory\n");
    free(ptr);
}

edge_allocator_t allocator = edge_allocator_create(my_malloc, my_free, NULL, NULL, NULL);

edge_zip_archive_t* archive;
edge_zip_open("archive.zip", &allocator, &archive);
```

### Custom Compression (e.g., zlib DEFLATE)

```c
#include <zlib.h>

int my_compress(void* output, size_t* output_size,
                const void* input, size_t input_size,
                edge_zip_compression_method_t method,
                void* user_data) {
    uLongf dest_len = *output_size;
    int ret = compress2(output, &dest_len, input, input_size, Z_DEFAULT_COMPRESSION);
    *output_size = dest_len;
    return (ret == Z_OK) ? EDGE_ZIP_OK : EDGE_ZIP_ERROR_COMPRESSION;
}

int my_decompress(void* output, size_t output_size,
                  const void* input, size_t input_size,
                  edge_zip_compression_method_t method,
                  void* user_data) {
    uLongf dest_len = output_size;
    int ret = uncompress(output, &dest_len, input, input_size);
    return (ret == Z_OK) ? EDGE_ZIP_OK : EDGE_ZIP_ERROR_DECOMPRESSION;
}

edge_zip_compressor_t compressor = {
    .compress_fn = my_compress,
    .decompress_fn = my_decompress,
    .user_data = NULL
};

edge_zip_archive_t* archive;
edge_zip_create("compressed.zip", NULL, &archive);
edge_zip_set_compressor(archive, &compressor);

// Now add entries with DEFLATE compression
edge_zip_add_entry(archive, "file.txt", data, size,
                 EDGE_ZIP_COMPRESSION_DEFLATE, EDGE_ZIP_ENCRYPTION_NONE);
```

### Custom Encryption

```c
typedef struct {
    uint8_t key[32];
    size_t key_len;
} encryption_context_t;

int my_encrypt(void* output, size_t* output_size,
               const void* input, size_t input_size,
               edge_zip_encryption_method_t method,
               void* user_data) {
    encryption_context_t* ctx = (encryption_context_t*)user_data;
    
    // Simple XOR encryption (use proper encryption in production!)
    for (size_t i = 0; i < input_size; i++) {
        ((uint8_t*)output)[i] = ((uint8_t*)input)[i] ^ ctx->key[i % ctx->key_len];
    }
    *output_size = input_size;
    return EDGE_ZIP_OK;
}

int my_decrypt(void* output, size_t* output_size,
               const void* input, size_t input_size,
               edge_zip_encryption_method_t method,
               void* user_data) {
    // XOR decryption is same as encryption
    return my_encrypt(output, output_size, input, input_size, user_data);
}

encryption_context_t ctx = {
    .key = {0x01, 0x02, 0x03, /* ... */},
    .key_len = 32
};

edge_zip_encryptor_t encryptor = {
    .encrypt_fn = my_encrypt,
    .decrypt_fn = my_decrypt,
    .user_data = &ctx
};

edge_zip_archive_t* archive;
edge_zip_create("encrypted.zip", NULL, &archive);
edge_zip_set_encryptor(archive, &encryptor);

edge_zip_add_entry(archive, "secret.txt", data, size,
                 EDGE_ZIP_COMPRESSION_STORE, EDGE_ZIP_ENCRYPTION_CUSTOM);
```

### Extracting Files

```c
edge_zip_archive_t* archive;
edge_zip_open("archive.zip", NULL, &archive);

// Find specific entry
edge_zip_entry_t* entry;
if (edge_zip_find_entry(archive, "document.txt", &entry) == EDGE_ZIP_OK) {
    // Extract to file
    edge_zip_entry_extract(entry, "output.txt");
    
    // Or read into memory
    edge_zip_entry_info_t info;
    edge_zip_entry_get_info(entry, &info);
    
    void* buffer = malloc(info.uncompressed_size);
    size_t bytes_read;
    edge_zip_entry_read(entry, buffer, info.uncompressed_size, &bytes_read);
    
    // Use buffer...
    free(buffer);
}

edge_zip_close(archive);
```

## API Reference

### Archive Operations

- `edge_zip_open()` - Open existing ZIP archive for reading
- `edge_zip_create()` - Create new ZIP archive for writing
- `edge_zip_close()` - Close and finalize archive
- `edge_zip_get_num_entries()` - Get number of entries
- `edge_zip_set_compressor()` - Set compression callbacks
- `edge_zip_set_encryptor()` - Set encryption callbacks

### Entry Operations (Reading)

- `edge_zip_get_entry_by_index()` - Get entry by index
- `edge_zip_find_entry()` - Find entry by name
- `edge_zip_entry_get_info()` - Get entry metadata
- `edge_zip_entry_read()` - Read entry data into buffer
- `edge_zip_entry_extract()` - Extract entry to file

### Entry Operations (Writing)

- `edge_zip_add_entry()` - Add data from memory
- `edge_zip_add_file()` - Add file from disk
- `edge_zip_add_directory()` - Add directory entry

### Utility Functions

- `edge_zip_error_string()` - Get error message
- `edge_zip_crc32()` - Calculate CRC-32 checksum
- `edge_zip_version()` - Get library version
- `edge_zip_default_allocator()` - Get default memory allocator

## Compression Methods

- `EDGE_ZIP_COMPRESSION_STORE` - No compression (stored)
- `EDGE_ZIP_COMPRESSION_DEFLATE` - DEFLATE compression (requires callback)
- `EDGE_ZIP_COMPRESSION_DEFLATE64` - DEFLATE64 compression (requires callback)
- `EDGE_ZIP_COMPRESSION_BZIP2` - BZIP2 compression (requires callback)
- `EDGE_ZIP_COMPRESSION_LZMA` - LZMA compression (requires callback)
- `EDGE_ZIP_COMPRESSION_LZ77` - LZ77 compression (requires callback)
- `EDGE_ZIP_COMPRESSION_LZMA2` - LZMA2 compression (requires callback)
- `EDGE_ZIP_COMPRESSION_ZSTD` - ZSTD compression (requires callback)
- `EDGE_ZIP_COMPRESSION_CUSTOM` - Custom compression (requires callback)

## Encryption Methods

- `EDGE_ZIP_ENCRYPTION_NONE` - No encryption
- `EDGE_ZIP_ENCRYPTION_ZIPCRYPTO` - Traditional PKWARE encryption (requires callback)
- `EDGE_ZIP_ENCRYPTION_AES128/192/256` - AES encryption (requires callback)
- `EDGE_ZIP_ENCRYPTION_CUSTOM` - Custom encryption (requires callback)

## Error Codes

- `EDGE_ZIP_OK` - Success
- `EDGE_ZIP_ERROR_INVALID_ARGUMENT` - Invalid function argument
- `EDGE_ZIP_ERROR_OUT_OF_MEMORY` - Memory allocation failed
- `EDGE_ZIP_ERROR_IO` - File I/O error
- `EDGE_ZIP_ERROR_CORRUPT_ARCHIVE` - Archive is corrupted
- `EDGE_ZIP_ERROR_NOT_FOUND` - Entry not found
- `EDGE_ZIP_ERROR_COMPRESSION` - Compression failed
- `EDGE_ZIP_ERROR_DECOMPRESSION` - Decompression failed
- `EDGE_ZIP_ERROR_ENCRYPTION` - Encryption failed
- `EDGE_ZIP_ERROR_DECRYPTION` - Decryption failed
- `EDGE_ZIP_ERROR_UNSUPPORTED` - Unsupported feature
- `EDGE_ZIP_ERROR_CALLBACK` - Callback error

## Design Philosophy

edge_zip is designed with **flexibility** and **simplicity** in mind:

1. **No dependencies** - The library has zero external dependencies, making it easy to integrate
2. **Callback-based** - All expensive operations (compression, encryption, memory allocation) are delegated to user-provided callbacks
3. **Standard compliance** - Creates standard ZIP files compatible with all ZIP tools
4. **Memory control** - Users have full control over memory allocation
5. **Simple API** - Clean, easy-to-understand API with consistent error handling

## Limitations

- Multi-disk archives are not supported
- ZIP64 format (for files > 4GB) is not implemented
- Built-in compression/encryption requires user-provided callbacks
- Directory creation on extraction is not fully implemented

## Examples

See the `examples/` directory for complete working examples:

- `example_basic.c` - Basic reading and writing
- `example_extract.c` - Extracting all files from archive

## Testing

Build and run tests:

```bash
mkdir build && cd build
cmake ..
make
ctest
```

## License

MIT License - See LICENSE file for details.

## Version History

### 1.0.0 (2025-11-06)
- Initial release
- Basic ZIP reading and writing
- Callback-based compression, encryption, and memory allocation
- CRC-32 verification
- Standard ZIP format support
