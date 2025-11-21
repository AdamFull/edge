#include "edge_logger_internal.h"

#include <stdio.h>
#include <string.h>
#include <edge_allocator.h>

typedef struct edge_logger_output_stdout {
    edge_logger_output_t base;
} edge_logger_output_stdout_t;

static void stdout_write(edge_logger_output_t* output, const edge_log_entry_t* entry) {
    if (!output || !entry) {
        return;
    }

    char buffer[EDGE_LOGGER_BUFFER_SIZE];
    edge_logger_format_entry(buffer, sizeof(buffer), entry, output->format_flags);
    printf("%s\n", buffer);
}

static void stdout_flush(edge_logger_output_t* output) {
    (void)output;
    fflush(stdout);
}

static void stdout_destroy(edge_logger_output_t* output) {
    if (!output) {
        return;
    }

    edge_logger_output_stdout_t* stdout_output = (edge_logger_output_stdout_t*)output;
    edge_allocator_free(output->allocator, stdout_output);
}

static const edge_logger_output_vtable_t stdout_vtable = {
    .write = stdout_write,
    .flush = stdout_flush,
    .destroy = stdout_destroy
};

edge_logger_output_t* edge_logger_create_stdout_output(const edge_allocator_t* allocator, int format_flags) {
    if (!allocator) {
        return NULL;
    }

    edge_logger_output_stdout_t* output = (edge_logger_output_stdout_t*)edge_allocator_malloc(allocator, sizeof(edge_logger_output_stdout_t));

    if (!output) {
        return NULL;
    }

    output->base.vtable = &stdout_vtable;
    output->base.format_flags = format_flags;
    output->base.allocator = allocator;
    output->base.user_data = NULL;

    return (edge_logger_output_t*)output;
}
