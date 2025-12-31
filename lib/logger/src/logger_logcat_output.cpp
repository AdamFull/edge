#include "logger.hpp"
#include "allocator.hpp"

#if defined(__ANDROID__)

#include <android/log.h>
#include <string.h>

namespace edge {
	struct LoggerOutputLogcat {
		LoggerOutput base;
	};

	static i32 get_android_priority(LogLevel level) {
		switch (level) {
		case LogLevel::Trace:
		case LogLevel::Debug:
			return ANDROID_LOG_DEBUG;
		case LogLevel::Info:
			return ANDROID_LOG_INFO;
		case LogLevel::Warn:
			return ANDROID_LOG_WARN;
		case LogLevel::Error:
			return ANDROID_LOG_ERROR;
		case LogLevel::Fatal:
			return ANDROID_LOG_FATAL;
		default:
			return ANDROID_LOG_DEFAULT;
		}
	}

	static void logcat_write(LoggerOutput* output, const LogEntry* entry) {
		if (!output || !entry) {
			return;
		}

		char buffer[EDGE_LOGGER_BUFFER_SIZE];
		/* Strip color codes for Android logcat */
		i32 format_flags = output->format_flags & ~LogFormat_Color;
		logger_format_entry(buffer, sizeof(buffer), entry, format_flags);

		i32 priority = get_android_priority(entry->level);
		__android_log_print(priority, "EdgeLogger", "%s", buffer);
	}

	static void logcat_flush(LoggerOutput* output) {
		(void)output;
	}

	static void logcat_destroy(LoggerOutput* output) {
		if (!output) {
			return;
		}

		LoggerOutputLogcat* logcat_output = (LoggerOutputLogcat*)output;
        output->allocator->deallocate(logcat_output);
	}

	static const LoggerOutputVTable logcat_vtable = {
		.write = logcat_write,
		.flush = logcat_flush,
		.destroy = logcat_destroy
	};

	LoggerOutput* logger_create_logcat_output(const Allocator* allocator, i32 format_flags) {
		if (!allocator) {
			return nullptr;
		}

		LoggerOutputLogcat* output = allocator->allocate<LoggerOutputLogcat>();
		if (!output) {
			return nullptr;
		}

		output->base.vtable = &logcat_vtable;
		output->base.format_flags = format_flags;
		output->base.allocator = allocator;
		output->base.user_data = nullptr;

		return (LoggerOutput*)output;
	}
}

#endif