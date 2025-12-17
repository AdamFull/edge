#include "logger.hpp"
#include "allocator.hpp"

#include <stdio.h>
#include <string.h>

namespace edge {
	struct LoggerOutputFile {
		LoggerOutput base;
		FILE* file;
		bool auto_flush;
	};

	static void file_write(LoggerOutput* output, const LogEntry* entry) {
		if (!output || !entry) {
			return;
		}

		LoggerOutputFile* file_output = (LoggerOutputFile*)output;
		if (!file_output->file) {
			return;
		}

		char buffer[EDGE_LOGGER_BUFFER_SIZE];
		/* Strip color codes for file output */
		i32 format_flags = output->format_flags & ~LogFormat_Color;
		logger_format_entry(buffer, sizeof(buffer), entry, format_flags);

		fprintf(file_output->file, "%s\n", buffer);

		if (file_output->auto_flush) {
			fflush(file_output->file);
		}
	}

	static void file_flush(LoggerOutput* output) {
		if (!output) {
			return;
		}

		LoggerOutputFile* file_output = (LoggerOutputFile*)output;
		if (file_output->file) {
			fflush(file_output->file);
		}
	}

	static void file_destroy(LoggerOutput* output) {
		if (!output) {
			return;
		}

		LoggerOutputFile* file_output = (LoggerOutputFile*)output;
		if (file_output->file) {
			fclose(file_output->file);
		}

		output->allocator->deallocate(file_output);
	}

	static const LoggerOutputVTable file_vtable = {
		.write = file_write,
		.flush = file_flush,
		.destroy = file_destroy
	};

	LoggerOutput* logger_create_file_output(const Allocator* allocator, i32 format_flags, const char* file_path, bool auto_flush) {
		if (!allocator || !file_path) {
			return nullptr;
		}

		LoggerOutputFile* output = allocator->allocate<LoggerOutputFile>();
		if (!output) {
			return nullptr;
		}

		output->file = fopen(file_path, "a");
		if (!output->file) {
			allocator->deallocate(output);
			return nullptr;
		}

		output->base.vtable = &file_vtable;
		output->base.format_flags = format_flags;
		output->base.allocator = allocator;
		output->base.user_data = nullptr;
		output->auto_flush = auto_flush;

		return (LoggerOutput*)output;
	}
}