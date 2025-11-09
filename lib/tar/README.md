# Edge TAR Library

A C11 library for working with TAR (Tape Archive) files, featuring custom memory allocator support.

## Building

### Build Instructions

```bash
mkdir build
cd build
cmake ..
make
make install
```

## Usage

### Creating a TAR Archive

```c
#include <edge_tar.h>

// Create a new TAR archive
edge_tar_archive_t* archive;
edge_tar_create("output.tar", EDGE_TAR_FORMAT_USTAR, NULL, &archive);

// Add a file from memory
const char* data = "Hello, World!";
edge_tar_add_entry(archive, "hello.txt", data, strlen(data), 0644);

// Add a file from disk
edge_tar_add_file(archive, "readme.txt", "/path/to/readme.txt");

// Add a directory
edge_tar_add_directory(archive, "docs/", 0755);

// Add a symbolic link
edge_tar_add_symlink(archive, "link", "target.txt");

// Close archive (writes end marker)
edge_tar_close(archive);
```

### Reading a TAR Archive

```c
#include <edge_tar.h>

// Open existing archive
edge_tar_archive_t* archive;
edge_tar_open("input.tar", NULL, &archive);

// Get number of entries
int num_entries = edge_tar_get_num_entries(archive);

// Iterate through entries
for (int i = 0; i < num_entries; i++) {
    edge_tar_entry_t* entry;
    edge_tar_get_entry_by_index(archive, i, &entry);
    
    edge_tar_entry_info_t info;
    edge_tar_entry_get_info(entry, &info);
    
    printf("Entry: %s (%llu bytes)\n", info.filename, info.size);
}

// Find specific entry
edge_tar_entry_t* entry;
if (edge_tar_find_entry(archive, "hello.txt", &entry) == EDGE_TAR_OK) {
    // Read entry data
    char buffer[1024];
    size_t bytes_read;
    edge_tar_entry_read(entry, buffer, sizeof(buffer), &bytes_read);
    
    // Or extract to file
    edge_tar_entry_extract(entry, "output/hello.txt");
}

edge_tar_close(archive);
```

### Using Custom Allocators

```c
#include <edge_tar.h>
#include <edge_allocator.h>

// Create custom allocator
edge_allocator_t allocator = edge_allocator_create(
    my_malloc, my_free, my_realloc, my_calloc, my_strdup
);

// Use allocator with TAR library
edge_tar_archive_t* archive;
edge_tar_create("output.tar", EDGE_TAR_FORMAT_USTAR, &allocator, &archive);

// ... use archive ...

edge_tar_close(archive);
```

## API Reference

### Core Functions

- `edge_tar_open()` - Open existing TAR archive for reading
- `edge_tar_create()` - Create new TAR archive for writing
- `edge_tar_close()` - Close and finalize archive
- `edge_tar_get_num_entries()` - Get number of entries in archive

### Entry Access

- `edge_tar_get_entry_by_index()` - Get entry by index
- `edge_tar_find_entry()` - Find entry by name
- `edge_tar_entry_get_info()` - Get entry metadata
- `edge_tar_entry_read()` - Read entry data into buffer
- `edge_tar_entry_extract()` - Extract entry to file

### Writing Entries

- `edge_tar_add_entry()` - Add file from memory
- `edge_tar_add_file()` - Add file from disk
- `edge_tar_add_directory()` - Add directory entry
- `edge_tar_add_symlink()` - Add symbolic link

### Utilities

- `edge_tar_error_string()` - Get error message
- `edge_tar_version()` - Get library version
- `edge_tar_checksum()` - Calculate TAR header checksum

## TAR Format Support

### USTAR (POSIX.1-1988)

The default and most widely supported format. Supports:
- Filenames up to 256 characters (100 name + 155 prefix)
- File sizes up to 8GB
- User/group names up to 32 characters
- Portable across all platforms

### PAX (POSIX.1-2001)

Extended format with support for:
- Unlimited filename length
- Unlimited file size
- Extended attributes
- Unicode filenames

### GNU TAR

GNU-specific extensions with support for:
- Long filenames
- Sparse files
- Incremental backups

### V7

Original UNIX V7 tar format:
- Filenames up to 100 characters
- Limited metadata support
- Maximum compatibility

## Error Handling

All functions return an error code of type `edge_tar_error_t`:

```c
int result = edge_tar_open("archive.tar", NULL, &archive);
if (result != EDGE_TAR_OK) {
    fprintf(stderr, "Error: %s\n", edge_tar_error_string(result));
}
```

### Error Codes

- `EDGE_TAR_OK` - Success
- `EDGE_TAR_ERROR_INVALID_ARGUMENT` - Invalid argument
- `EDGE_TAR_ERROR_OUT_OF_MEMORY` - Out of memory
- `EDGE_TAR_ERROR_IO` - I/O error
- `EDGE_TAR_ERROR_CORRUPT_ARCHIVE` - Corrupt archive
- `EDGE_TAR_ERROR_NOT_FOUND` - Entry not found
- `EDGE_TAR_ERROR_UNSUPPORTED` - Unsupported feature

## Examples

See the `examples/` directory for complete examples:

- `example_basic.c` - Basic archive creation and reading
- `example_extract.c` - Extract files from TAR archive