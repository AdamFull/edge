#pragma once

#include "platform.h"

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
}