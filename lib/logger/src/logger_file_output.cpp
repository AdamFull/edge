#include "allocator.hpp"
#include "logger.hpp"

#include <stdio.h>
#include <string.h>

namespace edge {
struct LoggerOutputFile final : ILoggerOutput {
  i32 format_flags = 0;

  FILE *file = nullptr;
  bool auto_flush = false;

  void write(const LogEntry *entry) override {
    char buffer[EDGE_LOGGER_BUFFER_SIZE];
    /* Strip color codes for file output */
    logger_format_entry(buffer, sizeof(buffer), entry,
                        format_flags & ~LogFormat_Color);

    fprintf(file, "%s\n", buffer);

    if (auto_flush) {
      fflush(file);
    }
  }

  void flush() override {
    if (file) {
      fflush(file);
    }
  }

  void destroy() override {
    if (file) {
      fclose(file);
    }
  }
};

ILoggerOutput *logger_create_file_output(NotNull<const Allocator *> alloc,
                                         i32 format_flags,
                                         const char *file_path,
                                         bool auto_flush) {
  LoggerOutputFile *output = alloc->allocate<LoggerOutputFile>();
  if (!output) {
    return nullptr;
  }

  output->file = fopen(file_path, "a");
  if (!output->file) {
    alloc->deallocate(output);
    return nullptr;
  }

  output->format_flags = format_flags;
  output->auto_flush = auto_flush;
  return output;
}
} // namespace edge