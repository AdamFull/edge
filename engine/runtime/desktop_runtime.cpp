#include "runtime.h"
#include "input.h"

#include <logger.hpp>

#if defined(EDGE_PLATFORM_WINDOWS)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(EDGE_PLATFORM_LINUX)
#endif

#include <stdio.h>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <vulkan/vulkan.h>

extern i32 edge_main(edge::RuntimeLayout* runtime_layout);

namespace edge {
	struct RuntimeLayout {
#if defined(EDGE_PLATFORM_WINDOWS)
		HINSTANCE hinst;
		HINSTANCE prev_hinst;
		PSTR cmd_line;
		INT cmd_show;
#endif
	};

	namespace {
		InputKeyboardKey glfw_key_to_engine_key(i32 glfw_key) {
			switch (glfw_key) {
			case GLFW_KEY_SPACE: return InputKeyboardKey::Space;
			case GLFW_KEY_APOSTROPHE: return InputKeyboardKey::Apostrophe;
			case GLFW_KEY_COMMA: return InputKeyboardKey::Comma;
			case GLFW_KEY_MINUS: return InputKeyboardKey::Minus;
			case GLFW_KEY_PERIOD: return InputKeyboardKey::Period;
			case GLFW_KEY_SLASH: return InputKeyboardKey::Slash;
			case GLFW_KEY_0: return InputKeyboardKey::_0;
			case GLFW_KEY_1: return InputKeyboardKey::_1;
			case GLFW_KEY_2: return InputKeyboardKey::_2;
			case GLFW_KEY_3: return InputKeyboardKey::_3;
			case GLFW_KEY_4: return InputKeyboardKey::_4;
			case GLFW_KEY_5: return InputKeyboardKey::_5;
			case GLFW_KEY_6: return InputKeyboardKey::_6;
			case GLFW_KEY_7: return InputKeyboardKey::_7;
			case GLFW_KEY_8: return InputKeyboardKey::_8;
			case GLFW_KEY_9: return InputKeyboardKey::_9;
			case GLFW_KEY_SEMICOLON: return InputKeyboardKey::Semicolon;
			case GLFW_KEY_EQUAL: return InputKeyboardKey::Eq;
			case GLFW_KEY_A: return InputKeyboardKey::A;
			case GLFW_KEY_B: return InputKeyboardKey::B;
			case GLFW_KEY_C: return InputKeyboardKey::C;
			case GLFW_KEY_D: return InputKeyboardKey::D;
			case GLFW_KEY_E: return InputKeyboardKey::E;
			case GLFW_KEY_F: return InputKeyboardKey::F;
			case GLFW_KEY_G: return InputKeyboardKey::G;
			case GLFW_KEY_H: return InputKeyboardKey::H;
			case GLFW_KEY_I: return InputKeyboardKey::I;
			case GLFW_KEY_J: return InputKeyboardKey::J;
			case GLFW_KEY_K: return InputKeyboardKey::K;
			case GLFW_KEY_L: return InputKeyboardKey::L;
			case GLFW_KEY_M: return InputKeyboardKey::M;
			case GLFW_KEY_N: return InputKeyboardKey::N;
			case GLFW_KEY_O: return InputKeyboardKey::O;
			case GLFW_KEY_P: return InputKeyboardKey::P;
			case GLFW_KEY_Q: return InputKeyboardKey::Q;
			case GLFW_KEY_R: return InputKeyboardKey::R;
			case GLFW_KEY_S: return InputKeyboardKey::S;
			case GLFW_KEY_T: return InputKeyboardKey::T;
			case GLFW_KEY_U: return InputKeyboardKey::U;
			case GLFW_KEY_V: return InputKeyboardKey::V;
			case GLFW_KEY_W: return InputKeyboardKey::W;
			case GLFW_KEY_X: return InputKeyboardKey::X;
			case GLFW_KEY_Y: return InputKeyboardKey::Y;
			case GLFW_KEY_Z: return InputKeyboardKey::Z;
			case GLFW_KEY_LEFT_BRACKET: return InputKeyboardKey::LeftBracket;
			case GLFW_KEY_BACKSLASH: return InputKeyboardKey::Backslash;
			case GLFW_KEY_RIGHT_BRACKET: return InputKeyboardKey::RightBracket;
			case GLFW_KEY_GRAVE_ACCENT: return InputKeyboardKey::GraveAccent;
			case GLFW_KEY_ESCAPE: return InputKeyboardKey::Esc;
			case GLFW_KEY_ENTER: return InputKeyboardKey::Enter;
			case GLFW_KEY_TAB: return InputKeyboardKey::Tab;
			case GLFW_KEY_BACKSPACE: return InputKeyboardKey::Backspace;
			case GLFW_KEY_INSERT: return InputKeyboardKey::Insert;
			case GLFW_KEY_DELETE: return InputKeyboardKey::Del;
			case GLFW_KEY_RIGHT: return InputKeyboardKey::Right;
			case GLFW_KEY_LEFT: return InputKeyboardKey::Left;
			case GLFW_KEY_DOWN: return InputKeyboardKey::Down;
			case GLFW_KEY_UP: return InputKeyboardKey::Up;
			case GLFW_KEY_PAGE_UP: return InputKeyboardKey::PageUp;
			case GLFW_KEY_PAGE_DOWN: return InputKeyboardKey::PageDown;
			case GLFW_KEY_HOME: return InputKeyboardKey::Home;
			case GLFW_KEY_END: return InputKeyboardKey::End;
			case GLFW_KEY_CAPS_LOCK: return InputKeyboardKey::CapsLock;
			case GLFW_KEY_SCROLL_LOCK: return InputKeyboardKey::ScrollLock;
			case GLFW_KEY_NUM_LOCK: return InputKeyboardKey::NumLock;
			case GLFW_KEY_PRINT_SCREEN: return InputKeyboardKey::PrintScreen;
			case GLFW_KEY_PAUSE: return InputKeyboardKey::Pause;
			case GLFW_KEY_F1: return InputKeyboardKey::F1;
			case GLFW_KEY_F2: return InputKeyboardKey::F2;
			case GLFW_KEY_F3: return InputKeyboardKey::F3;
			case GLFW_KEY_F4: return InputKeyboardKey::F4;
			case GLFW_KEY_F5: return InputKeyboardKey::F5;
			case GLFW_KEY_F6: return InputKeyboardKey::F6;
			case GLFW_KEY_F7: return InputKeyboardKey::F7;
			case GLFW_KEY_F8: return InputKeyboardKey::F8;
			case GLFW_KEY_F9: return InputKeyboardKey::F9;
			case GLFW_KEY_F10: return InputKeyboardKey::F10;
			case GLFW_KEY_F11: return InputKeyboardKey::F11;
			case GLFW_KEY_F12: return InputKeyboardKey::F12;
			case GLFW_KEY_KP_0: return InputKeyboardKey::Kp0;
			case GLFW_KEY_KP_1: return InputKeyboardKey::Kp1;
			case GLFW_KEY_KP_2: return InputKeyboardKey::Kp2;
			case GLFW_KEY_KP_3: return InputKeyboardKey::Kp3;
			case GLFW_KEY_KP_4: return InputKeyboardKey::Kp4;
			case GLFW_KEY_KP_5: return InputKeyboardKey::Kp5;
			case GLFW_KEY_KP_6: return InputKeyboardKey::Kp6;
			case GLFW_KEY_KP_7: return InputKeyboardKey::Kp7;
			case GLFW_KEY_KP_8: return InputKeyboardKey::Kp8;
			case GLFW_KEY_KP_9: return InputKeyboardKey::Kp9;
			case GLFW_KEY_KP_DECIMAL: return InputKeyboardKey::KpDec;
			case GLFW_KEY_KP_DIVIDE: return InputKeyboardKey::KpDiv;
			case GLFW_KEY_KP_MULTIPLY: return InputKeyboardKey::KpMul;
			case GLFW_KEY_KP_SUBTRACT: return InputKeyboardKey::KpSub;
			case GLFW_KEY_KP_ADD: return InputKeyboardKey::KpAdd;
			case GLFW_KEY_KP_ENTER: return InputKeyboardKey::KpEnter;
			case GLFW_KEY_KP_EQUAL: return InputKeyboardKey::KpEq;
			case GLFW_KEY_LEFT_SHIFT: return InputKeyboardKey::LeftShift;
			case GLFW_KEY_LEFT_CONTROL: return InputKeyboardKey::LeftControl;
			case GLFW_KEY_LEFT_ALT: return InputKeyboardKey::LeftAlt;
			case GLFW_KEY_LEFT_SUPER: return InputKeyboardKey::LeftSuper;
			case GLFW_KEY_RIGHT_SHIFT: return InputKeyboardKey::RightShift;
			case GLFW_KEY_RIGHT_CONTROL: return InputKeyboardKey::RightControl;
			case GLFW_KEY_RIGHT_ALT: return InputKeyboardKey::RightAlt;
			case GLFW_KEY_RIGHT_SUPER: return InputKeyboardKey::RightSuper;
			case GLFW_KEY_MENU: return InputKeyboardKey::Menu;
			default: return InputKeyboardKey::Unknown;
			}
		}

