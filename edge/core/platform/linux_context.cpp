#include "entry_point.h"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "desktop_input.h"
#include "desktop_window.h"

#define EDGE_LOGGER_SCOPE "platform::LinuxPlatformContext"

namespace edge::platform {
	auto LinuxPlatformContext::construct() -> std::unique_ptr<LinuxPlatformContext> {
		auto ctx = std::make_unique<LinuxPlatformContext>();
		ctx->_construct();
		return ctx;
	}

	auto LinuxPlatformContext::_construct() -> bool {
		std::ios::sync_with_stdio(true);

#ifdef NDEBUG
		static constexpr auto log_level{ spdlog::level::info };
#else
		static constexpr auto log_level{ spdlog::level::trace };
#endif

		// Init logger
		spdlog::sinks_init_list sink_list{
			std::make_shared<spdlog::sinks::basic_file_sink_mt>("log.log", true),
			std::make_shared<spdlog::sinks::stdout_color_sink_mt>()
		};

		for (auto& sink : sink_list) {
			sink->set_level(log_level);
		}

		auto logger = std::make_shared<spdlog::logger>("Logger", sink_list.begin(), sink_list.end());
		logger->set_level(log_level);
		logger->set_pattern(EDGE_LOGGER_PATTERN);
		spdlog::set_default_logger(logger);

		window_ = DesktopPlatformWindow::construct(this);
		if (!window_) {
			EDGE_SLOGE("Window construction failed");
			return false;
		}

		input_ = DesktopPlatformInput::construct(static_cast<DesktopPlatformWindow*>(window_.get()));
		if (!input_) {
			EDGE_SLOGE("Input construction failed");
			return false;
		}

		return true;
	}

	auto LinuxPlatformContext::get_platform_name() const -> std::string_view {
		return "Linux";
	}

	auto LinuxPlatformContext::shutdown() -> void {
		if (window_) {
			window_.reset();
		}
	}
}

int main(int argc, char* argv[]) {
	auto context = edge::platform::PlatformContext::construct();
	return platform_main(*context);
}

#undef EDGE_LOGGER_SCOPE