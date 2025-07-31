#include "platform.h"

#include <unordered_map>

#include <GLFW/glfw3.h>

namespace edge::platform {
	static int32_t g_glfw_context_init_counter{ 0 };
	PlatformContextInterface* DesktopPlatformWindow::platform_context_{ nullptr };
	static std::array<GLFWgamepadstate, GLFW_JOYSTICK_LAST + 1> gamepad_last_state_{};

	inline constexpr auto trnaslate_key_action(int action) -> KeyAction {
		if (action == GLFW_PRESS)
			return KeyAction::ePress;
		else if (action == GLFW_RELEASE)
			return KeyAction::eRelease;
		else if (action == GLFW_REPEAT)
			return KeyAction::eHold;

		return KeyAction::eUnknown;
	}

	inline auto translate_keyboard_key_code(int key) -> KeyboardKeyCode {
		static auto lut = std::unordered_map<int32_t, KeyboardKeyCode>({
			{ GLFW_KEY_SPACE, KeyboardKeyCode::eSpace },
			{ GLFW_KEY_APOSTROPHE, KeyboardKeyCode::eApostrophe },
			{ GLFW_KEY_COMMA, KeyboardKeyCode::eComma },
			{ GLFW_KEY_MINUS, KeyboardKeyCode::eMinus },
			{ GLFW_KEY_PERIOD, KeyboardKeyCode::ePeriod },
			{ GLFW_KEY_SLASH, KeyboardKeyCode::eSlash },
			{ GLFW_KEY_0, KeyboardKeyCode::e0 },
			{ GLFW_KEY_1, KeyboardKeyCode::e1 },
			{ GLFW_KEY_2, KeyboardKeyCode::e2 },
			{ GLFW_KEY_3, KeyboardKeyCode::e3 },
			{ GLFW_KEY_4, KeyboardKeyCode::e4 },
			{ GLFW_KEY_5, KeyboardKeyCode::e5 },
			{ GLFW_KEY_6, KeyboardKeyCode::e6 },
			{ GLFW_KEY_7, KeyboardKeyCode::e7 },
			{ GLFW_KEY_8, KeyboardKeyCode::e8 },
			{ GLFW_KEY_9, KeyboardKeyCode::e9 },
			{ GLFW_KEY_SEMICOLON, KeyboardKeyCode::eSemicolon },
			{ GLFW_KEY_EQUAL, KeyboardKeyCode::eEq },
			{ GLFW_KEY_A, KeyboardKeyCode::eA },
			{ GLFW_KEY_B, KeyboardKeyCode::eB },
			{ GLFW_KEY_C, KeyboardKeyCode::eC },
			{ GLFW_KEY_D, KeyboardKeyCode::eD },
			{ GLFW_KEY_E, KeyboardKeyCode::eE },
			{ GLFW_KEY_F, KeyboardKeyCode::eF },
			{ GLFW_KEY_G, KeyboardKeyCode::eG },
			{ GLFW_KEY_H, KeyboardKeyCode::eH },
			{ GLFW_KEY_I, KeyboardKeyCode::eI },
			{ GLFW_KEY_J, KeyboardKeyCode::eJ },
			{ GLFW_KEY_K, KeyboardKeyCode::eK },
			{ GLFW_KEY_L, KeyboardKeyCode::eL },
			{ GLFW_KEY_M, KeyboardKeyCode::eM },
			{ GLFW_KEY_N, KeyboardKeyCode::eN },
			{ GLFW_KEY_O, KeyboardKeyCode::eO },
			{ GLFW_KEY_P, KeyboardKeyCode::eP },
			{ GLFW_KEY_Q, KeyboardKeyCode::eQ },
			{ GLFW_KEY_R, KeyboardKeyCode::eR },
			{ GLFW_KEY_S, KeyboardKeyCode::eS },
			{ GLFW_KEY_T, KeyboardKeyCode::eT },
			{ GLFW_KEY_U, KeyboardKeyCode::eU },
			{ GLFW_KEY_V, KeyboardKeyCode::eV },
			{ GLFW_KEY_W, KeyboardKeyCode::eW },
			{ GLFW_KEY_X, KeyboardKeyCode::eX },
			{ GLFW_KEY_Y, KeyboardKeyCode::eY },
			{ GLFW_KEY_Z, KeyboardKeyCode::eZ },
			{ GLFW_KEY_LEFT_BRACKET, KeyboardKeyCode::eLeftBracket },
			{ GLFW_KEY_BACKSLASH, KeyboardKeyCode::eBackslash },
			{ GLFW_KEY_RIGHT_BRACKET, KeyboardKeyCode::eRightBracket },
			{ GLFW_KEY_GRAVE_ACCENT, KeyboardKeyCode::eGraveAccent },
			{ GLFW_KEY_WORLD_1, KeyboardKeyCode::eWorld1 },
			{ GLFW_KEY_WORLD_2, KeyboardKeyCode::eWorld2 },
			{ GLFW_KEY_ESCAPE, KeyboardKeyCode::eEsc },
			{ GLFW_KEY_ENTER, KeyboardKeyCode::eEnter },
			{ GLFW_KEY_TAB, KeyboardKeyCode::eTab },
			{ GLFW_KEY_BACKSPACE, KeyboardKeyCode::eBackspace },
			{ GLFW_KEY_INSERT, KeyboardKeyCode::eInsert },
			{ GLFW_KEY_DELETE, KeyboardKeyCode::eDel },
			{ GLFW_KEY_RIGHT, KeyboardKeyCode::eRight },
			{ GLFW_KEY_LEFT, KeyboardKeyCode::eLeft },
			{ GLFW_KEY_DOWN, KeyboardKeyCode::eDown },
			{ GLFW_KEY_UP, KeyboardKeyCode::eUp },
			{ GLFW_KEY_PAGE_UP, KeyboardKeyCode::ePageUp },
			{ GLFW_KEY_PAGE_DOWN, KeyboardKeyCode::ePageDown },
			{ GLFW_KEY_HOME, KeyboardKeyCode::eHome },
			{ GLFW_KEY_END, KeyboardKeyCode::eEnd },
			{ GLFW_KEY_CAPS_LOCK, KeyboardKeyCode::eCapsLock },
			{ GLFW_KEY_SCROLL_LOCK, KeyboardKeyCode::eScrollLock },
			{ GLFW_KEY_NUM_LOCK, KeyboardKeyCode::eNumLock },
			{ GLFW_KEY_PRINT_SCREEN, KeyboardKeyCode::ePrintScreen },
			{ GLFW_KEY_PAUSE, KeyboardKeyCode::ePause },
			{ GLFW_KEY_F1, KeyboardKeyCode::eF1 },
			{ GLFW_KEY_F2, KeyboardKeyCode::eF2 },
			{ GLFW_KEY_F3, KeyboardKeyCode::eF3 },
			{ GLFW_KEY_F4, KeyboardKeyCode::eF4 },
			{ GLFW_KEY_F5, KeyboardKeyCode::eF5 },
			{ GLFW_KEY_F6, KeyboardKeyCode::eF6 },
			{ GLFW_KEY_F7, KeyboardKeyCode::eF7 },
			{ GLFW_KEY_F8, KeyboardKeyCode::eF8 },
			{ GLFW_KEY_F9, KeyboardKeyCode::eF9 },
			{ GLFW_KEY_F10, KeyboardKeyCode::eF10 },
			{ GLFW_KEY_F11, KeyboardKeyCode::eF11 },
			{ GLFW_KEY_F12, KeyboardKeyCode::eF12 },
			{ GLFW_KEY_F13, KeyboardKeyCode::eF13 },
			{ GLFW_KEY_F14, KeyboardKeyCode::eF14 },
			{ GLFW_KEY_F15, KeyboardKeyCode::eF15 },
			{ GLFW_KEY_F16, KeyboardKeyCode::eF16 },
			{ GLFW_KEY_F17, KeyboardKeyCode::eF17 },
			{ GLFW_KEY_F18, KeyboardKeyCode::eF18 },
			{ GLFW_KEY_F19, KeyboardKeyCode::eF19 },
			{ GLFW_KEY_F20, KeyboardKeyCode::eF20 },
			{ GLFW_KEY_F21, KeyboardKeyCode::eF21 },
			{ GLFW_KEY_F22, KeyboardKeyCode::eF22 },
			{ GLFW_KEY_F23, KeyboardKeyCode::eF23 },
			{ GLFW_KEY_F24, KeyboardKeyCode::eF24 },
			{ GLFW_KEY_F25, KeyboardKeyCode::eF25 },
			{ GLFW_KEY_KP_0, KeyboardKeyCode::eKp0 },
			{ GLFW_KEY_KP_1, KeyboardKeyCode::eKp1 },
			{ GLFW_KEY_KP_2, KeyboardKeyCode::eKp2 },
			{ GLFW_KEY_KP_3, KeyboardKeyCode::eKp3 },
			{ GLFW_KEY_KP_4, KeyboardKeyCode::eKp4 },
			{ GLFW_KEY_KP_5, KeyboardKeyCode::eKp5 },
			{ GLFW_KEY_KP_6, KeyboardKeyCode::eKp6 },
			{ GLFW_KEY_KP_7, KeyboardKeyCode::eKp7 },
			{ GLFW_KEY_KP_8, KeyboardKeyCode::eKp8 },
			{ GLFW_KEY_KP_9, KeyboardKeyCode::eKp9 },
			{ GLFW_KEY_KP_DECIMAL, KeyboardKeyCode::eKpDec },
			{ GLFW_KEY_KP_DIVIDE, KeyboardKeyCode::eKpDiv },
			{ GLFW_KEY_KP_MULTIPLY, KeyboardKeyCode::eKpMul },
			{ GLFW_KEY_KP_SUBTRACT, KeyboardKeyCode::eKpSub },
			{ GLFW_KEY_KP_ADD, KeyboardKeyCode::eKpAdd },
			{ GLFW_KEY_KP_ENTER, KeyboardKeyCode::eKpEnter },
			{ GLFW_KEY_KP_EQUAL, KeyboardKeyCode::eKpEq },
			{ GLFW_KEY_LEFT_SHIFT, KeyboardKeyCode::eLeftShift },
			{ GLFW_KEY_LEFT_CONTROL, KeyboardKeyCode::eLeftControl },
			{ GLFW_KEY_LEFT_ALT, KeyboardKeyCode::eLeftAlt },
			{ GLFW_KEY_LEFT_SUPER, KeyboardKeyCode::eLeftSuper },
			{ GLFW_KEY_RIGHT_SHIFT, KeyboardKeyCode::eRightShift },
			{ GLFW_KEY_RIGHT_CONTROL, KeyboardKeyCode::eRightControl },
			{ GLFW_KEY_RIGHT_ALT, KeyboardKeyCode::eRightAlt },
			{ GLFW_KEY_RIGHT_SUPER, KeyboardKeyCode::eRightSuper },
			{ GLFW_KEY_MENU, KeyboardKeyCode::eMenu }
			});

			if (auto found = lut.find(key); found != lut.end()) {
				return found->second;
			}

			return KeyboardKeyCode::eUnknown;
	}