		InputMouseBtn glfw_mouse_btn_to_engine_btn(i32 glfw_btn) {
			switch (glfw_btn) {
			case GLFW_MOUSE_BUTTON_LEFT: return InputMouseBtn::Left;
			case GLFW_MOUSE_BUTTON_RIGHT: return InputMouseBtn::Right;
			case GLFW_MOUSE_BUTTON_MIDDLE: return InputMouseBtn::Middle;
			case GLFW_MOUSE_BUTTON_4: return InputMouseBtn::_4;
			case GLFW_MOUSE_BUTTON_5: return InputMouseBtn::_5;
			case GLFW_MOUSE_BUTTON_6: return InputMouseBtn::_6;
			case GLFW_MOUSE_BUTTON_7: return InputMouseBtn::_7;
			case GLFW_MOUSE_BUTTON_8: return InputMouseBtn::_8;
			default: return InputMouseBtn::Unknown;
			}
		}

		InputPadBtn glfw_gamepad_btn_to_engine_btn(i32 glfw_btn) {
			switch (glfw_btn) {
			case GLFW_GAMEPAD_BUTTON_A: return InputPadBtn::A;
			case GLFW_GAMEPAD_BUTTON_B: return InputPadBtn::B;
			case GLFW_GAMEPAD_BUTTON_X: return InputPadBtn::X;
			case GLFW_GAMEPAD_BUTTON_Y: return InputPadBtn::Y;
			case GLFW_GAMEPAD_BUTTON_LEFT_BUMPER: return InputPadBtn::BumperLeft;
			case GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER: return InputPadBtn::BumperRight;
			case GLFW_GAMEPAD_BUTTON_BACK: return InputPadBtn::Back;
			case GLFW_GAMEPAD_BUTTON_START: return InputPadBtn::Start;
			case GLFW_GAMEPAD_BUTTON_GUIDE: return InputPadBtn::Guide;
			case GLFW_GAMEPAD_BUTTON_LEFT_THUMB: return InputPadBtn::ThumbLeft;
			case GLFW_GAMEPAD_BUTTON_RIGHT_THUMB: return InputPadBtn::ThumbRight;
			case GLFW_GAMEPAD_BUTTON_DPAD_UP: return InputPadBtn::DpadUp;
			case GLFW_GAMEPAD_BUTTON_DPAD_RIGHT: return InputPadBtn::DpadRight;
			case GLFW_GAMEPAD_BUTTON_DPAD_DOWN: return InputPadBtn::DpadDown;
			case GLFW_GAMEPAD_BUTTON_DPAD_LEFT: return InputPadBtn::DpadLeft;
			default: return InputPadBtn::Unknown;
			}
		}

