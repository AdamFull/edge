#include "platform.h"

#include <GLFW/glfw3.h>

namespace edge::platform {
	static int32_t g_glfw_context_init_counter{ 0 };

	inline auto translate_key_code(int key) -> KeyboardKeyCode {

	}

	auto glfw_error_callback(int error, const char* description) -> void {
		//EDGE_LOGE("[GLFW Window] (code {}): {}", error, description);
	}

	void DesktopPlatformWindow::window_close_callback(GLFWwindow* window) {
		if (auto* platform_context = (PlatformContextInterface*)(glfwGetWindowUserPointer(window))) {
			auto& dispatcher = platform_context->get_event_dispatcher();
			dispatcher.emit(events::WindowShouldCloseEvent{ 
				.window_id = (uint64_t)window 
				});
		}
		glfwSetWindowShouldClose(window, GLFW_TRUE);
	}

	void DesktopPlatformWindow::window_size_callback(GLFWwindow* window, int width, int height) {
		if (auto* platform_context = (PlatformContextInterface*)(glfwGetWindowUserPointer(window))) {
			auto& dispatcher = platform_context->get_event_dispatcher();
			dispatcher.emit(events::WindowSizeChangedEvent{ 
				.width = width, 
				.height = height, 
				.window_id = (uint64_t)window 
				});
		}
	}

	void DesktopPlatformWindow::window_focus_callback(GLFWwindow* window, int focused) {
		if (auto* platform_context = (PlatformContextInterface*)(glfwGetWindowUserPointer(window))) {
			auto& dispatcher = platform_context->get_event_dispatcher();
			dispatcher.emit(events::WindowFocusChangedEvent{ 
				.focused = focused == 1, 
				.window_id = (uint64_t)window
				});
		}
	}

