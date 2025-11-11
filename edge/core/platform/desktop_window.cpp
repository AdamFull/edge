#include "desktop_window.h"

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <vulkan/vulkan.hpp>

#define EDGE_LOGGER_SCOPE "platform::DesktopPlatformWindow"

namespace edge::platform {
	static int32_t g_glfw_context_init_counter{ 0 };

	auto glfw_error_callback(int error, const char* description) -> void {
		EDGE_SLOGE("Error: (code {}): {}", error, description);
	}

	auto DesktopPlatformWindow::window_close_callback(GLFWwindow* window) -> void {
		auto* platform_context = static_cast<IPlatformContext*>(glfwGetWindowUserPointer(window));
		if (!platform_context) {
			return;
		}

		auto& dispatcher = platform_context->get_event_dispatcher();
		dispatcher.emit(events::WindowShouldCloseEvent{
			.window_id = (uint64_t)window
			});
		glfwSetWindowShouldClose(window, GLFW_TRUE);
	}

	auto DesktopPlatformWindow::window_size_callback(GLFWwindow* window, int width, int height) -> void {
		auto* platform_context = static_cast<IPlatformContext*>(glfwGetWindowUserPointer(window));
		if (!platform_context) {
			return;
		}

		EDGE_SLOGD("Window[{}] size changed[{}, {}]", (uint64_t)window, width, height);

		auto& dispatcher = platform_context->get_event_dispatcher();
		dispatcher.emit(events::WindowSizeChangedEvent{
			.width = width,
			.height = height,
			.window_id = (uint64_t)window
			});
	}

	auto DesktopPlatformWindow::window_focus_callback(GLFWwindow* window, int focused) -> void {
		auto* platform_context = static_cast<IPlatformContext*>(glfwGetWindowUserPointer(window));
		if (!platform_context) {
			return;
		}

		EDGE_SLOGD("Window[{}] {}.", (uint64_t)window, focused == 1 ? "focused" : "unfocused");

		auto& dispatcher = platform_context->get_event_dispatcher();
		dispatcher.emit(events::WindowFocusChangedEvent{
			.focused = focused == 1,
			.window_id = (uint64_t)window
			});
	}

	DesktopPlatformWindow::DesktopPlatformWindow() {
		// NOTE: In case where we can have multiple windows, we need to init glfw context only one tile
		if (g_glfw_context_init_counter <= 0) {
			if (!glfwInit()) {
				EDGE_SLOGE("Failed to init glfw context.");
				return;
			}

			glfwSetErrorCallback(glfw_error_callback);
			glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		}
		g_glfw_context_init_counter++;
	}

	DesktopPlatformWindow::~DesktopPlatformWindow() {
		if (handle_) {
			glfwSetWindowShouldClose(handle_, GLFW_TRUE);
			glfwDestroyWindow(handle_);
			handle_ = nullptr;
		}

		if (--g_glfw_context_init_counter <= 0) {
			EDGE_SLOGD("GLFW context terminated.");
			glfwTerminate();
		}

		glfwSetErrorCallback(nullptr);
	}

	auto DesktopPlatformWindow::construct(IPlatformContext* platform_context) -> std::unique_ptr<DesktopPlatformWindow> {
		auto window = std::make_unique<DesktopPlatformWindow>();
		window->platform_context_ = platform_context;
		return window;
	}

	auto DesktopPlatformWindow::create(const window::Properties& props) -> bool {
		properties_ = props;

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
			EDGE_SLOGE("Cannot support stretch mode on this platform.");
			break;
		}

		default:
			handle_ = glfwCreateWindow(properties_.extent.width, properties_.extent.height, properties_.title.c_str(), NULL, NULL);
			break;
		}

		if (!handle_) {
			EDGE_SLOGE("Couldn't create glfw window.");
			return false;
		}

		glfwSetWindowCloseCallback(handle_, &DesktopPlatformWindow::window_close_callback);
		glfwSetWindowSizeCallback(handle_, &DesktopPlatformWindow::window_size_callback);
		glfwSetWindowFocusCallback(handle_, &DesktopPlatformWindow::window_focus_callback);

		glfwSetInputMode(handle_, GLFW_STICKY_KEYS, 1);
		glfwSetInputMode(handle_, GLFW_STICKY_MOUSE_BUTTONS, 1);

		glfwSetWindowUserPointer(handle_, platform_context_);

		return true;
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
		return true;
	}

	auto DesktopPlatformWindow::poll_events(float delta_time) -> void {
		if (!handle_) {
			return;
		}

		glfwPollEvents();
		requested_close_ = glfwWindowShouldClose(handle_);

		auto& input = platform_context_->get_input();
		input.update(delta_time);
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

	auto DesktopPlatformWindow::get_native_handle() -> void* {
#if EDGE_PLATFORM_WINDOWS
		vk::Win32SurfaceCreateInfoKHR surface_create_info{};
		surface_create_info.hwnd = glfwGetWin32Window(handle_);
		surface_create_info.hinstance = (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE);
#elif EDGE_PLATFORM_LINUX
		static vk::XlibSurfaceCreateInfoKHR surface_create_info{};
		surface_create_info.dpy = glfwGetX11Display();
		surface_create_info.window = glfwGetX11Window(handle_);
#endif
		return &surface_create_info;
	}
}

#undef EDGE_LOGGER_SCOPE