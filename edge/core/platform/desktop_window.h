#pragma once

#pragma once

#include "platform.h"

struct GLFWwindow;

namespace edge::platform {
	class DesktopPlatformInput;
	class DesktopPlatformWindow;

	class DesktopPlatformWindow final : public IPlatformWindow {
	public:
		DesktopPlatformWindow();
		~DesktopPlatformWindow() override;
		static auto construct(IPlatformContext* platform_context) -> std::unique_ptr<DesktopPlatformWindow>;

		[[nodiscard]] auto create(const window::Properties& props) -> bool override;
		auto show() -> void override;
		auto hide() -> void override;
		[[nodiscard]] auto is_visible() const -> bool override;
		auto poll_events(float delta_time) -> void override;

		[[nodiscard]] auto get_dpi_factor() const noexcept -> float override;
		[[nodiscard]] auto get_content_scale_factor() const noexcept -> float override;

		[[nodiscard]] auto get_native_handle() -> void* override;

		auto set_title(std::string_view title) -> void override;

		auto get_handle() -> GLFWwindow* {
			return handle_;
		}

		auto get_handle() const -> GLFWwindow const* {
			return handle_;
		}

		auto get_context() -> IPlatformContext* {
			return platform_context_;
		}

		auto get_context() const -> IPlatformContext const* {
			return platform_context_;
		}
	private:
		static auto window_close_callback(GLFWwindow* window) -> void;
		static auto window_size_callback(GLFWwindow* window, int width, int height) -> void;
		static auto window_focus_callback(GLFWwindow* window, int focused) -> void;

		GLFWwindow* handle_{ nullptr };
		IPlatformContext* platform_context_;
	};
}