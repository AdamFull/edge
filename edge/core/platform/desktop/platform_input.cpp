#include "platform.h"

#include <spdlog/spdlog.h>
#include <GLFW/glfw3.h>

namespace edge::platform {
	IPlatformContext* DesktopPlatformInput::platform_context_{ nullptr };
	static bool gamepad_connection_states[4]{};

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

	auto DesktopPlatformInput::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) -> void {
		if (platform_context_) {
			auto& dispatcher = platform_context_->get_event_dispatcher();
			dispatcher.emit(events::KeyEvent{
				.key_code = translate_keyboard_key_code(key),
				.state = action == GLFW_PRESS,
				.window_id = (uint64_t)window
				});
		}
	}

	auto DesktopPlatformInput::cursor_position_callback(GLFWwindow* window, double xpos, double ypos) -> void {
		if (platform_context_) {
			auto& dispatcher = platform_context_->get_event_dispatcher();
			dispatcher.emit(events::MousePositionEvent{
				.x = xpos,
				.y = ypos,
				.window_id = (uint64_t)window
				});
		}
	}

	auto DesktopPlatformInput::mouse_button_callback(GLFWwindow* window, int button, int action, int /*mods*/) -> void {
		if (platform_context_) {
			auto& dispatcher = platform_context_->get_event_dispatcher();
			dispatcher.emit(events::MouseKeyEvent{
				.key_code = translate_mouse_key_code(button),
				.state = action == GLFW_PRESS,
				.window_id = (uint64_t)window
				});
		}
	}

	auto DesktopPlatformInput::mouse_scroll_callback(GLFWwindow* window, double xoffset, double yoffset) -> void {
		if (platform_context_) {
			auto& dispatcher = platform_context_->get_event_dispatcher();
			dispatcher.emit(events::MouseScrollEvent{
				.offset_x = xoffset,
				.offset_y = yoffset,
				.window_id = (uint64_t)window
				});
		}
	}

	void DesktopPlatformInput::character_input_callback(GLFWwindow* window, uint32_t codepoint) {
		if (platform_context_) {
			spdlog::debug("[Desktop Window]: Window[{}] character input: {}", (uint64_t)window, codepoint);

			auto& dispatcher = platform_context_->get_event_dispatcher();
			dispatcher.emit(events::CharacterInputEvent{
				.charcode = codepoint,
				.window_id = (uint64_t)window
				});
		}
	}

	auto DesktopPlatformInput::gamepad_connected_callback(int jid, int event) -> void {
		if (platform_context_) {
			spdlog::debug("[Desktop Window]: Gamepad[{}] {}.", jid, event == GLFW_CONNECTED ? "connected" : "disconnected");

			gamepad_connection_states[jid] = event == GLFW_CONNECTED;

			auto& dispatcher = platform_context_->get_event_dispatcher();
			dispatcher.emit(events::GamepadConnectionEvent{
				.gamepad_id = jid,
				.connected = event == GLFW_CONNECTED,
				.name = glfwGetJoystickName(jid)
				});
		}
	}

	DesktopPlatformInput::~DesktopPlatformInput() {
		glfwSetJoystickCallback(nullptr);
	}

	auto DesktopPlatformInput::construct(DesktopPlatformWindow* window) -> std::unique_ptr<DesktopPlatformInput> {
		auto self = std::make_unique<DesktopPlatformInput>();
		self->platform_window_ = window;
		DesktopPlatformInput::platform_context_ = window->get_context();
		return self;
	}

	auto DesktopPlatformInput::create() -> bool {
		auto* handle = platform_window_->get_handle();
		glfwSetKeyCallback(handle, &DesktopPlatformInput::key_callback);
		glfwSetCursorPosCallback(handle, &DesktopPlatformInput::cursor_position_callback);
		glfwSetMouseButtonCallback(handle, &DesktopPlatformInput::mouse_button_callback);
		glfwSetScrollCallback(handle, &DesktopPlatformInput::mouse_scroll_callback);
		glfwSetCharCallback(handle, &DesktopPlatformInput::character_input_callback);
		glfwSetJoystickCallback(&DesktopPlatformInput::gamepad_connected_callback);

		return true;
	}

	auto DesktopPlatformInput::update(float delta_time) -> void {
		auto& dispatcher = platform_context_->get_event_dispatcher();

		// Process gamepad inputs
		for (int32_t jid = 0; jid < GLFW_JOYSTICK_LAST + 1; ++jid) {
			GLFWgamepadstate state;
			if (glfwGetGamepadState(jid, &state)) {

				if (!gamepad_connection_states[jid]) {
					gamepad_connection_states[jid] = true;

					auto& dispatcher = platform_context_->get_event_dispatcher();
					dispatcher.emit(events::GamepadConnectionEvent{
						.gamepad_id = jid,
						.connected = true,
						.name = glfwGetJoystickName(jid)
						});
				}

				// Process buttons
				for (int32_t btn = 0; btn < GLFW_GAMEPAD_BUTTON_LAST + 1; ++btn) {
					dispatcher.emit(events::GamepadButtonEvent{
						.gamepad_id = jid,
						.key_code = translate_gamepad_key_code(btn),
						.state = state.buttons[btn] == 1
						});
				}

				// Process axis
				dispatcher.emit(events::GamepadAxisEvent{
						.gamepad_id = jid,
						.values = { state.axes[GLFW_GAMEPAD_AXIS_LEFT_X], state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y] },
						.axis_code = GamepadAxisCode::eLeftStick
					});

				
				dispatcher.emit(events::GamepadAxisEvent{
						.gamepad_id = jid,
						.values = { state.axes[GLFW_GAMEPAD_AXIS_RIGHT_X], state.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y] },
						.axis_code = GamepadAxisCode::eRightStick
					});

				dispatcher.emit(events::GamepadAxisEvent{
						.gamepad_id = jid,
						.values = { state.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER] },
						.axis_code = GamepadAxisCode::eLeftTrigger
					});

				dispatcher.emit(events::GamepadAxisEvent{
						.gamepad_id = jid,
						.values = { state.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] },
						.axis_code = GamepadAxisCode::eRightTrigger
					});
			}
		}
	}

	auto DesktopPlatformInput::begin_text_input_capture(std::string_view initial_text) -> bool { 
		return true; 
	}

	auto DesktopPlatformInput::end_text_input_capture() -> void {

	}

	auto DesktopPlatformInput::set_gamepad_color(int32_t gamepad_id, uint32_t color) -> bool {
		return true;
	}
}