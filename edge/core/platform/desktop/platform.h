#pragma once

#include "../platform.h"

struct GLFWwindow;

namespace edge::platform {
	class DesktopPlatformInput;
	class DesktopPlatformWindow;

	class DesktopPlatformInput final : public IPlatformInput {
	public:
		~DesktopPlatformInput() override;
		static auto construct(DesktopPlatformWindow* window) -> std::unique_ptr<DesktopPlatformInput>;

		auto create() -> bool override;

		auto update(float delta_time) -> void override;

		auto begin_text_input_capture(std::string_view initial_text = {}) -> bool override;
		auto end_text_input_capture() -> void override;

		auto set_gamepad_color(int32_t gamepad_id, uint32_t color) -> bool override;
	private:
		static auto key_callback(GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/) -> void;
		static auto cursor_position_callback(GLFWwindow* window, double xpos, double ypos) -> void;
		static auto mouse_button_callback(GLFWwindow* window, int button, int action, int /*mods*/) -> void;
		static auto mouse_scroll_callback(GLFWwindow* window, double xoffset, double yoffset) -> void;
		static auto character_input_callback(GLFWwindow* window, uint32_t codepoint) -> void;
		static auto gamepad_connected_callback(int jid, int event) -> void;

		DesktopPlatformWindow* platform_window_{ nullptr };
		static IPlatformContext* platform_context_;
	};

	class DesktopPlatformWindow final : public IPlatformWindow {
	public:
		DesktopPlatformWindow();
		~DesktopPlatformWindow() override;
		static auto construct(IPlatformContext* platform_context) -> std::unique_ptr<DesktopPlatformWindow>;

		[[nodiscard]] auto create(const window::Properties& props) -> bool override;
		auto show() -> void override;
		auto hide() -> void override;
		[[nodiscard]] auto is_visible() const -> bool override;
		auto poll_events() -> void override;

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

	using Window = DesktopPlatformWindow;
}