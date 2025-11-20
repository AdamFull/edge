#include "edge_logger_internal.h"

#include <android/log.h>
#include <string.h>
#include <edge_allocator.h>

typedef struct edge_logger_output_logcat {
    edge_logger_output_t base;
} edge_logger_output_logcat_t;

static int get_android_priority(edge_log_level_t level) {
    switch (level) {
    case EDGE_LOG_LEVEL_TRACE:
    case EDGE_LOG_LEVEL_DEBUG:
        return ANDROID_LOG_DEBUG;
    case EDGE_LOG_LEVEL_INFO:
        return ANDROID_LOG_INFO;
    case EDGE_LOG_LEVEL_WARN:
        return ANDROID_LOG_WARN;
    case EDGE_LOG_LEVEL_ERROR:
        return ANDROID_LOG_ERROR;
    case EDGE_LOG_LEVEL_FATAL:
        return ANDROID_LOG_FATAL;
    default:
        return ANDROID_LOG_DEFAULT;
    }
}

static void platform_write(edge_logger_output_t* output, const edge_log_entry_t* entry) {
    if (!output || !entry) {
        return;
    }

    char buffer[EDGE_LOGGER_BUFFER_SIZE];
    /* Strip color codes for Android logcat */
    int format_flags = output->format_flags & ~EDGE_LOG_FORMAT_COLOR;
    edge_logger_format_entry(buffer, sizeof(buffer), entry, format_flags);

    int priority = get_android_priority(entry->level);
    __android_log_print(priority, "EdgeLogger", "%s", buffer);
}

static void platform_flush(edge_logger_output_t* output) {
    (void)output;
}

static void platform_destroy(edge_logger_output_t* output) {
    if (!output) {
        return;
    }

    edge_log_writer_platform_t* platform_writer = (edge_log_writer_platform_t*)output;
    edge_allocator_free(output->allocator, platform_writer);
}

static const edge_logger_output_vtable_t platform_vtable = {
    .write = platform_write,
    .flush = platform_flush,
    .destroy = platform_destroy
};

edge_logger_output_t* edge_logger_create_logcat_output(const edge_allocator_t* allocator, int format_flags) {
    if (!allocator) {
        return NULL;
    }

    edge_logger_output_logcat_t* output = (edge_logger_output_logcat_t*)edge_allocator_malloc(allocator, sizeof(edge_logger_output_logcat_t));
    if (!output) {
        return NULL;
    }

    output->base.vtable = &platform_vtable;
    output->base.format_flags = format_flags;
    output->base.allocator = allocator;
    output->base.user_data = NULL;

    return (edge_logger_output_t*)output;
}