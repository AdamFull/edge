#include "../entry_point.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "../../gfx/vulkan/vk_context.h"

namespace edge::platform {
	auto WindowsPlatformContext::construct(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow)
		-> std::unique_ptr<WindowsPlatformContext> {
		auto ctx = std::make_unique<WindowsPlatformContext>();
		ctx->_construct(hInstance, hPrevInstance, lpCmdLine, nCmdShow);
		return ctx;
	}

	auto WindowsPlatformContext::_construct(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow) -> bool {
		hInstance_ = hInstance;
		hPrevInstance_ = hPrevInstance;
		lpCmdLine_ = lpCmdLine;
		nCmdShow_ = nCmdShow;

		// Attempt to attach to the parent process console if it exists
		if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
			// No parent console, allocate a new one for this process
			if (!AllocConsole()) {
				return false;
			}
		}

		FILE* fp;
		freopen_s(&fp, "conin$", "r", stdin);
		freopen_s(&fp, "conout$", "w", stdout);
		freopen_s(&fp, "conout$", "w", stderr);

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
		logger->set_pattern("[%Y-%m-%d %H:%M:%S] [%^%l%$] %v");
		spdlog::set_default_logger(logger);

		window_ = DesktopPlatformWindow::construct(this);
		if (!window_) {
			spdlog::error("[Windows Runtime Context]: Window construction failed");
			return false;
		}

		auto* desktop_window = static_cast<DesktopPlatformWindow*>(window_.get());

		input_ = DesktopPlatformInput::construct(desktop_window);
		if (!input_) {
			spdlog::error("[Windows Runtime Context]: Input construction failed");
			return false;
		}

		graphics_ = gfx::VulkanGraphicsContext::construct({
			.physical_device_type = gfx::GraphicsDeviceType::eDiscrete,
			//.hwnd = glfwGetWin32Window(desktop_window->get_handle()),
			.hinst = hInstance
			});
		if (!graphics_) {
			spdlog::error("[Windows Runtime Context]: Graphics construction failed");
			return false;
		}

		return true;
	}

	auto WindowsPlatformContext::get_platform_name() const -> std::string_view {
		return "Windows";
	}

	auto WindowsPlatformContext::shutdown() -> void {
		if (window_) {
			window_->destroy();
			window_.reset();
		}
	}
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow) {
	auto context = edge::platform::PlatformContext::construct(hInstance, hPrevInstance, lpCmdLine, nCmdShow);
	return platform_main(*context);
}