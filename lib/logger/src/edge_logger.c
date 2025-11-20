#include "edge_logger_internal.h"

#include <edge_allocator.h>
#include <edge_threads.h>
#include <edge_vector.h>

#include <stdio.h>
#include <string.h>
#include <time.h>

#define ANSI_COLOR_RESET   "\x1b[0m"
#define ANSI_COLOR_TRACE   "\x1b[37m"      /* White */
#define ANSI_COLOR_DEBUG   "\x1b[36m"      /* Cyan */
#define ANSI_COLOR_INFO    "\x1b[32m"      /* Green */
#define ANSI_COLOR_WARN    "\x1b[33m"      /* Yellow */
#define ANSI_COLOR_ERROR   "\x1b[31m"      /* Red */
#define ANSI_COLOR_FATAL   "\x1b[35;1m"    /* Bold Magenta */

static edge_logger_t* g_global_logger = NULL;

static const char* get_filename(const char* path) {
    if (!path) {
        return "";
    }

    const char* filename = path;
    const char* p = path;

    while (*p) {
        if (*p == '/' || *p == '\\') {
            filename = p + 1;
        }
        p++;
    }

    return filename;
}

int edge_logger_format_entry(char* buffer, size_t buffer_size, const edge_log_entry_t* entry, int format_flags) {
    if (!buffer || buffer_size == 0 || !entry) {
        return 0;
    }

    int pos = 0;

    const char* color = (format_flags & EDGE_LOG_FORMAT_COLOR) ? edge_logger_level_color(entry->level) : "";
    const char* reset = (format_flags & EDGE_LOG_FORMAT_COLOR) ? ANSI_COLOR_RESET : "";

    if (format_flags & EDGE_LOG_FORMAT_TIMESTAMP) {
        pos += snprintf(buffer + pos, buffer_size - pos, "[%s] ", entry->timestamp);
    }

    if (format_flags & EDGE_LOG_FORMAT_THREAD_ID) {
        pos += snprintf(buffer + pos, buffer_size - pos, "[%lu] ", entry->thread_id);
    }

    if (format_flags & EDGE_LOG_FORMAT_LEVEL) {
        pos += snprintf(buffer + pos, buffer_size - pos, "%s[%s]%s ", color, edge_logger_level_string(entry->level), reset);
    }

    if ((format_flags & EDGE_LOG_FORMAT_FILE) && entry->file) {
        const char* filename = get_filename(entry->file);
        if (format_flags & EDGE_LOG_FORMAT_LINE) {
            pos += snprintf(buffer + pos, buffer_size - pos, "[%s:%d] ", filename, entry->line);
        }
        else {
            pos += snprintf(buffer + pos, buffer_size - pos, "[%s] ", filename);
        }
    }

    if ((format_flags & EDGE_LOG_FORMAT_FUNCTION) && entry->func) {
        pos += snprintf(buffer + pos, buffer_size - pos, "<%s> ", entry->func);
    }

    if (entry->message) {
        pos += snprintf(buffer + pos, buffer_size - pos, "%s", entry->message);
    }

    return pos;
}

edge_logger_t* edge_logger_create(const edge_allocator_t* allocator, edge_log_level_t min_level) {
    if (!allocator) {
        return NULL;
    }

    edge_logger_t* logger = (edge_logger_t*)edge_allocator_malloc(allocator, sizeof(edge_logger_t));
    if (!logger) {
        return NULL;
    }

    logger->min_level = min_level;
    logger->allocator = allocator;

    logger->outputs = edge_vector_create(allocator, sizeof(edge_logger_output_t*), 4);
    if (!logger->outputs) {
        edge_allocator_free(allocator, logger);
        return NULL;
    }

    logger->mutex = (edge_mtx_t*)edge_allocator_malloc(allocator, sizeof(edge_mtx_t));
    if (!logger->mutex) {
        edge_vector_destroy(logger->outputs);
        edge_allocator_free(allocator, logger);
        return NULL;
    }

    if (edge_mtx_init(logger->mutex, edge_mtx_plain) != 0) {
        edge_allocator_free(allocator, logger->mutex);
        edge_vector_destroy(logger->outputs);
        edge_allocator_free(allocator, logger);
        return NULL;
    }

    return logger;
}

void edge_logger_destroy(edge_logger_t* logger) {
    if (!logger) {
        return;
    }

    if (logger->outputs) {
        size_t count = edge_vector_size(logger->outputs);
        for (size_t i = 0; i < count; i++) {
            edge_logger_output_t** output_ptr = (edge_logger_output_t**)edge_vector_at(logger->outputs, i);
            if (output_ptr && *output_ptr) {
                edge_logger_output_t* output = *output_ptr;
                if (output->vtable && output->vtable->destroy) {
                    output->vtable->destroy(output);
                }
            }
        }
        edge_vector_destroy(logger->outputs);
    }

    if (logger->mutex) {
        edge_mtx_destroy(logger->mutex);
        edge_allocator_free(logger->allocator, logger->mutex);
    }

    edge_allocator_free(logger->allocator, logger);
}

