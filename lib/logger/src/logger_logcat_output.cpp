#include "logger.hpp"
#include "allocator.hpp"

#if defined(__ANDROID__)

#include <android/log.h>
#include <cstring>

namespace edge {
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

	struct LoggerOutputLogcat final : ILoggerOutput {
		i32 format_flags = 0;

		void write(const LogEntry* entry) noexcept override {
			char buffer[EDGE_LOGGER_BUFFER_SIZE];
			/* Strip color codes for Android logcat */
			logger_format_entry(buffer, sizeof(buffer), entry, format_flags & ~LogFormat_Color);

			i32 priority = get_android_priority(entry->level);
			__android_log_print(priority, "EdgeLogger", "%s", buffer);
		}

		void flush() noexcept override {
		}

		void destroy() noexcept override {
		}
	};

	ILoggerOutput* logger_create_logcat_output(NotNull<const Allocator*> alloc, i32 format_flags) {
		LoggerOutputLogcat* output = alloc->allocate<LoggerOutputLogcat>();
		if (!output) {
			return nullptr;
		}

		output->format_flags = format_flags;
		return output;
	}
}

#endif