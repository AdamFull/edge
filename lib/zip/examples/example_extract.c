/**
 * @file example_extract.c
 * @brief Example of extracting files from a ZIP archive
 */

#include <edge_zip.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#endif

static int create_directory(const char* path) {
    #ifdef _WIN32
    return mkdir(path);
    #else
    return mkdir(path, 0755);
    #endif
}

static void extract_all_entries(const char* zip_path, const char* output_dir) {
    printf("Extracting archive: %s\n", zip_path);
    printf("Output directory: %s\n\n", output_dir);
    
    // Create output directory
    create_directory(output_dir);
    
    // Open archive
    edge_zip_archive_t* archive;
    int result = edge_zip_open(zip_path, NULL, &archive);
    if (result != EDGE_ZIP_OK) {
        fprintf(stderr, "Failed to open archive: %s\n", edge_zip_error_string(result));
        return;
    }
    
    int num_entries = edge_zip_get_num_entries(archive);
    printf("Archive contains %d entries\n\n", num_entries);
    
    int extracted = 0;
    int failed = 0;
    
    // Extract each entry
    for (int i = 0; i < num_entries; i++) {
        edge_zip_entry_t* entry;
        result = edge_zip_get_entry_by_index(archive, i, &entry);
        if (result != EDGE_ZIP_OK) {
            failed++;
            continue;
        }
        
        edge_zip_entry_info_t info;
        edge_zip_entry_get_info(entry, &info);
        
        // Build output path
        char output_path[1024];
        snprintf(output_path, sizeof(output_path), "%s/%s", output_dir, info.filename);
        
        if (info.is_directory) {
            printf("Creating directory: %s\n", output_path);
            create_directory(output_path);
            extracted++;
        } else {
            printf("Extracting: %s (%u bytes) ... ", info.filename, info.uncompressed_size);
            fflush(stdout);
            
            result = edge_zip_entry_extract(entry, output_path);
            if (result == EDGE_ZIP_OK) {
                printf("OK\n");
                extracted++;
            } else {
                printf("FAILED (%s)\n", edge_zip_error_string(result));
                failed++;
            }
        }
    }
    
    printf("\n");
    printf("Extraction complete:\n");
    printf("  Success: %d\n", extracted);
    printf("  Failed: %d\n", failed);
    
    edge_zip_close(archive);
}

static void list_archive_contents(const char* zip_path) {
    printf("Listing archive: %s\n\n", zip_path);
    
    edge_zip_archive_t* archive;
    int result = edge_zip_open(zip_path, NULL, &archive);
    if (result != EDGE_ZIP_OK) {
        fprintf(stderr, "Failed to open archive: %s\n", edge_zip_error_string(result));
        return;
    }
    
    int num_entries = edge_zip_get_num_entries(archive);
    
    printf("%-50s %12s %12s %s\n", "Name", "Compressed", "Uncompressed", "Method");
    printf("%s\n", "--------------------------------------------------------------------------------");
    
    uint64_t total_compressed = 0;
    uint64_t total_uncompressed = 0;
    
    for (int i = 0; i < num_entries; i++) {
        edge_zip_entry_t* entry;
        edge_zip_get_entry_by_index(archive, i, &entry);
        
        edge_zip_entry_info_t info;
        edge_zip_entry_get_info(entry, &info);
        
        const char* method = "Store";
        if (info.compression == EDGE_ZIP_COMPRESSION_DEFLATE) {
            method = "Deflate";
        } else if (info.compression == EDGE_ZIP_COMPRESSION_CUSTOM) {
            method = "Custom";
        }
        
        printf("%-50s %12u %12u %s%s\n",
               info.filename,
               info.compressed_size,
               info.uncompressed_size,
               method,
               info.is_directory ? " (dir)" : "");
        
        total_compressed += info.compressed_size;
        total_uncompressed += info.uncompressed_size;
    }
    
    printf("%s\n", "--------------------------------------------------------------------------------");
    printf("%-50s %12lu %12lu\n", "Total", 
           (unsigned long)total_compressed,
           (unsigned long)total_uncompressed);
    
    if (total_uncompressed > 0) {
        double ratio = (1.0 - (double)total_compressed / total_uncompressed) * 100.0;
        printf("Compression ratio: %.1f%%\n", ratio);
    }
    
    edge_zip_close(archive);
}

int main(int argc, char* argv[]) {
    printf("ZipLib Extract Example\n\n");
    
    if (argc < 2) {
        printf("Usage:\n");
        printf("  %s <zipfile>           - List contents\n", argv[0]);
        printf("  %s <zipfile> <outdir>  - Extract all files\n", argv[0]);
        return 1;
    }
    
    const char* zip_path = argv[1];
    
    if (argc >= 3) {
        // Extract mode
        const char* output_dir = argv[2];
        extract_all_entries(zip_path, output_dir);
    } else {
        // List mode
        list_archive_contents(zip_path);
    }
    
    return 0;
}
