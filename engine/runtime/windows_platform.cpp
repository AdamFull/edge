#include "platform.h"
#include "input_events.h"
#include "window_events.h"

#include <allocator.hpp>
#include <logger.hpp>

#include <math.h>
#include <stdio.h>

// TODO: GLFW is common between windows and linux
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <vulkan/vulkan.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

extern i32 edge_main(edge::PlatformLayout* platform_layout);

namespace edge {
	EventDispatcher* g_event_dispatcher = nullptr;

	struct PlatformLayout {
		HINSTANCE hinst;
		HINSTANCE prev_hinst;
		PSTR cmd_line;
		INT cmd_show;
	};

	struct Window {
		GLFWwindow* handle;

		WindowMode mode;
		bool resizable;
		WindowVsyncMode vsync_mode;

		bool should_close;
	};

	struct PlatformContext {
		const Allocator* alloc;
		PlatformLayout* layout;
		EventDispatcher* event_dispatcher;

		Window* wnd;
		InputState input_state;
	};

	static InputKeyboardKey glfw_key_code_to_edge[] = {
		[GLFW_KEY_SPACE] = InputKeyboardKey::Space,
		[GLFW_KEY_APOSTROPHE] = InputKeyboardKey::Apostrophe,
		[GLFW_KEY_COMMA] = InputKeyboardKey::Comma,
		[GLFW_KEY_MINUS] = InputKeyboardKey::Minus,
		[GLFW_KEY_PERIOD] = InputKeyboardKey::Period,
		[GLFW_KEY_SLASH] = InputKeyboardKey::Slash,
		[GLFW_KEY_0] = InputKeyboardKey::_0,
		[GLFW_KEY_1] = InputKeyboardKey::_1,
		[GLFW_KEY_2] = InputKeyboardKey::_2,
		[GLFW_KEY_3] = InputKeyboardKey::_3,
		[GLFW_KEY_4] = InputKeyboardKey::_4,
		[GLFW_KEY_5] = InputKeyboardKey::_5,
		[GLFW_KEY_6] = InputKeyboardKey::_6,
		[GLFW_KEY_7] = InputKeyboardKey::_7,
		[GLFW_KEY_8] = InputKeyboardKey::_8,
		[GLFW_KEY_9] = InputKeyboardKey::_9,
		[GLFW_KEY_SEMICOLON] = InputKeyboardKey::Semicolon,
		[GLFW_KEY_EQUAL] = InputKeyboardKey::Eq,
		[GLFW_KEY_A] = InputKeyboardKey::A,
		[GLFW_KEY_B] = InputKeyboardKey::B,
		[GLFW_KEY_C] = InputKeyboardKey::C,
		[GLFW_KEY_D] = InputKeyboardKey::D,
		[GLFW_KEY_E] = InputKeyboardKey::E,
		[GLFW_KEY_F] = InputKeyboardKey::F,
		[GLFW_KEY_G] = InputKeyboardKey::G,
		[GLFW_KEY_H] = InputKeyboardKey::H,
		[GLFW_KEY_I] = InputKeyboardKey::I,
		[GLFW_KEY_J] = InputKeyboardKey::J,
		[GLFW_KEY_K] = InputKeyboardKey::K,
		[GLFW_KEY_L] = InputKeyboardKey::L,
		[GLFW_KEY_M] = InputKeyboardKey::M,
		[GLFW_KEY_N] = InputKeyboardKey::N,
		[GLFW_KEY_O] = InputKeyboardKey::O,
		[GLFW_KEY_P] = InputKeyboardKey::P,
		[GLFW_KEY_Q] = InputKeyboardKey::Q,
		[GLFW_KEY_R] = InputKeyboardKey::R,
		[GLFW_KEY_S] = InputKeyboardKey::S,
		[GLFW_KEY_T] = InputKeyboardKey::T,
		[GLFW_KEY_U] = InputKeyboardKey::U,
		[GLFW_KEY_V] = InputKeyboardKey::V,
		[GLFW_KEY_W] = InputKeyboardKey::W,
		[GLFW_KEY_X] = InputKeyboardKey::X,
		[GLFW_KEY_Y] = InputKeyboardKey::Y,
		[GLFW_KEY_Z] = InputKeyboardKey::Z,
		[GLFW_KEY_LEFT_BRACKET] = InputKeyboardKey::LeftBracket,
		[GLFW_KEY_BACKSLASH] = InputKeyboardKey::Backslash,
		[GLFW_KEY_RIGHT_BRACKET] = InputKeyboardKey::RightBracket,
		[GLFW_KEY_GRAVE_ACCENT] = InputKeyboardKey::GraveAccent,

		[GLFW_KEY_ESCAPE] = InputKeyboardKey::Esc,
		[GLFW_KEY_ENTER] = InputKeyboardKey::Enter,
		[GLFW_KEY_TAB] = InputKeyboardKey::Tab,
		[GLFW_KEY_BACKSPACE] = InputKeyboardKey::Backspace,
		[GLFW_KEY_INSERT] = InputKeyboardKey::Insert,
		[GLFW_KEY_DELETE] = InputKeyboardKey::Del,
		[GLFW_KEY_RIGHT] = InputKeyboardKey::Right,
		[GLFW_KEY_LEFT] = InputKeyboardKey::Left,
		[GLFW_KEY_DOWN] = InputKeyboardKey::Down,
		[GLFW_KEY_UP] = InputKeyboardKey::Up,
		[GLFW_KEY_PAGE_UP] = InputKeyboardKey::PageUp,
		[GLFW_KEY_PAGE_DOWN] = InputKeyboardKey::PageDown,
		[GLFW_KEY_HOME] = InputKeyboardKey::Home,
		[GLFW_KEY_END] = InputKeyboardKey::End,
		[GLFW_KEY_CAPS_LOCK] = InputKeyboardKey::CapsLock,
		[GLFW_KEY_SCROLL_LOCK] = InputKeyboardKey::ScrollLock,
		[GLFW_KEY_NUM_LOCK] = InputKeyboardKey::NumLock,
		[GLFW_KEY_PRINT_SCREEN] = InputKeyboardKey::PrintScreen,
		[GLFW_KEY_PAUSE] = InputKeyboardKey::Pause,
		[GLFW_KEY_F1] = InputKeyboardKey::F1,
		[GLFW_KEY_F2] = InputKeyboardKey::F2,
		[GLFW_KEY_F3] = InputKeyboardKey::F3,
		[GLFW_KEY_F4] = InputKeyboardKey::F4,
		[GLFW_KEY_F5] = InputKeyboardKey::F5,
		[GLFW_KEY_F6] = InputKeyboardKey::F6,
		[GLFW_KEY_F7] = InputKeyboardKey::F7,
		[GLFW_KEY_F8] = InputKeyboardKey::F8,
		[GLFW_KEY_F9] = InputKeyboardKey::F9,
		[GLFW_KEY_F10] = InputKeyboardKey::F10,
		[GLFW_KEY_F11] = InputKeyboardKey::F11,
		[GLFW_KEY_F12] = InputKeyboardKey::F12,
		[GLFW_KEY_F13] = InputKeyboardKey::F13,
		[GLFW_KEY_F14] = InputKeyboardKey::F14,
		[GLFW_KEY_F15] = InputKeyboardKey::F15,
		[GLFW_KEY_F16] = InputKeyboardKey::F16,
		[GLFW_KEY_F17] = InputKeyboardKey::F17,
		[GLFW_KEY_F18] = InputKeyboardKey::F18,
		[GLFW_KEY_F19] = InputKeyboardKey::F19,
		[GLFW_KEY_F20] = InputKeyboardKey::F20,
		[GLFW_KEY_F21] = InputKeyboardKey::F21,
		[GLFW_KEY_F22] = InputKeyboardKey::F22,
		[GLFW_KEY_F23] = InputKeyboardKey::F23,
		[GLFW_KEY_F24] = InputKeyboardKey::F24,
		[GLFW_KEY_F25] = InputKeyboardKey::F25,
		[GLFW_KEY_KP_0] = InputKeyboardKey::Kp0,
		[GLFW_KEY_KP_1] = InputKeyboardKey::Kp1,
		[GLFW_KEY_KP_2] = InputKeyboardKey::Kp2,
		[GLFW_KEY_KP_3] = InputKeyboardKey::Kp3,
		[GLFW_KEY_KP_4] = InputKeyboardKey::Kp4,
		[GLFW_KEY_KP_5] = InputKeyboardKey::Kp5,
		[GLFW_KEY_KP_6] = InputKeyboardKey::Kp6,
		[GLFW_KEY_KP_7] = InputKeyboardKey::Kp7,
		[GLFW_KEY_KP_8] = InputKeyboardKey::Kp8,
		[GLFW_KEY_KP_9] = InputKeyboardKey::Kp9,
		[GLFW_KEY_KP_DECIMAL] = InputKeyboardKey::KpDec,
		[GLFW_KEY_KP_DIVIDE] = InputKeyboardKey::KpDiv,
		[GLFW_KEY_KP_MULTIPLY] = InputKeyboardKey::KpMul,
		[GLFW_KEY_KP_SUBTRACT] = InputKeyboardKey::KpSub,
		[GLFW_KEY_KP_ADD] = InputKeyboardKey::KpAdd,
		[GLFW_KEY_KP_ENTER] = InputKeyboardKey::KpEnter,
		[GLFW_KEY_KP_EQUAL] = InputKeyboardKey::KpEq,
		[GLFW_KEY_LEFT_SHIFT] = InputKeyboardKey::LeftShift,
		[GLFW_KEY_LEFT_CONTROL] = InputKeyboardKey::LeftControl,
		[GLFW_KEY_LEFT_ALT] = InputKeyboardKey::LeftAlt,
		[GLFW_KEY_LEFT_SUPER] = InputKeyboardKey::LeftSuper,
		[GLFW_KEY_RIGHT_SHIFT] = InputKeyboardKey::RightShift,
		[GLFW_KEY_RIGHT_CONTROL] = InputKeyboardKey::RightControl,
		[GLFW_KEY_RIGHT_ALT] = InputKeyboardKey::RightAlt,
		[GLFW_KEY_RIGHT_SUPER] = InputKeyboardKey::RightSuper,
		[GLFW_KEY_MENU] = InputKeyboardKey::Menu
	};

