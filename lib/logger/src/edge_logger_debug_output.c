#include "edge_logger_internal.h"

#include <string.h>
#include <edge_allocator.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef struct edge_logger_output_debug_console {
    edge_logger_output_t base;
} edge_logger_output_debug_console_t;

static void debug_console_write(edge_logger_output_t* output, const edge_log_entry_t* entry) {
    if (!output || !entry) {
        return;
    }

    char buffer[EDGE_LOGGER_BUFFER_SIZE];
    // debug console does not have color support
    int format_flags = output->format_flags & ~EDGE_LOG_FORMAT_COLOR;
    edge_logger_format_entry(buffer, sizeof(buffer), entry, format_flags);

    OutputDebugStringA(buffer);
    OutputDebugStringA("\n");
}

static void debug_console_flush(edge_logger_output_t* output) {
    (void)output;
}

static void debug_console_destroy(edge_logger_output_t* output) {
    if (!output) {
        return;
    }

    edge_logger_output_debug_console_t* debug_console_output = (edge_logger_output_debug_console_t*)output;
    edge_allocator_free(output->allocator, debug_console_output);
}

static const edge_logger_output_vtable_t debug_console_vtable = {
    .write = debug_console_write,
    .flush = debug_console_flush,
    .destroy = debug_console_destroy
};

edge_logger_output_t* edge_logger_create_debug_console_output(const edge_allocator_t* allocator, int format_flags) {
    if (!allocator) {
        return NULL;
    }

    edge_logger_output_debug_console_t* ouptut = (edge_logger_output_debug_console_t*)edge_allocator_malloc(allocator, sizeof(edge_logger_output_debug_console_t));
    if (!ouptut) {
        return NULL;
    }

    ouptut->base.vtable = &debug_console_vtable;
    ouptut->base.format_flags = format_flags;
    ouptut->base.allocator = allocator;
    ouptut->base.user_data = NULL;

    return (edge_logger_output_t*)ouptut;
}