	inline auto translate_mouse_key_code(int key) -> MouseKeyCode {
		static auto lut = std::unordered_map<int32_t, MouseKeyCode>({
				{ GLFW_MOUSE_BUTTON_1, MouseKeyCode::eButton1 },
				{ GLFW_MOUSE_BUTTON_2, MouseKeyCode::eButton2 },
				{ GLFW_MOUSE_BUTTON_3, MouseKeyCode::eButton3 },
				{ GLFW_MOUSE_BUTTON_4, MouseKeyCode::eButton4 },
				{ GLFW_MOUSE_BUTTON_5, MouseKeyCode::eButton5 },
				{ GLFW_MOUSE_BUTTON_6, MouseKeyCode::eButton6 },
				{ GLFW_MOUSE_BUTTON_7, MouseKeyCode::eButton7 },
				{ GLFW_MOUSE_BUTTON_8, MouseKeyCode::eButton8 }
			});

		if (auto found = lut.find(key); found != lut.end()) {
			return found->second;
		}

		return MouseKeyCode::eUnknown;
	}

	inline auto translate_gamepad_key_code(int key) -> GamepadKeyCode {
		static auto lut = std::unordered_map<int32_t, GamepadKeyCode>({
				{ GLFW_GAMEPAD_BUTTON_A, GamepadKeyCode::eButtonA },
				{ GLFW_GAMEPAD_BUTTON_B, GamepadKeyCode::eButtonB },
				{ GLFW_GAMEPAD_BUTTON_X, GamepadKeyCode::eButtonX },
				{ GLFW_GAMEPAD_BUTTON_Y, GamepadKeyCode::eButtonY },
				{ GLFW_GAMEPAD_BUTTON_LEFT_BUMPER, GamepadKeyCode::eButtonLeftBumper },
				{ GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER, GamepadKeyCode::eButtonRightBumper },
				{ GLFW_GAMEPAD_BUTTON_BACK, GamepadKeyCode::eButtonBack },
				{ GLFW_GAMEPAD_BUTTON_START, GamepadKeyCode::eButtonStart },
				{ GLFW_GAMEPAD_BUTTON_GUIDE, GamepadKeyCode::eButtonGuide },
				{ GLFW_GAMEPAD_BUTTON_LEFT_THUMB, GamepadKeyCode::eButtonLeftThumb },
				{ GLFW_GAMEPAD_BUTTON_RIGHT_THUMB, GamepadKeyCode::eButtonRightThumb },
				{ GLFW_GAMEPAD_BUTTON_DPAD_UP, GamepadKeyCode::eButtonDPadUp },
				{ GLFW_GAMEPAD_BUTTON_DPAD_RIGHT, GamepadKeyCode::eButtonDPadRight },
				{ GLFW_GAMEPAD_BUTTON_DPAD_DOWN, GamepadKeyCode::eButtonDPadDown },
				{ GLFW_GAMEPAD_BUTTON_DPAD_LEFT, GamepadKeyCode::eButtonDPadLeft }
			});

		if (auto found = lut.find(key); found != lut.end()) {
			return found->second;
		}

		return GamepadKeyCode::eUnknown;
	}

