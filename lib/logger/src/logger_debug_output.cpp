#include "logger.hpp"
#include "allocator.hpp"

#if EDGE_HAS_WINDOWS_API

#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace edge {
	struct LoggerOutputDebugConsole {
		LoggerOutput base;
	};

	static void debug_console_write(LoggerOutput* output, const LogEntry* entry) {
		if (!output || !entry) {
			return;
		}

		char buffer[EDGE_LOGGER_BUFFER_SIZE];
		// debug console does not have color support
		i32 format_flags = output->format_flags & ~LogFormat_Color;
		logger_format_entry(buffer, sizeof(buffer), entry, format_flags);

		OutputDebugStringA(buffer);
		OutputDebugStringA("\n");
	}

	static void debug_console_flush(LoggerOutput* output) {
		(void)output;
	}

	static void debug_console_destroy(LoggerOutput* output) {
		if (!output) {
			return;
		}

		LoggerOutputDebugConsole* debug_console_output = (LoggerOutputDebugConsole*)output;
		deallocate(output->allocator, debug_console_output);
	}

	static const LoggerOutputVTable debug_console_vtable = {
		.write = debug_console_write,
		.flush = debug_console_flush,
		.destroy = debug_console_destroy
	};

	LoggerOutput* logger_create_debug_console_output(const Allocator* allocator, i32 format_flags) {
		if (!allocator) {
			return nullptr;
		}

		LoggerOutputDebugConsole* output = allocate<LoggerOutputDebugConsole>(allocator);
		if (!output) {
			return nullptr;
		}

		output->base.vtable = &debug_console_vtable;
		output->base.format_flags = format_flags;
		output->base.allocator = allocator;
		output->base.user_data = nullptr;

		return (LoggerOutput*)output;
	}
}

#endif