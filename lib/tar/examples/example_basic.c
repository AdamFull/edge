/**
 * @file example_basic.c
 * @brief Basic example of reading and writing TAR archives
 */

#include <edge_tar.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void example_create_archive(void) {
    printf("=== Creating Archive ===\n");

    edge_tar_archive_t* archive;
    int result = edge_tar_create("example.tar", EDGE_TAR_FORMAT_USTAR, NULL, &archive);
    if (result != EDGE_TAR_OK) {
        fprintf(stderr, "Failed to create archive: %s\n", edge_tar_error_string(result));
        return;
    }

    // Add some text files
    const char* text1 = "Hello, World!\nThis is a test file.\n";
    result = edge_tar_add_entry(archive, "hello.txt", text1, strlen(text1), 0644);
    if (result != EDGE_TAR_OK) {
        fprintf(stderr, "Failed to add entry: %s\n", edge_tar_error_string(result));
    }
    else {
        printf("Added: hello.txt\n");
    }

    const char* text2 = "This is another file\nWith multiple lines\n";
    result = edge_tar_add_entry(archive, "readme.txt", text2, strlen(text2), 0644);
    if (result != EDGE_TAR_OK) {
        fprintf(stderr, "Failed to add entry: %s\n", edge_tar_error_string(result));
    }
    else {
        printf("Added: readme.txt\n");
    }

    // Add a directory
    result = edge_tar_add_directory(archive, "docs/", 0755);
    if (result != EDGE_TAR_OK) {
        fprintf(stderr, "Failed to add directory: %s\n", edge_tar_error_string(result));
    }
    else {
        printf("Added directory: docs/\n");
    }

    // Add file in directory
    const char* text3 = "Documentation content goes here.\n";
    result = edge_tar_add_entry(archive, "docs/manual.txt", text3, strlen(text3), 0644);
    if (result != EDGE_TAR_OK) {
        fprintf(stderr, "Failed to add entry: %s\n", edge_tar_error_string(result));
    }
    else {
        printf("Added: docs/manual.txt\n");
    }

    // Add a symbolic link
    result = edge_tar_add_symlink(archive, "link_to_readme", "readme.txt");
    if (result != EDGE_TAR_OK) {
        fprintf(stderr, "Failed to add symlink: %s\n", edge_tar_error_string(result));
    }
    else {
        printf("Added symlink: link_to_readme -> readme.txt\n");
    }

    // Close archive (writes end marker)
    result = edge_tar_close(archive);
    if (result != EDGE_TAR_OK) {
        fprintf(stderr, "Failed to close archive: %s\n", edge_tar_error_string(result));
    }
    else {
        printf("Archive created successfully!\n");
    }

    printf("\n");
}

static const char* entry_type_to_string(edge_tar_entry_type_t type) {
    switch (type) {
    case EDGE_TAR_TYPE_REGULAR:
    case EDGE_TAR_TYPE_REGULAR_ALT:
        return "Regular File";
    case EDGE_TAR_TYPE_DIRECTORY:
        return "Directory";
    case EDGE_TAR_TYPE_SYMLINK:
        return "Symbolic Link";
    case EDGE_TAR_TYPE_LINK:
        return "Hard Link";
    case EDGE_TAR_TYPE_CHAR:
        return "Character Device";
    case EDGE_TAR_TYPE_BLOCK:
        return "Block Device";
    case EDGE_TAR_TYPE_FIFO:
        return "FIFO";
    default:
        return "Unknown";
    }
}

static void example_read_archive(void) {
    printf("=== Reading Archive ===\n");

    edge_tar_archive_t* archive;
    int result = edge_tar_open("example.tar", NULL, &archive);
    if (result != EDGE_TAR_OK) {
        fprintf(stderr, "Failed to open archive: %s\n", edge_tar_error_string(result));
        return;
    }

    int num_entries = edge_tar_get_num_entries(archive);
    printf("Archive contains %d entries:\n\n", num_entries);

    // List all entries
    for (int i = 0; i < num_entries; i++) {
        edge_tar_entry_t* entry;
        result = edge_tar_get_entry_by_index(archive, i, &entry);
        if (result != EDGE_TAR_OK) {
            fprintf(stderr, "Failed to get entry %d: %s\n", i, edge_tar_error_string(result));
            continue;
        }

        edge_tar_entry_info_t info;
        result = edge_tar_entry_get_info(entry, &info);
        if (result != EDGE_TAR_OK) {
            fprintf(stderr, "Failed to get entry info: %s\n", edge_tar_error_string(result));
            continue;
        }

        printf("Entry %d:\n", i);
        printf("  Name: %s\n", info.filename);
        printf("  Type: %s\n", entry_type_to_string(info.type));
        printf("  Size: %llu bytes\n", (unsigned long long)info.size);
        printf("  Mode: %04o\n", info.mode);
        printf("  UID/GID: %u/%u\n", info.uid, info.gid);
        printf("  User/Group: %s/%s\n", info.uname, info.gname);

        if (info.linkname) {
            printf("  Link target: %s\n", info.linkname);
        }

        printf("\n");
    }

    // Read specific file
    printf("=== Reading hello.txt ===\n");
    edge_tar_entry_t* entry;
    result = edge_tar_find_entry(archive, "hello.txt", &entry);
    if (result == EDGE_TAR_OK) {
        edge_tar_entry_info_t info;
        edge_tar_entry_get_info(entry, &info);

        char* buffer = malloc(info.size + 1);
        if (buffer) {
            size_t bytes_read;
            result = edge_tar_entry_read(entry, buffer, info.size, &bytes_read);
            if (result == EDGE_TAR_OK) {
                buffer[bytes_read] = '\0';
                printf("Content:\n%s\n", buffer);
            }
            else {
                fprintf(stderr, "Failed to read entry: %s\n", edge_tar_error_string(result));
            }
            free(buffer);
        }
    }
    else {
        fprintf(stderr, "Entry not found: %s\n", edge_tar_error_string(result));
    }

    edge_tar_close(archive);
}

int main(void) {
    printf("TarLib Basic Example\n");
    printf("Version: %s\n\n", edge_tar_version());

    // Create an archive
    example_create_archive();

    // Read the archive
    example_read_archive();

    return 0;
}