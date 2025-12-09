#include "logger.hpp"
#include "allocator.hpp"

#include <stdio.h>
#include <string.h>

namespace edge {
	struct LoggerOutputStdout {
		LoggerOutput base;
	};

	static void stdout_write(LoggerOutput* output, const LogEntry* entry) {
		if (!output || !entry) {
			return;
		}

		char buffer[EDGE_LOGGER_BUFFER_SIZE];
		logger_format_entry(buffer, sizeof(buffer), entry, output->format_flags);
		printf("%s\n", buffer);
	}

	static void stdout_flush(LoggerOutput* output) {
		(void)output;
		fflush(stdout);
	}

	static void stdout_destroy(LoggerOutput* output) {
		if (!output) {
			return;
		}

		LoggerOutputStdout* stdout_output = (LoggerOutputStdout*)output;
		deallocate(output->allocator, stdout_output);
	}

	static const LoggerOutputVTable stdout_vtable = {
		.write = stdout_write,
		.flush = stdout_flush,
		.destroy = stdout_destroy
	};

	LoggerOutput* logger_create_stdout_output(const Allocator* allocator, i32 format_flags) {
		if (!allocator) {
			return nullptr;
		}

		LoggerOutputStdout* output = allocate<LoggerOutputStdout>(allocator);
		if (!output) {
			return nullptr;
		}

		output->base.vtable = &stdout_vtable;
		output->base.format_flags = format_flags;
		output->base.allocator = allocator;
		output->base.user_data = nullptr;

		return (LoggerOutput*)output;
	}
}