	static InputMouseBtn glfw_mouse_btn_code_to_edge[] = {
		[GLFW_MOUSE_BUTTON_1] = InputMouseBtn::Left,
		[GLFW_MOUSE_BUTTON_2] = InputMouseBtn::Right,
		[GLFW_MOUSE_BUTTON_3] = InputMouseBtn::Middle,
		[GLFW_MOUSE_BUTTON_4] = InputMouseBtn::_4,
		[GLFW_MOUSE_BUTTON_5] = InputMouseBtn::_5,
		[GLFW_MOUSE_BUTTON_6] = InputMouseBtn::_6,
		[GLFW_MOUSE_BUTTON_7] = InputMouseBtn::_7,
		[GLFW_MOUSE_BUTTON_8] = InputMouseBtn::_8
	};

	static InputPadBtn glfw_pad_btn_code_to_edge[] = {
		[GLFW_GAMEPAD_BUTTON_A] = InputPadBtn::A,
		[GLFW_GAMEPAD_BUTTON_B] = InputPadBtn::B,
		[GLFW_GAMEPAD_BUTTON_X] = InputPadBtn::X,
		[GLFW_GAMEPAD_BUTTON_Y] = InputPadBtn::Y,
		[GLFW_GAMEPAD_BUTTON_LEFT_BUMPER] = InputPadBtn::BumperLeft,
		[GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER] = InputPadBtn::BumperRight,
		[GLFW_GAMEPAD_BUTTON_BACK] = InputPadBtn::Back,
		[GLFW_GAMEPAD_BUTTON_START] = InputPadBtn::Start,
		[GLFW_GAMEPAD_BUTTON_GUIDE] = InputPadBtn::Guide,
		[GLFW_GAMEPAD_BUTTON_LEFT_THUMB] = InputPadBtn::ThumbLeft,
		[GLFW_GAMEPAD_BUTTON_RIGHT_THUMB] = InputPadBtn::ThumbRight,
		[GLFW_GAMEPAD_BUTTON_DPAD_UP] = InputPadBtn::DpadUp,
		[GLFW_GAMEPAD_BUTTON_DPAD_RIGHT] = InputPadBtn::DpadRight,
		[GLFW_GAMEPAD_BUTTON_DPAD_DOWN] = InputPadBtn::DpadDown,
		[GLFW_GAMEPAD_BUTTON_DPAD_LEFT] = InputPadBtn::DpadLeft
	};

