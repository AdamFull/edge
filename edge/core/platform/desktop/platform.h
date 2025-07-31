#pragma once

#include "../platform.h"

struct GLFWwindow;

namespace edge::platform {
	class DesktopPlatformWindow final : public PlatformWindowInterface {
	public:
		DesktopPlatformWindow();
		~DesktopPlatformWindow() override;
		static auto construct(PlatformContextInterface* platform_context) -> std::unique_ptr<DesktopPlatformWindow>;

		[[nodiscard]] auto create(const window::Properties& props) -> bool override;
		auto destroy() -> void override;
		auto show() -> void override;
		auto hide() -> void override;
		[[nodiscard]] auto is_visible() const -> bool override;
		auto poll_events() -> void override;

		[[nodiscard]] auto get_dpi_factor() const noexcept -> float override;
		[[nodiscard]] auto get_content_scale_factor() const noexcept -> float override;

		auto set_title(std::string_view title) -> void override;
	private:
		static auto window_close_callback(GLFWwindow* window) -> void;
		static auto window_size_callback(GLFWwindow* window, int width, int height) -> void;
		static auto window_focus_callback(GLFWwindow* window, int focused) -> void;
		static auto key_callback(GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/) -> void;
		static auto cursor_position_callback(GLFWwindow* window, double xpos, double ypos) -> void;
		static auto mouse_button_callback(GLFWwindow* window, int button, int action, int /*mods*/) -> void;
		static auto mouse_scroll_callback(GLFWwindow* window, double xoffset, double yoffset) -> void;
		static auto character_input_callback(GLFWwindow* window, uint32_t codepoint) -> void;
		static auto gamepad_connected_callback(int jid, int event) -> void;

		GLFWwindow* handle_{ nullptr };
		static PlatformContextInterface* platform_context_;
	};

	using Window = DesktopPlatformWindow;
}