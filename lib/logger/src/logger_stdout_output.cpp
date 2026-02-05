#include "allocator.hpp"
#include "logger.hpp"

#include <stdio.h>
#include <string.h>

namespace edge {
struct LoggerOutputStdout final : ILoggerOutput {
  i32 format_flags = 0;

  void write(const LogEntry *entry) override {
    char buffer[EDGE_LOGGER_BUFFER_SIZE];
    logger_format_entry(buffer, sizeof(buffer), entry, format_flags);
    printf("%s\n", buffer);
  }

  void flush() override { fflush(stdout); }

  void destroy() override {}
};

ILoggerOutput *logger_create_stdout_output(NotNull<const Allocator *> alloc,
                                           i32 format_flags) {
  LoggerOutputStdout *output = alloc->allocate<LoggerOutputStdout>();
  if (!output) {
    return nullptr;
  }

  output->format_flags = format_flags;
  return output;
}
} // namespace edge