	static void* glfw_allocate_cb(size_t size, void* user) {
		if (!user) {
			return nullptr;
		}

		Allocator* alloc = (Allocator*)user;
		return alloc->malloc(size);
	}

	static void* glfw_reallocate_cb(void* block, size_t size, void* user) {
		if (!user) {
			return nullptr;
		}

		Allocator* alloc = (Allocator*)user;
		return alloc->realloc(block, size);
	}

	static void glfw_deallocate_cb(void* block, void* user) {
		if (!user) {
			return;
		}

		Allocator* alloc = (Allocator*)user;
		alloc->free(block);
	}

	static void window_error_cb(i32 error, const char* description) {
		EDGE_LOG_ERROR("GLFW error: %d. %s.", error, description);
	}

	static i32 glfw_init_counter = 0;

	static void init_glfw_context(const Allocator* alloc) {
		if (glfw_init_counter <= 0) {
			GLFWallocator glfw_allocator;
			glfw_allocator.allocate = glfw_allocate_cb;
			glfw_allocator.reallocate = glfw_reallocate_cb;
			glfw_allocator.deallocate = glfw_deallocate_cb;
			glfw_allocator.user = (void*)alloc;

			glfwInitAllocator(&glfw_allocator);

			if (!glfwInit()) {
				return;
			}

			glfwSetErrorCallback(window_error_cb);
			glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		}
		glfw_init_counter++;
	}