bool edge_logger_add_output(edge_logger_t* logger, edge_logger_output_t* output) {
    if (!logger || !output) {
        return false;
    }

    edge_mtx_lock(logger->mutex);
    bool result = edge_vector_push_back(logger->outputs, &output);
    edge_mtx_unlock(logger->mutex);

    return result;
}

void edge_logger_set_level(edge_logger_t* logger, edge_log_level_t level) {
    if (logger) {
        logger->min_level = level;
    }
}

void edge_logger_flush(edge_logger_t* logger) {
    if (!logger || !logger->outputs) {
        return;
    }

    edge_mtx_lock(logger->mutex);

    size_t count = edge_vector_size(logger->outputs);
    for (size_t i = 0; i < count; i++) {
        edge_logger_output_t** output_ptr = (edge_logger_output_t**)edge_vector_at(logger->outputs, i);
        if (output_ptr && *output_ptr) {
            edge_logger_output_t* output = *output_ptr;
            if (output->vtable && output->vtable->flush) {
                output->vtable->flush(output);
            }
        }
    }

    edge_mtx_unlock(logger->mutex);
}

void edge_logger_vlog(edge_logger_t* logger, edge_log_level_t level,
    const char* file, int line, const char* func,
    const char* format, va_list args) {
    if (!logger || level < logger->min_level || !logger->outputs) {
        return;
    }

    char message[EDGE_LOGGER_BUFFER_SIZE];
    vsnprintf(message, sizeof(message), format, args);

    edge_log_entry_t entry;
    entry.level = level;
    entry.message = message;
    entry.file = file;
    entry.line = line;
    entry.func = func;
    entry.thread_id = edge_thrd_current_thread_id();

    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);

    if (tm_info) {
        strftime(entry.timestamp, sizeof(entry.timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    }
    else {
        snprintf(entry.timestamp, sizeof(entry.timestamp), "UNKNOWN");
    }

    edge_mtx_lock(logger->mutex);

    size_t count = edge_vector_size(logger->outputs);
    for (size_t i = 0; i < count; i++) {
        edge_logger_output_t** output_ptr = (edge_logger_output_t**)edge_vector_at(logger->outputs, i);
        if (output_ptr && *output_ptr) {
            edge_logger_output_t* output = *output_ptr;
            if (output->vtable && output->vtable->write) {
                output->vtable->write(output, &entry);
            }
        }
    }

    edge_mtx_unlock(logger->mutex);
}

void edge_logger_log(edge_logger_t* logger, edge_log_level_t level,
    const char* file, int line, const char* func,
    const char* format, ...) {
    if (!logger) {
        return;
    }

    va_list args;
    va_start(args, format);
    edge_logger_vlog(logger, level, file, line, func, format, args);
    va_end(args);
}

void edge_logger_set_global(edge_logger_t* logger) {
    g_global_logger = logger;
}

edge_logger_t* edge_logger_get_global(void) {
    return g_global_logger;
}

const char* edge_logger_level_string(edge_log_level_t level) {
    switch (level) {
    case EDGE_LOG_LEVEL_TRACE: return "TRACE";
    case EDGE_LOG_LEVEL_DEBUG: return "DEBUG";
    case EDGE_LOG_LEVEL_INFO:  return "INFO";
    case EDGE_LOG_LEVEL_WARN:  return "WARN";
    case EDGE_LOG_LEVEL_ERROR: return "ERROR";
    case EDGE_LOG_LEVEL_FATAL: return "FATAL";
    default: return "UNKNOWN";
    }
}

const char* edge_logger_level_color(edge_log_level_t level) {
    switch (level) {
    case EDGE_LOG_LEVEL_TRACE: return ANSI_COLOR_TRACE;
    case EDGE_LOG_LEVEL_DEBUG: return ANSI_COLOR_DEBUG;
    case EDGE_LOG_LEVEL_INFO:  return ANSI_COLOR_INFO;
    case EDGE_LOG_LEVEL_WARN:  return ANSI_COLOR_WARN;
    case EDGE_LOG_LEVEL_ERROR: return ANSI_COLOR_ERROR;
    case EDGE_LOG_LEVEL_FATAL: return ANSI_COLOR_FATAL;
    default: return ANSI_COLOR_RESET;
    }
}