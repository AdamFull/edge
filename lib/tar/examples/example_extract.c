/**
 * @file example_extract.c
 * @brief Example of extracting files from a TAR archive
 */

#include <edge_tar.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#endif

static int ensure_directory_exists(const char* path) {
    char* path_copy = strdup(path);
    if (!path_copy) {
        return -1;
    }

    char* p = path_copy;

    // Skip drive letter on Windows
#ifdef _WIN32
    if (p[0] && p[1] == ':') {
        p += 2;
    }
#endif

    // Skip leading slash
    if (*p == '/' || *p == '\\') {
        p++;
    }

    // Create directories progressively
    while (*p) {
        while (*p && *p != '/' && *p != '\\') {
            p++;
        }

        if (*p) {
            char saved = *p;
            *p = '\0';

            struct stat st;
            if (stat(path_copy, &st) != 0) {
                if (mkdir(path_copy, 0755) != 0) {
                    free(path_copy);
                    return -1;
                }
            }

            *p = saved;
            p++;
        }
    }

    free(path_copy);
    return 0;
}

static int extract_archive(const char* archive_path, const char* output_dir) {
    edge_tar_archive_t* archive;
    int result = edge_tar_open(archive_path, NULL, &archive);
    if (result != EDGE_TAR_OK) {
        fprintf(stderr, "Failed to open archive: %s\n", edge_tar_error_string(result));
        return -1;
    }

    printf("Extracting archive: %s\n", archive_path);
    printf("Output directory: %s\n\n", output_dir);

    // Create output directory if it doesn't exist
    ensure_directory_exists(output_dir);

    int num_entries = edge_tar_get_num_entries(archive);
    int extracted_count = 0;

    for (int i = 0; i < num_entries; i++) {
        edge_tar_entry_t* entry;
        result = edge_tar_get_entry_by_index(archive, i, &entry);
        if (result != EDGE_TAR_OK) {
            fprintf(stderr, "Failed to get entry %d: %s\n", i, edge_tar_error_string(result));
            continue;
        }

        edge_tar_entry_info_t info;
        edge_tar_entry_get_info(entry, &info);

        // Build output path
        char output_path[512];
        snprintf(output_path, sizeof(output_path), "%s/%s", output_dir, info.filename);

        printf("Extracting: %s", info.filename);

        // Ensure parent directory exists
        char* last_slash = strrchr(output_path, '/');
        if (last_slash) {
            *last_slash = '\0';
            ensure_directory_exists(output_path);
            *last_slash = '/';
        }

        // Extract entry
        result = edge_tar_entry_extract(entry, output_path);
        if (result == EDGE_TAR_OK) {
            printf(" ... OK\n");
            extracted_count++;
        }
        else if (result == EDGE_TAR_ERROR_UNSUPPORTED) {
            printf(" ... SKIPPED (unsupported type)\n");
        }
        else {
            printf(" ... FAILED (%s)\n", edge_tar_error_string(result));
        }
    }

    printf("\nExtraction complete: %d/%d files extracted\n", extracted_count, num_entries);

    edge_tar_close(archive);
    return 0;
}

static void print_usage(const char* program_name) {
    printf("Usage: %s <archive.tar> [output_dir]\n", program_name);
    printf("\n");
    printf("Extract files from a TAR archive.\n");
    printf("\n");
    printf("Arguments:\n");
    printf("  archive.tar  Path to the TAR archive to extract\n");
    printf("  output_dir   Output directory (default: current directory)\n");
    printf("\n");
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char* archive_path = argv[1];
    const char* output_dir = (argc >= 3) ? argv[2] : ".";

    printf("TarLib Extract Example\n");
    printf("Version: %s\n\n", edge_tar_version());

    return extract_archive(archive_path, output_dir);
}