		f32 apply_deadzone(f32 value, f32 deadzone) {
			if (abs(value) < deadzone) {
				return 0.0f;
			}

			// Rescale to 0-1 range after deadzone
			f32 sign = value < 0.0f ? -1.0f : 1.0f;
			f32 abs_val = abs(value);
			return sign * ((abs_val - deadzone) / (1.0f - deadzone));
		}
	}

	struct DesktopRuntime final : IRuntime {
		bool init(const RuntimeInitInfo& init_info) noexcept override {
			layout = init_info.layout;

#if defined(EDGE_PLATFORM_WINDOWS)
#if EDGE_DEBUG
			if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
				if (!AllocConsole()) {
					EDGE_LOG_DEBUG("Failed to allocate console.");
				}
			}

			HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
			DWORD dwMode = 0;

			GetConsoleMode(hOut, &dwMode);
			dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
			SetConsoleMode(hOut, dwMode);

			FILE* fp = nullptr;
			freopen_s(&fp, "conin$", "r", stdin);
			freopen_s(&fp, "conout$", "w", stdout);
			freopen_s(&fp, "conout$", "w", stderr);
#endif // EDGE_DEBUG

			Logger* logger = logger_get_global();
			ILoggerOutput* debug_output = logger_create_debug_console_output(init_info.alloc, LogFormat::LogFormat_Default);
			logger->add_output(init_info.alloc, debug_output);

#elif defined(EDGE_PLATFORM_LINUX)
#endif

