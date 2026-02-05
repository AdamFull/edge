#ifndef EDGE_LOGGER_H
#define EDGE_LOGGER_H

#include <array.hpp>
#include <stdarg.h>

#ifndef EDGE_LOGGER_BUFFER_SIZE
#define EDGE_LOGGER_BUFFER_SIZE 4096
#endif

namespace edge {
struct Allocator;

enum class LogLevel {
  Trace = 0,
  Debug = 1,
  Info = 2,
  Warn = 3,
  Error = 4,
  Fatal = 5,
  None = 6
};

enum LogFormat {
  LogFormat_None = 0,
  LogFormat_Timestamp = 1 << 0,
  LogFormat_Level = 1 << 1,
  LogFormat_File = 1 << 2,
  LogFormat_Line = 1 << 3,
  LogFormat_Function = 1 << 4,
  LogFormat_ThreadId = 1 << 5,
  LogFormat_Color = 1 << 6,
  LogFormat_Default =
      LogFormat_Timestamp | LogFormat_Level | LogFormat_File | LogFormat_Line
};

struct LogEntry {
  LogLevel level = LogLevel::Trace;
  const char *message = nullptr;
  const char *file = nullptr;
  i32 line = 0;
  const char *func = nullptr;
  char timestamp[32] = {};
};

struct ILoggerOutput {
  virtual ~ILoggerOutput() = default;

  virtual void write(const LogEntry *entry) = 0;
  virtual void flush() = 0;
  virtual void destroy() = 0;
};

struct Logger {
  LogLevel min_level = LogLevel::Trace;
  Array<ILoggerOutput *> outputs = {};

  bool create(NotNull<const Allocator *> alloc,
              LogLevel min_level = LogLevel::Trace);
  void destroy(NotNull<const Allocator *> alloc);

  bool add_output(NotNull<const Allocator *> alloc, ILoggerOutput *output);
  void set_level(LogLevel level);
  void flush();
};

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
void logger_log(Logger *logger, LogLevel level, const char *file, i32 line,
                const char *func, const char *format, ...);

/**
 * Log a message with va_list
 */
void logger_vlog(Logger *logger, LogLevel level, const char *file, i32 line,
                 const char *func, const char *format, va_list args);

/**
 * Get string representation of log level
 */
const char *logger_level_string(LogLevel level);

/**
 * Get ANSI color code for log level
 */
const char *logger_level_color(LogLevel level);

/**
 * Format a log entry into a string buffer
 *
 * @param buffer Output buffer
 * @param buffer_size Size of buffer
 * @param entry Log entry to format
 * @param format_flags Format options
 * @return Number of characters written (excluding null terminator)
 */
i32 logger_format_entry(char *buffer, usize buffer_size, const LogEntry *entry,
                        i32 format_flags);

/**
 * Set the global default logger
 */
void logger_set_global(Logger *logger);

/**
 * Get the global default logger
 */
Logger *logger_get_global();

#if EDGE_HAS_WINDOWS_API
ILoggerOutput *
logger_create_debug_console_output(NotNull<const Allocator *> alloc,
                                   i32 format_flags);
#elif defined(EDGE_HAS_ANDROID_NDK)
ILoggerOutput *logger_create_logcat_output(NotNull<const Allocator *> alloc,
                                           i32 format_flags);
#endif

ILoggerOutput *logger_create_file_output(NotNull<const Allocator *> alloc,
                                         i32 format_flags,
                                         const char *file_path,
                                         bool auto_flush);
ILoggerOutput *logger_create_stdout_output(NotNull<const Allocator *> alloc,
                                           i32 format_flags);

#define EDGE_LOG_TRACE(...)                                                    \
  edge::logger_log(edge::logger_get_global(), edge::LogLevel::Trace, __FILE__, \
                   __LINE__, __func__, __VA_ARGS__)

#define EDGE_LOG_DEBUG(...)                                                    \
  edge::logger_log(edge::logger_get_global(), edge::LogLevel::Debug, __FILE__, \
                   __LINE__, __func__, __VA_ARGS__)

#define EDGE_LOG_INFO(...)                                                     \
  edge::logger_log(edge::logger_get_global(), edge::LogLevel::Info, __FILE__,  \
                   __LINE__, __func__, __VA_ARGS__)

#define EDGE_LOG_WARN(...)                                                     \
  edge::logger_log(edge::logger_get_global(), edge::LogLevel::Warn, __FILE__,  \
                   __LINE__, __func__, __VA_ARGS__)

#define EDGE_LOG_ERROR(...)                                                    \
  edge::logger_log(edge::logger_get_global(), edge::LogLevel::Error, __FILE__, \
                   __LINE__, __func__, __VA_ARGS__)

#define EDGE_LOG_FATAL(...)                                                    \
  edge::logger_log(edge::logger_get_global(), edge::LogLevel::Fatal, __FILE__, \
                   __LINE__, __func__, __VA_ARGS__)

} // namespace edge

#endif /* EDGE_LOGGER_H */