	static void deinit_glfw_context(void) {
		if (--glfw_init_counter <= 0) {
			glfwTerminate();
		}

		glfwSetErrorCallback(nullptr);
	}

	static void window_close_cb(GLFWwindow* window) {
		PlatformContext* ctx = (PlatformContext*)glfwGetWindowUserPointer(window);
		if (!ctx) {
			return;
		}

		WindowCloseEvent evt;
		window_close_event_init(&evt);
		event_dispatcher_dispatch(ctx->event_dispatcher, (EventHeader*)&evt);
	}

	static void window_size_cb(GLFWwindow* window, i32 width, i32 height) {
		PlatformContext* ctx = (PlatformContext*)glfwGetWindowUserPointer(window);
		if (!ctx) {
			return;
		}

		WindowResizeEvent evt;
		window_resize_event_init(&evt, width, height);
		event_dispatcher_dispatch(ctx->event_dispatcher, (EventHeader*)&evt);
	}

	static void window_focus_cb(GLFWwindow* window, i32 focused) {
		PlatformContext* ctx = (PlatformContext*)glfwGetWindowUserPointer(window);
		if (!ctx) {
			return;
		}

		WindowFocusEvent evt;
		window_focus_event_init(&evt, focused == 1);
		event_dispatcher_dispatch(ctx->event_dispatcher, (EventHeader*)&evt);
	}

	static void window_key_cb(GLFWwindow* window, i32 key, i32 scancode, i32 action, i32 mods) {
		PlatformContext* ctx = (PlatformContext*)glfwGetWindowUserPointer(window);
		if (!ctx) {
			return;
		}

		input_update_keyboard_state(&ctx->input_state, ctx->event_dispatcher, glfw_key_code_to_edge[key], action == GLFW_PRESS ? InputKeyAction::Down : InputKeyAction::Up);
	}

	static void window_cursor_cb(GLFWwindow* window, double xpos, double ypos) {
		PlatformContext* ctx = (PlatformContext*)glfwGetWindowUserPointer(window);
		if (!ctx) {
			return;
		}

		input_update_mouse_move_state(&ctx->input_state, ctx->event_dispatcher, (f32)xpos, (f32)ypos);
	}

