#include "platform.h"

namespace edge::platform {
	bool WindowsPlatformWindow::class_registered_ = false;

	auto WindowsPlatformWindow::construct(const window::Properties& properties) -> std::unique_ptr<WindowsPlatformWindow> {
		auto window = std::make_unique<WindowsPlatformWindow>();
		window->_construct(properties);
		return window;
	}

	auto WindowsPlatformWindow::_construct(const window::Properties& properties) -> bool {
		properties_ = properties;
		return true;
	}

	auto WindowsPlatformWindow::create() -> bool {
		// Register window class if not already registered
		if (!class_registered_) {
			WNDCLASSEXW wc = {};
			wc.cbSize = sizeof(WNDCLASSEXW);
			wc.style = CS_HREDRAW | CS_VREDRAW;
			wc.lpfnWndProc = _window_proc;
			wc.cbClsExtra = 0;
			wc.cbWndExtra = 0;
			wc.hInstance = GetModuleHandleW(nullptr);
			wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
			wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
			wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
			wc.lpszMenuName = nullptr;
			wc.lpszClassName = L"EdgeWindowClass";
			wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

			if (!RegisterClassExW(&wc)) {
				return false;
			}
			class_registered_ = true;
		}

		// Calculate window size including borders
		RECT rect = { 0, 0, (LONG)properties_.extent.width, (LONG)properties_.extent.height };
		AdjustWindowRectEx(&rect, WS_OVERLAPPEDWINDOW, FALSE, 0);

		int window_width = rect.right - rect.left;
		int window_height = rect.bottom - rect.top;

		// Convert title to wide string
		int wide_length = MultiByteToWideChar(CP_UTF8, 0, properties_.title.c_str(), -1, nullptr, 0);
		std::wstring wide_title(wide_length - 1, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, properties_.title.c_str(), -1, wide_title.data(), wide_length);

		// Create window
		hwnd_ = CreateWindowExW(
			0,                              // Optional window styles
			L"EdgeWindowClass",             // Window class
			wide_title.c_str(),             // Window text
			WS_OVERLAPPEDWINDOW,            // Window style
			CW_USEDEFAULT, CW_USEDEFAULT,   // Size and position
			window_width, window_height,    // Width and height
			nullptr,                        // Parent window    
			nullptr,                        // Menu
			GetModuleHandleW(nullptr),      // Instance handle
			this                            // Additional application data
		);

		if (!hwnd_) {
			return false;
		}

		return true;
	}

	auto WindowsPlatformWindow::destroy() -> void {
		if (hwnd_) {
			DestroyWindow(hwnd_);
			hwnd_ = nullptr;
			is_visible_ = false;
		}
	}

	auto WindowsPlatformWindow::show() -> void {
		if (hwnd_) {
			ShowWindow(hwnd_, SW_SHOW);
			UpdateWindow(hwnd_);
			is_visible_ = true;
		}
	}

	auto WindowsPlatformWindow::hide() -> void {
		if (hwnd_) {
			ShowWindow(hwnd_, SW_HIDE);
			is_visible_ = false;
		}
	}

	auto WindowsPlatformWindow::is_visible() const -> bool {
		return is_visible_ && hwnd_ && IsWindowVisible(hwnd_);
	}

	auto WindowsPlatformWindow::get_width() const -> uint32_t {
		if (hwnd_) {
			RECT rect;
			GetClientRect(hwnd_, &rect);
			return rect.right - rect.left;
		}
		return properties_.extent.width;
	}

	auto WindowsPlatformWindow::get_height() const -> uint32_t {
		if (hwnd_) {
			RECT rect;
			GetClientRect(hwnd_, &rect);
			return rect.bottom - rect.top;
		}
		return properties_.extent.height;
	}

	auto WindowsPlatformWindow::get_extent() const->window::Extent {
		return properties_.extent;
	}

	auto WindowsPlatformWindow::set_title(std::string_view title) -> void {
		properties_.title = title;
		if (hwnd_) {
			// Convert title to wide string
			int wide_length = MultiByteToWideChar(CP_UTF8, 0, properties_.title.c_str(), -1, nullptr, 0);
			std::wstring wide_title(wide_length - 1, L'\0');
			MultiByteToWideChar(CP_UTF8, 0, properties_.title.c_str(), -1, wide_title.data(), wide_length);

			SetWindowTextW(hwnd_, wide_title.c_str());
		}
	}

	auto WindowsPlatformWindow::get_title() const -> std::string_view {
		return properties_.title;
	}

	auto WindowsPlatformWindow::get_hwnd() -> HWND {
		return hwnd_;
	}

	auto WindowsPlatformWindow::get_hwnd() const -> const HWND {
		return hwnd_;
	}

	auto CALLBACK WindowsPlatformWindow::_window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) -> LRESULT {
		WindowsPlatformWindow* window = nullptr;

		if (uMsg == WM_NCCREATE) {
			CREATESTRUCTW* pCreate = reinterpret_cast<CREATESTRUCTW*>(lParam);
			window = reinterpret_cast<WindowsPlatformWindow*>(pCreate->lpCreateParams);
			SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
		}
		else {
			window = reinterpret_cast<WindowsPlatformWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
		}

		if (window) {
			switch (uMsg) {
			case WM_CLOSE:
				window->hide();
				return 0;

			case WM_SIZE:
				// Update cached dimensions when window is resized
				if (window->hwnd_) {
					RECT rect;
					GetClientRect(window->hwnd_, &rect);
					window->properties_.extent.width = rect.right - rect.left;
					window->properties_.extent.height = rect.bottom - rect.top;
				}
				break;

			case WM_SHOWWINDOW:
				window->is_visible_ = wParam != 0;
				break;
			}
		}

		return DefWindowProcW(hwnd, uMsg, wParam, lParam);
	}
}