			if (!input_system.create(init_info.alloc)) {
				EDGE_LOG_ERROR("Failed to create input system.");
				return false;
			}

			GLFWallocator glfw_allocator = {
				.allocate = [](size_t size, void* user) -> void* {
					return ((Allocator*)user)->malloc(size);
				},
				.reallocate = [](void* block, size_t size, void* user) -> void* {
					return ((Allocator*)user)->realloc(block, size);
				},
				.deallocate = [](void* block, void* user) -> void {
					((Allocator*)user)->free(block);
				},
				.user = (void*)init_info.alloc
			};

			glfwInitAllocator(&glfw_allocator);

			if (!glfwInit()) {
				EDGE_LOG_ERROR("Failed to init glfw context.");
				input_system.destroy(init_info.alloc);
				return false;
			}

			glfwSetErrorCallback([](i32 error, const char* description) -> void {
				EDGE_LOG_ERROR("GLFW error: %d. %s.", error, description);
				});
			glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

			switch (init_info.mode)
			{
			case WindowMode::Fullscreen: {
				GLFWmonitor* monitor = glfwGetPrimaryMonitor();
				const GLFWvidmode* mode = glfwGetVideoMode(monitor);
				wnd_handle = glfwCreateWindow(mode->width, mode->height, init_info.title, monitor, nullptr);
				break;
			}
			case WindowMode::FullscreenBorderless: {
				GLFWmonitor* monitor = glfwGetPrimaryMonitor();
				const GLFWvidmode* mode = glfwGetVideoMode(monitor);

				glfwWindowHint(GLFW_RED_BITS, mode->redBits);
				glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
				glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
				glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);

				wnd_handle = glfwCreateWindow(mode->width, mode->height, init_info.title, monitor, nullptr);
				break;
			}
			default: {
				wnd_handle = glfwCreateWindow(init_info.width, init_info.height, init_info.title, nullptr, nullptr);
				break;
			}
			}

			if (!wnd_handle) {
				input_system.destroy(init_info.alloc);
				return false;
			}

			glfwSetWindowFocusCallback(wnd_handle, 
				[](GLFWwindow* window, i32 focused) -> void {
					auto* rt = (DesktopRuntime*)glfwGetWindowUserPointer(window);
					rt->focused = focused == GLFW_TRUE;
				});

			// For input
			glfwSetKeyCallback(wnd_handle, 
				[](GLFWwindow* window, i32 key, i32 scancode, i32 action, i32 mods) -> void {
					auto* rt = (DesktopRuntime*)glfwGetWindowUserPointer(window);
					InputKeyboardKey engine_key = glfw_key_to_engine_key(key);
					if (engine_key != InputKeyboardKey::Unknown && action != GLFW_REPEAT) {
						rt->input_system.set_state(engine_key, action == GLFW_PRESS);
					}
				});