	static void mouse_button_cb(GLFWwindow* window, i32 button, i32 action, i32 mods) {
		PlatformContext* ctx = (PlatformContext*)glfwGetWindowUserPointer(window);
		if (!ctx) {
			return;
		}

		input_update_mouse_btn_state(&ctx->input_state, ctx->event_dispatcher, glfw_mouse_btn_code_to_edge[button], action == GLFW_PRESS ? InputKeyAction::Down : InputKeyAction::Up);
	}

	static void mouse_scroll_cb(GLFWwindow* window, double xoffset, double yoffset) {
		PlatformContext* ctx = (PlatformContext*)glfwGetWindowUserPointer(window);
		if (!ctx) {
			return;
		}

		InputMouseScrollEvent evt;
		input_mouse_scroll_event_init(&evt, (f32)xoffset, (f32)yoffset);

		event_dispatcher_dispatch(ctx->event_dispatcher, (EventHeader*)&evt);
	}

	static void character_input_cb(GLFWwindow* window, u32 codepoint) {
		PlatformContext* ctx = (PlatformContext*)glfwGetWindowUserPointer(window);
		if (!ctx) {
			return;
		}

		InputTextInputEvent evt;
		input_text_input_event_init(&evt, codepoint);

		event_dispatcher_dispatch(ctx->event_dispatcher, (EventHeader*)&evt);
	}

	static void gamepad_connected_cb(i32 jid, i32 event) {
		const char* guid = glfwGetJoystickGUID(jid);

		i32 vendor_id = 0, product_id = 0;
		sscanf(guid, "%04x%04x", &vendor_id, &product_id);

		InputPadConnectionEvent evt;
		input_pad_connection_event_init(&evt,
			jid,
			vendor_id,
			product_id,
			0,
			event == GLFW_CONNECTED,
			glfwGetJoystickName(jid)
		);

		event_dispatcher_dispatch(g_event_dispatcher, (EventHeader*)&evt);
	}

	PlatformContext* platform_context_create(PlatformContextCreateInfo create_info) {
		if (!create_info.alloc || !create_info.layout) {
			return nullptr;
		}

		PlatformContext* ctx = create_info.alloc->allocate<PlatformContext>();
		if (!ctx) {
			return nullptr;
		}

		memset(&ctx->input_state, 0, sizeof(InputState));

		ctx->alloc = create_info.alloc;
		ctx->layout = create_info.layout;
		ctx->event_dispatcher = create_info.event_dispatcher;
		ctx->wnd = nullptr;

		g_event_dispatcher = create_info.event_dispatcher;

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

		FILE* fp;
		freopen_s(&fp, "conin$", "r", stdin);
		freopen_s(&fp, "conout$", "w", stdout);
		freopen_s(&fp, "conout$", "w", stderr);
#endif
		
		Logger* logger = logger_get_global();
		LoggerOutput* debug_output = logger_create_debug_console_output(create_info.alloc, LogFormat::LogFormat_Default);
		logger_add_output(logger, debug_output);

		return ctx;
	}

	void platform_context_destroy(PlatformContext* ctx) {
		if (!ctx) {
			return;
		}

		// Destroy window if needed
		if (ctx->wnd) {
			Window* wnd = ctx->wnd;
			glfwSetWindowShouldClose(wnd->handle, GLFW_TRUE);
			glfwDestroyWindow(wnd->handle);

			ctx->alloc->deallocate(wnd);
			deinit_glfw_context();
		}

		g_event_dispatcher = nullptr;

		const Allocator* alloc = ctx->alloc;
		alloc->deallocate(ctx);
	}

	void platform_context_get_surface(NotNull<PlatformContext*> ctx, void* surface_info) {
		if (!surface_info) {
			return;
		}

		*(VkWin32SurfaceCreateInfoKHR*)surface_info = {
			.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
			.hinstance = ctx->layout->hinst,
			.hwnd = glfwGetWin32Window(ctx->wnd->handle)
		};
	}

