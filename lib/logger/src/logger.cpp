#include "logger.hpp"

#include <stdio.h>
#include <string.h>
#include <time.h>

#define ANSI_COLOR_RESET "\x1b[0m"
#define ANSI_COLOR_TRACE "\x1b[37m"   /* White */
#define ANSI_COLOR_DEBUG "\x1b[36m"   /* Cyan */
#define ANSI_COLOR_INFO "\x1b[32m"    /* Green */
#define ANSI_COLOR_WARN "\x1b[33m"    /* Yellow */
#define ANSI_COLOR_ERROR "\x1b[31m"   /* Red */
#define ANSI_COLOR_FATAL "\x1b[35;1m" /* Bold Magenta */

namespace edge {
static Logger *g_global_logger = nullptr;

static const char *get_filename(const char *path) {
  if (!path) {
    return "";
  }

  const char *filename = path;
  const char *p = path;

  while (*p) {
    if (*p == '/' || *p == '\\') {
      filename = p + 1;
    }
    p++;
  }

  return filename;
}

i32 logger_format_entry(char *buffer, usize buffer_size, const LogEntry *entry,
                        i32 format_flags) {
  if (!buffer || buffer_size == 0 || !entry) {
    return 0;
  }

  i32 pos = 0;

  const char *color =
      (format_flags & LogFormat_Color) ? logger_level_color(entry->level) : "";
  const char *reset = (format_flags & LogFormat_Color) ? ANSI_COLOR_RESET : "";

  if (format_flags & LogFormat_Timestamp) {
    pos += snprintf(buffer + pos, buffer_size - pos, "[%s] ", entry->timestamp);
  }

  // TODO: Need to do it after fix issuer with fibers and threading
  // if (format_flags & LogFormat_ThreadId) {
  //	pos += snprintf(buffer + pos, buffer_size - pos, "[%u] ",
  //entry->thread_id);
  //}

  if (format_flags & LogFormat_Level) {
    pos += snprintf(buffer + pos, buffer_size - pos, "%s[%s]%s ", color,
                    logger_level_string(entry->level), reset);
  }

  if ((format_flags & LogFormat_File) && entry->file) {
    const char *filename = get_filename(entry->file);
    if (format_flags & LogFormat_Line) {
      pos += snprintf(buffer + pos, buffer_size - pos, "[%s:%d] ", filename,
                      entry->line);
    } else {
      pos += snprintf(buffer + pos, buffer_size - pos, "[%s] ", filename);
    }
  }

  if ((format_flags & LogFormat_Function) && entry->func) {
    pos += snprintf(buffer + pos, buffer_size - pos, "<%s> ", entry->func);
  }

  if (entry->message) {
    pos += snprintf(buffer + pos, buffer_size - pos, "%s", entry->message);
  }

  return pos;
}

bool Logger::create(NotNull<const Allocator *> alloc, LogLevel min_level) {
  this->min_level = min_level;
  return outputs.reserve(alloc, 4);
}

void Logger::destroy(NotNull<const Allocator *> alloc) {
  for (ILoggerOutput *output : outputs) {
    if (output) {
      output->destroy();
      alloc->deallocate(output);
    }
  }
  outputs.destroy(alloc);
}

bool Logger::add_output(NotNull<const Allocator *> alloc,
                        ILoggerOutput *output) {
  if (!output) {
    return false;
  }
  return outputs.push_back(alloc, output);
}

void Logger::set_level(LogLevel level) { min_level = level; }

void Logger::flush() {
  for (ILoggerOutput *output : outputs) {
    if (output) {
      output->flush();
    }
  }
}

void logger_vlog(Logger *logger, LogLevel level, const char *file, i32 line,
                 const char *func, const char *format, va_list args) {
  if (!logger || level < logger->min_level) {
    return;
  }

  char message[EDGE_LOGGER_BUFFER_SIZE];
  vsnprintf(message, sizeof(message), format, args);

  LogEntry entry = {.level = level,
                    .message = message,
                    .file = file,
                    .line = line,
                    .func = func};

  time_t now = time(nullptr);
  struct tm *tm_info = localtime(&now);

  if (tm_info) {
    strftime(entry.timestamp, sizeof(entry.timestamp), "%Y-%m-%d %H:%M:%S",
             tm_info);
  } else {
    snprintf(entry.timestamp, sizeof(entry.timestamp), "UNKNOWN");
  }

  for (ILoggerOutput *output : logger->outputs) {
    if (output) {
      output->write(&entry);
    }
  }
}

void logger_log(Logger *logger, LogLevel level, const char *file, i32 line,
                const char *func, const char *format, ...) {
  if (!logger) {
    return;
  }

  va_list args;
  va_start(args, format);
  logger_vlog(logger, level, file, line, func, format, args);
  va_end(args);
}

void logger_set_global(Logger *logger) { g_global_logger = logger; }

Logger *logger_get_global() { return g_global_logger; }

const char *logger_level_string(LogLevel level) {
  switch (level) {
  case LogLevel::Trace:
    return "TRACE";
  case LogLevel::Debug:
    return "DEBUG";
  case LogLevel::Info:
    return "INFO";
  case LogLevel::Warn:
    return "WARN";
  case LogLevel::Error:
    return "ERROR";
  case LogLevel::Fatal:
    return "FATAL";
  default:
    return "UNKNOWN";
  }
}

const char *logger_level_color(LogLevel level) {
  switch (level) {
  case LogLevel::Trace:
    return ANSI_COLOR_TRACE;
  case LogLevel::Debug:
    return ANSI_COLOR_DEBUG;
  case LogLevel::Info:
    return ANSI_COLOR_INFO;
  case LogLevel::Warn:
    return ANSI_COLOR_WARN;
  case LogLevel::Error:
    return ANSI_COLOR_ERROR;
  case LogLevel::Fatal:
    return ANSI_COLOR_FATAL;
  default:
    return ANSI_COLOR_RESET;
  }
}
} // namespace edge