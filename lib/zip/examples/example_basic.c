/**
 * @file example_basic.c
 * @brief Basic example of reading and writing ZIP archives
 */

#include <edge_zip.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void example_create_archive(void) {
    printf("=== Creating Archive ===\n");
    
    edge_zip_archive_t* archive;
    int result = edge_zip_create("example.zip", NULL, &archive);
    if (result != EDGE_ZIP_OK) {
        fprintf(stderr, "Failed to create archive: %s\n", edge_zip_error_string(result));
        return;
    }
    
    // Add some text files
    const char* text1 = "Hello, World!\nThis is a test file.\n";
    result = edge_zip_add_entry(archive, "hello.txt", text1, strlen(text1),
                              EDGE_ZIP_COMPRESSION_STORE, EDGE_ZIP_ENCRYPTION_NONE);
    if (result != EDGE_ZIP_OK) {
        fprintf(stderr, "Failed to add entry: %s\n", edge_zip_error_string(result));
    } else {
        printf("Added: hello.txt\n");
    }
    
    const char* text2 = "This is another file\nWith multiple lines\n";
    result = edge_zip_add_entry(archive, "readme.txt", text2, strlen(text2),
                              EDGE_ZIP_COMPRESSION_STORE, EDGE_ZIP_ENCRYPTION_NONE);
    if (result != EDGE_ZIP_OK) {
        fprintf(stderr, "Failed to add entry: %s\n", edge_zip_error_string(result));
    } else {
        printf("Added: readme.txt\n");
    }
    
    // Add a directory
    result = edge_zip_add_directory(archive, "docs/");
    if (result != EDGE_ZIP_OK) {
        fprintf(stderr, "Failed to add directory: %s\n", edge_zip_error_string(result));
    } else {
        printf("Added directory: docs/\n");
    }
    
    // Add file in directory
    const char* text3 = "Documentation content goes here.\n";
    result = edge_zip_add_entry(archive, "docs/manual.txt", text3, strlen(text3),
                              EDGE_ZIP_COMPRESSION_STORE, EDGE_ZIP_ENCRYPTION_NONE);
    if (result != EDGE_ZIP_OK) {
        fprintf(stderr, "Failed to add entry: %s\n", edge_zip_error_string(result));
    } else {
        printf("Added: docs/manual.txt\n");
    }
    
    // Close archive (writes central directory)
    result = edge_zip_close(archive);
    if (result != EDGE_ZIP_OK) {
        fprintf(stderr, "Failed to close archive: %s\n", edge_zip_error_string(result));
    } else {
        printf("Archive created successfully!\n");
    }
    
    printf("\n");
}

static void example_read_archive(void) {
    printf("=== Reading Archive ===\n");
    
    edge_zip_archive_t* archive;
    int result = edge_zip_open("example.zip", NULL, &archive);
    if (result != EDGE_ZIP_OK) {
        fprintf(stderr, "Failed to open archive: %s\n", edge_zip_error_string(result));
        return;
    }
    
    int num_entries = edge_zip_get_num_entries(archive);
    printf("Archive contains %d entries:\n\n", num_entries);
    
    // List all entries
    for (int i = 0; i < num_entries; i++) {
        edge_zip_entry_t* entry;
        result = edge_zip_get_entry_by_index(archive, i, &entry);
        if (result != EDGE_ZIP_OK) {
            fprintf(stderr, "Failed to get entry %d: %s\n", i, edge_zip_error_string(result));
            continue;
        }
        
        edge_zip_entry_info_t info;
        result = edge_zip_entry_get_info(entry, &info);
        if (result != EDGE_ZIP_OK) {
            fprintf(stderr, "Failed to get entry info: %s\n", edge_zip_error_string(result));
            continue;
        }
        
        printf("Entry %d:\n", i);
        printf("  Name: %s\n", info.filename);
        printf("  Type: %s\n", info.is_directory ? "Directory" : "File");
        printf("  Uncompressed size: %u bytes\n", info.uncompressed_size);
        printf("  Compressed size: %u bytes\n", info.compressed_size);
        printf("  Compression: %s\n", 
               info.compression == EDGE_ZIP_COMPRESSION_STORE ? "Store" : "Other");
        printf("  CRC-32: 0x%08X\n", info.crc32);
        printf("\n");
    }
    
    // Read specific file
    printf("=== Reading hello.txt ===\n");
    edge_zip_entry_t* entry;
    result = edge_zip_find_entry(archive, "hello.txt", &entry);
    if (result == EDGE_ZIP_OK) {
        edge_zip_entry_info_t info;
        edge_zip_entry_get_info(entry, &info);
        
        char* buffer = malloc(info.uncompressed_size + 1);
        if (buffer) {
            size_t bytes_read;
            result = edge_zip_entry_read(entry, buffer, info.uncompressed_size, &bytes_read);
            if (result == EDGE_ZIP_OK) {
                buffer[bytes_read] = '\0';
                printf("Content:\n%s\n", buffer);
            } else {
                fprintf(stderr, "Failed to read entry: %s\n", edge_zip_error_string(result));
            }
            free(buffer);
        }
    } else {
        fprintf(stderr, "Entry not found: %s\n", edge_zip_error_string(result));
    }
    
    edge_zip_close(archive);
}

int main(void) {
    printf("ZipLib Basic Example\n");
    printf("Version: %s\n\n", edge_zip_version());
    
    // Create an archive
    example_create_archive();
    
    // Read the archive
    example_read_archive();
    
    return 0;
}