	bool platform_context_window_init(NotNull<PlatformContext*> ctx, WindowCreateInfo create_info) {
		init_glfw_context(ctx->alloc);

		Window* wnd = ctx->alloc->allocate<Window>();
		if (!wnd) {
			deinit_glfw_context();
			return false;
		}

		wnd->mode = create_info.mode;
		wnd->resizable = create_info.resizable;
		wnd->vsync_mode = create_info.vsync_mode;
		wnd->should_close = false;
		
		switch (create_info.mode)
		{
		case WindowMode::Fullscreen: {
			GLFWmonitor* monitor = glfwGetPrimaryMonitor();
			const GLFWvidmode* mode = glfwGetVideoMode(monitor);
			wnd->handle = glfwCreateWindow(mode->width, mode->height, create_info.title, monitor, nullptr);
			break;
		}
		case WindowMode::FullscreenBorderless: {
			GLFWmonitor* monitor = glfwGetPrimaryMonitor();
			const GLFWvidmode* mode = glfwGetVideoMode(monitor);

			glfwWindowHint(GLFW_RED_BITS, mode->redBits);
			glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
			glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
			glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);

			wnd->handle = glfwCreateWindow(mode->width, mode->height, create_info.title, monitor, nullptr);
			break;
		}
		default: {
			wnd->handle = glfwCreateWindow(create_info.width, create_info.height, create_info.title, nullptr, nullptr);
			break;
		}
		}

		if (!wnd->handle) {
			ctx->alloc->deallocate(wnd);
			deinit_glfw_context();
			return false;
		}

		glfwSetWindowCloseCallback(wnd->handle, window_close_cb);
		glfwSetWindowSizeCallback(wnd->handle, window_size_cb);
		glfwSetWindowFocusCallback(wnd->handle, window_focus_cb);

		// For input
		glfwSetKeyCallback(wnd->handle, window_key_cb);
		glfwSetCursorPosCallback(wnd->handle, window_cursor_cb);
		glfwSetMouseButtonCallback(wnd->handle, mouse_button_cb);
		glfwSetScrollCallback(wnd->handle, mouse_scroll_cb);
		glfwSetCharCallback(wnd->handle, character_input_cb);
		glfwSetJoystickCallback(gamepad_connected_cb);

		glfwSetInputMode(wnd->handle, GLFW_STICKY_KEYS, 1);
		glfwSetInputMode(wnd->handle, GLFW_STICKY_MOUSE_BUTTONS, 1);

		glfwSetWindowUserPointer(wnd->handle, ctx.m_ptr);

		ctx->wnd = wnd;

