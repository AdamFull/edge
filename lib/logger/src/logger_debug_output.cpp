#include "allocator.hpp"
#include "logger.hpp"

#if EDGE_HAS_WINDOWS_API

#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace edge {
struct LoggerOutputDebugConsole final : ILoggerOutput {
  i32 format_flags = 0;

  void write(const LogEntry *entry) override {
    char buffer[EDGE_LOGGER_BUFFER_SIZE];

    // debug console does not have color support
    logger_format_entry(buffer, sizeof(buffer), entry,
                        format_flags & ~LogFormat_Color);

    OutputDebugStringA(buffer);
    OutputDebugStringA("\n");
  }

  void flush() override {}

  void destroy() override {}
};

ILoggerOutput *
logger_create_debug_console_output(NotNull<const Allocator *> alloc,
                                   i32 format_flags) {
  LoggerOutputDebugConsole *output =
      alloc->allocate<LoggerOutputDebugConsole>();
  if (!output) {
    return nullptr;
  }

  output->format_flags = format_flags;
  return output;
}
} // namespace edge

#endif