			glfwSetCursorPosCallback(wnd_handle, 
				[](GLFWwindow* window, double xpos, double ypos) -> void {
					auto* rt = (DesktopRuntime*)glfwGetWindowUserPointer(window);
					rt->input_system.current_state.mouse.set_axis(InputMouseAxis::PosX, static_cast<f32>(xpos));
					rt->input_system.current_state.mouse.set_axis(InputMouseAxis::PosY, static_cast<f32>(ypos));
				});

			glfwSetMouseButtonCallback(wnd_handle, 
				[](GLFWwindow* window, i32 button, i32 action, i32 mods) -> void {
					auto* rt = (DesktopRuntime*)glfwGetWindowUserPointer(window);
					InputMouseBtn engine_btn = glfw_mouse_btn_to_engine_btn(button);
					if (engine_btn != InputMouseBtn::Unknown && action != GLFW_REPEAT) {
						rt->input_system.set_state(engine_btn, action == GLFW_PRESS);
					}
				});

			glfwSetScrollCallback(wnd_handle, 
				[](GLFWwindow* window, double xoffset, double yoffset) -> void {
					auto* rt = (DesktopRuntime*)glfwGetWindowUserPointer(window);
					rt->input_system.current_state.mouse.set_axis(InputMouseAxis::ScrollX, static_cast<f32>(xoffset));
					rt->input_system.current_state.mouse.set_axis(InputMouseAxis::ScrollY, static_cast<f32>(yoffset));
				});

			glfwSetCharCallback(wnd_handle, 
				[](GLFWwindow* window, u32 codepoint) -> void {
					auto* rt = (DesktopRuntime*)glfwGetWindowUserPointer(window);
#if 0
					if (rt->input_system.current_state.text_input_enabled) {
						rt->input_system.current_state.text_input.push_back(rt->alloc, static_cast<char32_t>(codepoint));
					}
#endif
				});

			glfwSetJoystickCallback(
				[](i32 jid, i32 event) -> void {
					const char* guid = glfwGetJoystickGUID(jid);

					i32 vendor_id = 0, product_id = 0;
					sscanf(guid, "%04x%04x", &vendor_id, &product_id);
				});

			glfwSetInputMode(wnd_handle, GLFW_STICKY_KEYS, 1);
			glfwSetInputMode(wnd_handle, GLFW_STICKY_MOUSE_BUTTONS, 1);

			glfwSetWindowUserPointer(wnd_handle, this);