	inline auto translate_gamepad_axis_code(int key) -> GamepadAxisCode {
		static auto lut = std::unordered_map<int32_t, GamepadAxisCode>({
				{ GLFW_GAMEPAD_AXIS_LEFT_X, GamepadAxisCode::eLeftX },
				{ GLFW_GAMEPAD_AXIS_LEFT_Y, GamepadAxisCode::eLeftY },
				{ GLFW_GAMEPAD_AXIS_RIGHT_X, GamepadAxisCode::eRightX },
				{ GLFW_GAMEPAD_AXIS_RIGHT_Y, GamepadAxisCode::eRightY },
				{ GLFW_GAMEPAD_AXIS_LEFT_TRIGGER, GamepadAxisCode::eLeftTrigger },
				{ GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER, GamepadAxisCode::eRightTrigger }
			});

		if (auto found = lut.find(key); found != lut.end()) {
			return found->second;
		}

		return GamepadAxisCode::eUnknown;
	}

	auto glfw_error_callback(int error, const char* description) -> void {
		//EDGE_LOGE("[GLFW Window] (code {}): {}", error, description);
	}

	auto DesktopPlatformWindow::window_close_callback(GLFWwindow* window) -> void {
		if (platform_context_) {
			auto& dispatcher = platform_context_->get_event_dispatcher();
			dispatcher.emit(events::WindowShouldCloseEvent{ 
				.window_id = (uint64_t)window 
				});
		}
		glfwSetWindowShouldClose(window, GLFW_TRUE);
	}

