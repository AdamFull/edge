#include "edge_logger_internal.h"

#include <stdio.h>
#include <string.h>
#include <edge_allocator.h>

typedef struct edge_logger_output_file {
    edge_logger_output_t base;
    FILE* file;
    bool auto_flush;
} edge_logger_output_file_t;

static void file_write(edge_logger_output_t* output, const edge_log_entry_t* entry) {
    if (!output || !entry) {
        return;
    }

    edge_logger_output_file_t* file_output = (edge_logger_output_file_t*)output;
    if (!file_output->file) {
        return;
    }

    char buffer[EDGE_LOGGER_BUFFER_SIZE];
    /* Strip color codes for file output */
    int format_flags = output->format_flags & ~EDGE_LOG_FORMAT_COLOR;
    edge_logger_format_entry(buffer, sizeof(buffer), entry, format_flags);

    fprintf(file_output->file, "%s\n", buffer);

    if (file_output->auto_flush) {
        fflush(file_output->file);
    }
}

static void file_flush(edge_logger_output_t* output) {
    if (!output) {
        return;
    }

    edge_logger_output_file_t* file_output = (edge_logger_output_file_t*)output;
    if (file_output->file) {
        fflush(file_output->file);
    }
}

static void file_destroy(edge_logger_output_t* output) {
    if (!output) {
        return;
    }

    edge_logger_output_file_t* file_output = (edge_logger_output_file_t*)output;
    if (file_output->file) {
        fclose(file_output->file);
    }

    edge_allocator_free(output->allocator, file_output);
}

static const edge_logger_output_vtable_t file_vtable = {
    .write = file_write,
    .flush = file_flush,
    .destroy = file_destroy
};

edge_logger_output_t* edge_logger_create_file_output(const edge_allocator_t* allocator, int format_flags, const char* file_path, bool auto_flush) {
    if (!allocator || !file_path) {
        return NULL;
    }

    edge_logger_output_file_t* output = (edge_logger_output_file_t*)edge_allocator_malloc(allocator, sizeof(edge_logger_output_file_t));
    if (!output) {
        return NULL;
    }

    output->file = fopen(file_path, "a");
    if (!output->file) {
        edge_allocator_free(allocator, output);
        return NULL;
    }

    output->base.vtable = &file_vtable;
    output->base.format_flags = format_flags;
    output->base.allocator = allocator;
    output->base.user_data = NULL;
    output->auto_flush = auto_flush;

    return (edge_logger_output_t*)output;
}
