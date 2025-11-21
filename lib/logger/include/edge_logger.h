#ifndef EDGE_LOGGER_H
#define EDGE_LOGGER_H

#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct edge_allocator edge_allocator_t;
    typedef struct edge_mtx edge_mtx_t;
    typedef struct edge_vector edge_vector_t;

    typedef enum edge_log_level {
        EDGE_LOG_LEVEL_TRACE = 0,
        EDGE_LOG_LEVEL_DEBUG = 1,
        EDGE_LOG_LEVEL_INFO = 2,
        EDGE_LOG_LEVEL_WARN = 3,
        EDGE_LOG_LEVEL_ERROR = 4,
        EDGE_LOG_LEVEL_FATAL = 5,
        EDGE_LOG_LEVEL_NONE = 6
    } edge_log_level_t;

    typedef enum edge_log_format {
        EDGE_LOG_FORMAT_NONE = 0,
        EDGE_LOG_FORMAT_TIMESTAMP = 1 << 0,
        EDGE_LOG_FORMAT_LEVEL = 1 << 1,
        EDGE_LOG_FORMAT_FILE = 1 << 2,
        EDGE_LOG_FORMAT_LINE = 1 << 3,
        EDGE_LOG_FORMAT_FUNCTION = 1 << 4,
        EDGE_LOG_FORMAT_THREAD_ID = 1 << 5,
        EDGE_LOG_FORMAT_COLOR = 1 << 6,
        EDGE_LOG_FORMAT_DEFAULT = EDGE_LOG_FORMAT_TIMESTAMP | EDGE_LOG_FORMAT_LEVEL | EDGE_LOG_FORMAT_FILE | EDGE_LOG_FORMAT_LINE
    } edge_log_format_t;

    typedef struct edge_log_entry edge_log_entry_t;
    typedef struct edge_logger_output edge_logger_output_t;
    typedef struct edge_logger edge_logger_t;

    /**
     * Create a new logger
     *
     * @param allocator Memory allocator to use
     * @param min_level Minimum log level to output
     * @return Logger instance or NULL on failure
     */
    edge_logger_t* edge_logger_create(const edge_allocator_t* allocator, edge_log_level_t min_level);

    /**
     * Destroy logger and free resources
     */
    void edge_logger_destroy(edge_logger_t* logger);

    /**
     * Add a writer to the logger
     *
     * @param logger Logger instance
     * @param writer Writer to add (logger takes ownership)
     * @return true on success
     */
    bool edge_logger_add_output(edge_logger_t* logger, edge_logger_output_t* output);

    /**
     * Set minimum log level
     */
    void edge_logger_set_level(edge_logger_t* logger, edge_log_level_t level);

    /**
     * Flush all writers
     */
    void edge_logger_flush(edge_logger_t* logger);

    /**
     * Log a message with full context information
     *
     * @param logger Logger instance
     * @param level Log level
     * @param file Source file name
     * @param line Line number
     * @param func Function name
     * @param format Printf-style format string
     */
    void edge_logger_log(edge_logger_t* logger, edge_log_level_t level,
        const char* file, int line, const char* func, 
        const char* format, ...);
    
    /**
     * Log a message with va_list
     */
    void edge_logger_vlog(edge_logger_t* logger, edge_log_level_t level,
        const char* file, int line, const char* func,
        const char* format, va_list args);

    /**
     * Get string representation of log level
     */
    const char* edge_logger_level_string(edge_log_level_t level);

    /**
     * Get ANSI color code for log level
     */
    const char* edge_logger_level_color(edge_log_level_t level);

    /**
     * Format a log entry into a string buffer
     *
     * @param buffer Output buffer
     * @param buffer_size Size of buffer
     * @param entry Log entry to format
     * @param format_flags Format options
     * @return Number of characters written (excluding null terminator)
     */
    int edge_logger_format_entry(char* buffer, size_t buffer_size, const edge_log_entry_t* entry, int format_flags);

    /**
     * Set the global default logger
     */
    void edge_logger_set_global(edge_logger_t* logger);

    /**
     * Get the global default logger
     */
    edge_logger_t* edge_logger_get_global(void);

#if _WIN32 || _WIN64
    edge_logger_output_t* edge_logger_create_debug_console_output(const edge_allocator_t* allocator, int format_flags);
#elif defined(__ANDROID__)
    edge_logger_output_t* edge_logger_create_logcat_output(const edge_allocator_t* allocator, int format_flags);
#endif

    edge_logger_output_t* edge_logger_create_file_output(const edge_allocator_t* allocator, int format_flags, const char* file_path, bool auto_flush);

    edge_logger_output_t* edge_logger_create_stdout_output(const edge_allocator_t* allocator, int format_flags);

#define EDGE_LOG_TRACE(...) \
    edge_logger_log(edge_logger_get_global(), EDGE_LOG_LEVEL_TRACE, __FILE__, __LINE__, __func__, __VA_ARGS__)

#define EDGE_LOG_DEBUG(...) \
    edge_logger_log(edge_logger_get_global(), EDGE_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__, __VA_ARGS__)

#define EDGE_LOG_INFO(...) \
    edge_logger_log(edge_logger_get_global(), EDGE_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__, __VA_ARGS__)

#define EDGE_LOG_WARN(...) \
    edge_logger_log(edge_logger_get_global(), EDGE_LOG_LEVEL_WARN, __FILE__, __LINE__, __func__, __VA_ARGS__)

#define EDGE_LOG_ERROR(...) \
    edge_logger_log(edge_logger_get_global(), EDGE_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__, __VA_ARGS__)

#define EDGE_LOG_FATAL(...) \
    edge_logger_log(edge_logger_get_global(), EDGE_LOG_LEVEL_FATAL, __FILE__, __LINE__, __func__, __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* EDGE_LOGGER_H */