	auto DesktopPlatformWindow::window_size_callback(GLFWwindow* window, int width, int height) -> void {
		if (platform_context_) {
			auto& dispatcher = platform_context_->get_event_dispatcher();
			dispatcher.emit(events::WindowSizeChangedEvent{ 
				.width = width, 
				.height = height, 
				.window_id = (uint64_t)window 
				});
		}
	}

	auto DesktopPlatformWindow::window_focus_callback(GLFWwindow* window, int focused) -> void {
		if (platform_context_) {
			auto& dispatcher = platform_context_->get_event_dispatcher();
			dispatcher.emit(events::WindowFocusChangedEvent{ 
				.focused = focused == 1, 
				.window_id = (uint64_t)window
				});
		}
	}

	auto DesktopPlatformWindow::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) -> void {
		if (platform_context_) {
			auto& dispatcher = platform_context_->get_event_dispatcher();
			dispatcher.emit(events::KeyEvent{ 
				.key_code = translate_keyboard_key_code(key),
				.key_action = trnaslate_key_action(action),
				.window_id = (uint64_t)window 
				});
		}
	}

	auto DesktopPlatformWindow::cursor_position_callback(GLFWwindow* window, double xpos, double ypos) -> void {
		if (platform_context_) {
			auto& dispatcher = platform_context_->get_event_dispatcher();
			dispatcher.emit(events::MousePositionEvent{
				.x = xpos,
				.y = ypos,
				.window_id = (uint64_t)window
				});
		}
	}

	auto DesktopPlatformWindow::mouse_button_callback(GLFWwindow* window, int button, int action, int /*mods*/) -> void {
		if (platform_context_) {
			auto& dispatcher = platform_context_->get_event_dispatcher();
			dispatcher.emit(events::MouseKeyEvent{
				.key_code = translate_mouse_key_code(button),
				.key_action = trnaslate_key_action(action),
				.window_id = (uint64_t)window
				});
		}
	}

	auto DesktopPlatformWindow::mouse_scroll_callback(GLFWwindow* window, double xoffset, double yoffset) -> void {
		if (platform_context_) {
			auto& dispatcher = platform_context_->get_event_dispatcher();
			dispatcher.emit(events::MouseScrollEvent{
				.offset_x = xoffset,
				.offset_y = yoffset,
				.window_id = (uint64_t)window
				});
		}
	}

	void DesktopPlatformWindow::character_input_callback(GLFWwindow* window, uint32_t codepoint) {
		if (platform_context_) {
			auto& dispatcher = platform_context_->get_event_dispatcher();
			dispatcher.emit(events::CharacterInputEvent{
				.charcode = codepoint,
				.window_id = (uint64_t)window
				});
		}
	}

	auto DesktopPlatformWindow::gamepad_connected_callback(int jid, int event) -> void {
		if (platform_context_) {
			auto& dispatcher = platform_context_->get_event_dispatcher();
			dispatcher.emit(events::GamepadConnectionEvent{
				.gamepad_id = jid,
				.connected = event == GLFW_CONNECTED,
				.name = glfwGetJoystickName(jid)
				});
		}

		// Try to save initial gamepad state
		if (!glfwGetGamepadState(jid, gamepad_last_state_.data() + jid)) {
			// TODO: Send error here
		}
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

	auto DesktopPlatformWindow::construct(PlatformContextInterface* platform_context) -> std::unique_ptr<DesktopPlatformWindow> {
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
		glfwSetJoystickCallback(&DesktopPlatformWindow::gamepad_connected_callback);

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

		auto& dispatcher = platform_context_->get_event_dispatcher();

		// Process gamepad inputs
		for (int32_t jid = 0; jid < GLFW_JOYSTICK_LAST + 1; ++jid) {
			GLFWgamepadstate state;
			if (glfwGetGamepadState(jid, &state)) {
				auto& last_state = gamepad_last_state_[jid];

				// Process buttons
				for (int32_t btn = 0; btn < GLFW_GAMEPAD_BUTTON_LAST + 1; ++btn) {
					bool curr = state.buttons[btn];
					bool prev = last_state.buttons[btn];

					KeyAction key_action = KeyAction::eUnknown;
					if (curr && !prev) {
						key_action = KeyAction::ePress;
					}
					else if (curr && prev) {
						key_action = KeyAction::eHold;
					}
					else if (!curr && prev) {
						key_action = KeyAction::eRelease;
					}

					if (key_action != KeyAction::eUnknown) {
						dispatcher.defer(events::GamepadButtonEvent{
							.gamepad_id = jid,
							.key_code = translate_gamepad_key_code(btn),
							.key_action = key_action
							});
					}
				}
				
				// Process axis
				for (int32_t axis = 0; axis < GLFW_GAMEPAD_AXIS_LAST + 1; ++axis) {
					float curr = state.axes[axis];
					float prev = last_state.axes[axis];

					dispatcher.defer(events::GamepadAxisEvent{
							.gamepad_id = jid,
							.value = curr,
							.axis_code = translate_gamepad_axis_code(axis)
						});
				}

				// Update gamepad state
				last_state = state;
			}
		}

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