	void DesktopPlatformWindow::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
		if (auto* platform_context = (PlatformContextInterface*)(glfwGetWindowUserPointer(window))) {
			auto& dispatcher = platform_context->get_event_dispatcher();
			dispatcher.emit(events::KeyEvent{ 
				.key_code = key,
				.window_id = (uint64_t)window 
				});
		}
	}

	void DesktopPlatformWindow::cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
	}

	void DesktopPlatformWindow::mouse_button_callback(GLFWwindow* window, int button, int action, int /*mods*/) {
	}

	void DesktopPlatformWindow::mouse_scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
	}

	void DesktopPlatformWindow::character_input_callback(GLFWwindow* window, uint32_t codepoint) {
	}

	DesktopPlatformWindow::DesktopPlatformWindow() {
		// NOTE: In case where we can have multiple windows, we need to init glfw context only one tile
		if (g_glfw_context_init_counter <= 0) {
			if (!glfwInit()) {
				// TODO: add error logging
				return;
			}

			glfwSetErrorCallback(glfw_error_callback);
			glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		}
		g_glfw_context_init_counter++;
	}

	DesktopPlatformWindow::~DesktopPlatformWindow() {
		if (--g_glfw_context_init_counter <= 0) {
			glfwTerminate();
		}
	}

	auto DesktopPlatformWindow::construct(const window::CreateInfo& create_info) -> std::unique_ptr<DesktopPlatformWindow> {
		auto window = std::make_unique<DesktopPlatformWindow>();
		window->properties_ = create_info.props;
		window->platform_context_ = create_info.platform_context_;
		return window;
	}

	auto DesktopPlatformWindow::create() -> bool {
		switch (properties_.mode) {
		case window::Mode::eFullscreen: {
			auto* monitor = glfwGetPrimaryMonitor();
			const auto* mode = glfwGetVideoMode(monitor);
			handle_ = glfwCreateWindow(mode->width, mode->height, properties_.title.c_str(), monitor, NULL);
			break;
		}

		case window::Mode::eFullscreenBorderless: {
			auto* monitor = glfwGetPrimaryMonitor();
			const auto* mode = glfwGetVideoMode(monitor);
			glfwWindowHint(GLFW_RED_BITS, mode->redBits);
			glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
			glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
			glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
			handle_ = glfwCreateWindow(mode->width, mode->height, properties_.title.c_str(), monitor, NULL);
			break;
		}

		case window::Mode::eFullscreenStretch: {
			//EDGE_LOGE("[GLFW Window] Cannot support stretch mode on this platform.");
			break;
		}

		default:
			handle_ = glfwCreateWindow(properties_.extent.width, properties_.extent.height, properties_.title.c_str(), NULL, NULL);
			break;
		}

		if (!handle_) {
			//EDGE_LOGE("[GLFW Window] Couldn't create glfw window.");
			return false;
		}

		glfwSetWindowCloseCallback(handle_, &DesktopPlatformWindow::window_close_callback);
		glfwSetWindowSizeCallback(handle_, &DesktopPlatformWindow::window_size_callback);
		glfwSetWindowFocusCallback(handle_, &DesktopPlatformWindow::window_focus_callback);
		glfwSetKeyCallback(handle_, &DesktopPlatformWindow::key_callback);
		glfwSetCursorPosCallback(handle_, &DesktopPlatformWindow::cursor_position_callback);
		glfwSetMouseButtonCallback(handle_, &DesktopPlatformWindow::mouse_button_callback);
		glfwSetScrollCallback(handle_, &DesktopPlatformWindow::mouse_scroll_callback);
		glfwSetCharCallback(handle_, &DesktopPlatformWindow::character_input_callback);

		glfwSetWindowUserPointer(handle_, platform_context_);

		glfwSetInputMode(handle_, GLFW_STICKY_KEYS, 1);
		glfwSetInputMode(handle_, GLFW_STICKY_MOUSE_BUTTONS, 1);

		return true;
	}

	auto DesktopPlatformWindow::destroy() -> void {
		if (handle_) {
			glfwSetWindowShouldClose(handle_, GLFW_TRUE);
			glfwDestroyWindow(handle_);
			handle_ = nullptr;
		}
	}

	auto DesktopPlatformWindow::show() -> void {
		if (!handle_) {
			return;
		}

		glfwShowWindow(handle_);
	}

	auto DesktopPlatformWindow::hide() -> void {
		if (!handle_) {
			return;
		}

		glfwHideWindow(handle_);
	}

	auto DesktopPlatformWindow::is_visible() const -> bool {
		return false;
	}

	auto DesktopPlatformWindow::poll_events() -> void {
		// TODO: Handle here to poll only when glfw is initialized
		glfwPollEvents();

		requested_close_ = glfwWindowShouldClose(handle_);
	}

	auto DesktopPlatformWindow::get_dpi_factor() const noexcept -> float {
		// TODO: select current active monitor
		auto primary_monitor = glfwGetPrimaryMonitor();
		auto vidmode = glfwGetVideoMode(primary_monitor);

		int width_mm, height_mm;
		glfwGetMonitorPhysicalSize(primary_monitor, &width_mm, &height_mm);

		// As suggested by the GLFW monitor guide
		constexpr const float inch_to_mm{ 25.0f };
		constexpr const float win_base_density{ 96.0f };

		auto dpi = static_cast<uint32_t>(vidmode->width / (width_mm / inch_to_mm));
		return dpi / win_base_density;
	}

	auto DesktopPlatformWindow::get_content_scale_factor() const noexcept -> float {
		if (!handle_) {
			return 1.0f;
		}

		int fb_width, fb_height;
		glfwGetFramebufferSize(handle_, &fb_width, &fb_height);
		int win_width, win_height;
		glfwGetWindowSize(handle_, &win_width, &win_height);

		// Needed for ui scale
		return static_cast<float>(fb_width) / win_width;
	}

	auto DesktopPlatformWindow::set_title(std::string_view title) -> void {
		if (title.compare(properties_.title.c_str()) != 0) {
			glfwSetWindowTitle(handle_, title.data());
			properties_.title = title;
		}
	}
}