		return true;
	}

	bool platform_context_window_should_close(NotNull<PlatformContext*> ctx) {
		if (!ctx->wnd) {
			return false;
		}

		return ctx->wnd->should_close;
	}

	void platform_context_window_process_events(NotNull<PlatformContext*> ctx, f32 delta_time) {
		if (!ctx->wnd || !ctx->wnd->handle) {
			return;
		}

		Window* wnd = ctx->wnd;

		glfwPollEvents();
		wnd->should_close = glfwWindowShouldClose(wnd->handle);

		for (i32 jid = 0; jid < GLFW_JOYSTICK_LAST; ++jid) {
			GLFWgamepadstate state;
			if (glfwGetGamepadState(jid, &state)) {
				for (i32 btn = 0; btn < GLFW_GAMEPAD_BUTTON_LAST + 1; ++btn) {
					input_update_pad_btn_state(&ctx->input_state, ctx->event_dispatcher, jid, glfw_pad_btn_code_to_edge[btn], (InputKeyAction)state.buttons[btn]);
				}
				
				input_update_pad_axis_state(&ctx->input_state, ctx->event_dispatcher, jid, InputPadAxis::StickLeft, state.axes[GLFW_GAMEPAD_AXIS_LEFT_X], state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y], 0.0f);
				input_update_pad_axis_state(&ctx->input_state, ctx->event_dispatcher, jid, InputPadAxis::StickRight, state.axes[GLFW_GAMEPAD_AXIS_RIGHT_X], state.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y], 0.0f);
				input_update_pad_axis_state(&ctx->input_state, ctx->event_dispatcher, jid, InputPadAxis::TriggerLeft, state.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER], 0.0f, 0.0f);
				input_update_pad_axis_state(&ctx->input_state, ctx->event_dispatcher, jid, InputPadAxis::TriggerRight, state.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER], 0.0f, 0.0f);
			}
		}
	}

	void platform_context_window_show(PlatformContext* ctx) {
		if (!ctx || !ctx->wnd || !ctx->wnd->handle) {
			return;
		}

		glfwShowWindow(ctx->wnd->handle);
	}

	void platform_context_window_hide(NotNull<PlatformContext*> ctx) {
		if (!ctx->wnd || !ctx->wnd->handle) {
			return;
		}

		glfwHideWindow(ctx->wnd->handle);
	}

	void platform_context_window_set_title(NotNull<PlatformContext*> ctx, const char* title) {
		if (!ctx->wnd || !ctx->wnd->handle || !title) {
			return;
		}

		glfwSetWindowTitle(ctx->wnd->handle, title);
	}

	void platform_context_window_get_size(NotNull<PlatformContext*> ctx, i32* width, i32* height) {
		if (!width || !height) {
			return;
		}

		glfwGetWindowSize(ctx->wnd->handle, width, height);
	}

	f32 platform_context_window_dpi_scale_factor(NotNull<PlatformContext*> ctx) {
		if (!ctx->wnd || !ctx->wnd->handle) {
			return 1.0f;
		}

#if 0
		GLFWmonitor* monitor = glfwGetWindowMonitor(ctx->wnd->handle);
		if (!monitor) {
			i32 window_x, window_y, window_width, window_height;
			glfwGetWindowPos(ctx->wnd->handle, &window_x, &window_y);
			glfwGetWindowSize(ctx->wnd->handle, &window_width, &window_height);

			int monitor_count;
			GLFWmonitor** monitors = glfwGetMonitors(&monitor_count);

			int max_overlap_area = 0;
			for (int i = 0; i < monitor_count; i++) {
				int monitor_x, monitor_y;
				glfwGetMonitorPos(monitors[i], &monitor_x, &monitor_y);

				const GLFWvidmode* mode = glfwGetVideoMode(monitors[i]);

				int overlap_x = max(0, min(window_x + window_width, monitor_x + mode->width) - max(window_x, monitor_x));
				int overlap_y = max(0, min(window_y + window_height, monitor_y + mode->height) - max(window_y, monitor_y));
				int overlap_area = overlap_x * overlap_y;

				if (overlap_area > max_overlap_area) {
					max_overlap_area = overlap_area;
					monitor = monitors[i];
				}
			}

			if (!monitor) {
				monitor = glfwGetPrimaryMonitor();
			}
		}

		const GLFWvidmode* vidmode = glfwGetVideoMode(monitor);

		i32 width_mm, height_mm;
		glfwGetMonitorPhysicalSize(monitor, &width_mm, &height_mm);

		// As suggested by the GLFW monitor guide
		const f32 inch_to_mm = 25.0f;
		const f32 win_base_density = 96.0f;

		f32 dpi = (f32)(vidmode->width / (width_mm / inch_to_mm));
		return dpi / win_base_density;
#else
		f32 xscale, yscale;
		glfwGetWindowContentScale(ctx->wnd->handle, &xscale, &yscale);
		return xscale;
#endif
	}

	f32 platform_context_window_content_scale_factor(NotNull<PlatformContext*> ctx) {
		if (!ctx->wnd || !ctx->wnd->handle) {
			return 1.0f;
		}

		i32 fb_width, fb_height;
		glfwGetFramebufferSize(ctx->wnd->handle, &fb_width, &fb_height);
		i32 win_width, win_height;
		glfwGetWindowSize(ctx->wnd->handle, &win_width, &win_height);

		// Needed for ui scale
		return (f32)(fb_width) / win_width;
	}
}

i32 APIENTRY WinMain(HINSTANCE hinst, HINSTANCE prev_hinst, PSTR cmd_line, INT cmd_show) {
	edge::PlatformLayout platform_layout = {
		.hinst = hinst,
		.prev_hinst = prev_hinst,
		.cmd_line = cmd_line,
		.cmd_show = cmd_show
	};

	return edge_main(&platform_layout);
}