			return true;
		}

		void deinit(NotNull<const Allocator*> alloc) noexcept override {
			input_system.destroy(alloc);

			glfwSetWindowShouldClose(wnd_handle, GLFW_TRUE);
			glfwDestroyWindow(wnd_handle);

			glfwTerminate();
			glfwSetErrorCallback(nullptr);
		}

		bool requested_close() const noexcept override {
			return glfwWindowShouldClose(wnd_handle) == GLFW_TRUE;
		}

		void process_events() noexcept override {
			glfwPollEvents();

			for (i32 jid = GLFW_JOYSTICK_1; jid <= GLFW_JOYSTICK_LAST; ++jid) {
				usize pad_idx = jid - GLFW_JOYSTICK_1;
				if (pad_idx >= MAX_PAD_SLOTS) break;

				InputPadState& pad = input_system.current_state.gamepads[pad_idx];

				if (glfwJoystickPresent(jid) && glfwJoystickIsGamepad(jid)) {
					if (!pad.connected) {
						pad.connected = true;

						const char* name = glfwGetGamepadName(jid);
						if (name) {
							strncpy(pad.name, name, sizeof(pad.name) - 1);
						}

						const char* guid = glfwGetJoystickGUID(jid);
						if (guid) {
							sscanf(guid, "%04hx%04hx", &pad.vendor_id, &pad.product_id);
						}
					}

					GLFWgamepadstate state;
					if (glfwGetGamepadState(jid, &state)) {
						for (i32 btn = 0; btn < GLFW_GAMEPAD_BUTTON_LAST + 1; ++btn) {
							InputPadBtn engine_btn = glfw_gamepad_btn_to_engine_btn(btn);
							if (engine_btn != InputPadBtn::Unknown) {
								usize btn_idx = static_cast<usize>(engine_btn);

								if (state.buttons[btn] == GLFW_PRESS) {
									pad.btn_states.set(btn_idx);
								}
								else {
									pad.btn_states.clear(btn_idx);
								}
							}
						}

						f32 left_x = apply_deadzone(state.axes[GLFW_GAMEPAD_AXIS_LEFT_X], pad.stick_deadzone);
						f32 left_y = apply_deadzone(state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y], pad.stick_deadzone);
						f32 right_x = apply_deadzone(state.axes[GLFW_GAMEPAD_AXIS_RIGHT_X], pad.stick_deadzone);
						f32 right_y = apply_deadzone(state.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y], pad.stick_deadzone);

						f32 trigger_left = apply_deadzone(state.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER], pad.trigger_deadzone);
						f32 trigger_right = apply_deadzone(state.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER], pad.trigger_deadzone);

						// Map to 0-1 range for triggers (GLFW returns -1 to 1)
						trigger_left = (trigger_left + 1.0f) * 0.5f;
						trigger_right = (trigger_right + 1.0f) * 0.5f;

						pad.set_axis(InputPadAxis::StickLeftX, left_x);
						pad.set_axis(InputPadAxis::StickLeftY, left_y);
						pad.set_axis(InputPadAxis::StickRightX, right_x);
						pad.set_axis(InputPadAxis::StickRightY, right_y);
						pad.set_axis(InputPadAxis::TriggerLeft, trigger_left);
						pad.set_axis(InputPadAxis::TriggerRight, trigger_right);

						if (trigger_left > 0.5f) {
							pad.btn_states.set(static_cast<usize>(InputPadBtn::TriggerLeft));
						}
						else {
							pad.btn_states.clear(static_cast<usize>(InputPadBtn::TriggerLeft));
						}

						if (trigger_right > 0.5f) {
							pad.btn_states.set(static_cast<usize>(InputPadBtn::TriggerRight));
						}
						else {
							pad.btn_states.clear(static_cast<usize>(InputPadBtn::TriggerRight));
						}
					}
				}
				else {
					if (pad.connected) {
						pad.connected = false;
						memset(&pad, 0, sizeof(InputPadState));
					}
				}
			}

			input_system.update();
		}

		void get_surface(void* surface_info) const noexcept override {
			if (!surface_info) {
				return;
			}

#if defined(EDGE_PLATFORM_WINDOWS)
			* (VkWin32SurfaceCreateInfoKHR*)surface_info = {
				.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
				.hinstance = layout->hinst,
				.hwnd = glfwGetWin32Window(wnd_handle)
			};
#else
#error "Not implamented"
#endif
		}

		void get_surface_extent(i32& width, i32& height) const noexcept override {
			glfwGetWindowSize(wnd_handle, &width, &height);
		}

		f32 get_surface_scale_factor() const noexcept override {
			f32 xscale, yscale;
			glfwGetWindowContentScale(wnd_handle, &xscale, &yscale);
			return xscale;
		}

		bool is_focused() const noexcept override {
			return focused;
		}

		void set_title(const char* title) noexcept override {
			glfwSetWindowTitle(wnd_handle, title);
		}

		InputSystem* get_input() noexcept override {
			return &input_system;
		}

	private:
		RuntimeLayout* layout = nullptr;
		GLFWwindow* wnd_handle = nullptr;
		bool focused = true;
		InputSystem input_system = {};
	};

	IRuntime* create_runtime(NotNull<const Allocator*> alloc) noexcept {
		return alloc->allocate<DesktopRuntime>();
	}
}

#if defined(EDGE_PLATFORM_WINDOWS)
i32 APIENTRY WinMain(HINSTANCE hinst, HINSTANCE prev_hinst, PSTR cmd_line, INT cmd_show) {
	edge::RuntimeLayout platform_layout = {
		.hinst = hinst,
		.prev_hinst = prev_hinst,
		.cmd_line = cmd_line,
		.cmd_show = cmd_show
	};

	return edge_main(&platform_layout);
}
#endif