#ifndef EDGE_LOGGER_INTERNAL_H
#define EDGE_LOGGER_INTERNAL_H

#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>

#include "edge_logger.h"

#ifndef EDGE_LOGGER_BUFFER_SIZE
#define EDGE_LOGGER_BUFFER_SIZE 4096
#endif

#ifdef __cplusplus
extern "C" {
#endif

    struct edge_log_entry {
        edge_log_level_t level;
        const char* message;
        const char* file;
        int line;
        const char* func;
        unsigned long thread_id;
        char timestamp[32];
    };

    typedef struct edge_logger_output_vtable {
        void (*write)(edge_logger_output_t* self, const edge_log_entry_t* entry);
        void (*flush)(edge_logger_output_t* self);
        void (*destroy)(edge_logger_output_t* self);
    } edge_logger_output_vtable_t;

    struct edge_logger_output {
        const edge_logger_output_vtable_t* vtable;
        int format_flags;
        const edge_allocator_t* allocator;
        void* user_data;
    };

    struct edge_logger {
        edge_log_level_t min_level;
        edge_vector_t* outputs;
        edge_mtx_t* mutex;
        const edge_allocator_t* allocator;
    };

#ifdef __cplusplus
}
#endif

#endif /* EDGE_LOGGER_INTERNAL_H */
