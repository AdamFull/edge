#include "platform.h"
#include "input_events.h"

#include <edge_allocator.h>
#include <edge_logger.h>

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

#include <Windows.h>

static event_dispatcher_t* g_event_dispatcher = NULL;

extern int edge_main(platform_layout_t* platform_layout);

struct platform_layout {
	HINSTANCE hinst;
	HINSTANCE prev_hinst;
	PSTR cmd_line;
	INT cmd_show;
};

struct edge_window {
	GLFWwindow* handle;

	window_mode_t mode;
	bool resizable;
	window_vsync_mode_t vsync_mode;

	bool should_close;
};

struct platform_context {
	edge_allocator_t* alloc;
	platform_layout_t* layout;
	event_dispatcher_t* event_dispatcher;

	struct edge_window* wnd;
	input_state_t input_state;
};

static input_keyboard_key_t glfw_key_code_to_edge[] = {
	[GLFW_KEY_SPACE] = INPUT_KEY_SPACE,
	[GLFW_KEY_APOSTROPHE] = INPUT_KEY_APOSTROPHE,
	[GLFW_KEY_COMMA] = INPUT_KEY_COMMA,
	[GLFW_KEY_MINUS] = INPUT_KEY_MINUS,
	[GLFW_KEY_PERIOD] = INPUT_KEY_PERIOD,
	[GLFW_KEY_SLASH] = INPUT_KEY_SLASH,
	[GLFW_KEY_0] = INPUT_KEY_0,
	[GLFW_KEY_1] = INPUT_KEY_1,
	[GLFW_KEY_2] = INPUT_KEY_2,
	[GLFW_KEY_3] = INPUT_KEY_3,
	[GLFW_KEY_4] = INPUT_KEY_4,
	[GLFW_KEY_5] = INPUT_KEY_5,
	[GLFW_KEY_6] = INPUT_KEY_6,
	[GLFW_KEY_7] = INPUT_KEY_7,
	[GLFW_KEY_8] = INPUT_KEY_8,
	[GLFW_KEY_9] = INPUT_KEY_9,
	[GLFW_KEY_SEMICOLON] = INPUT_KEY_SEMICOLON,
	[GLFW_KEY_EQUAL] = INPUT_KEY_EQ,
	[GLFW_KEY_A] = INPUT_KEY_A,
	[GLFW_KEY_B] = INPUT_KEY_B,
	[GLFW_KEY_C] = INPUT_KEY_C,
	[GLFW_KEY_D] = INPUT_KEY_D,
	[GLFW_KEY_E] = INPUT_KEY_E,
	[GLFW_KEY_F] = INPUT_KEY_F,
	[GLFW_KEY_G] = INPUT_KEY_G,
	[GLFW_KEY_H] = INPUT_KEY_H,
	[GLFW_KEY_I] = INPUT_KEY_I,
	[GLFW_KEY_J] = INPUT_KEY_J,
	[GLFW_KEY_K] = INPUT_KEY_K,
	[GLFW_KEY_L] = INPUT_KEY_L,
	[GLFW_KEY_M] = INPUT_KEY_M,
	[GLFW_KEY_N] = INPUT_KEY_N,
	[GLFW_KEY_O] = INPUT_KEY_O,
	[GLFW_KEY_P] = INPUT_KEY_P,
	[GLFW_KEY_Q] = INPUT_KEY_Q,
	[GLFW_KEY_R] = INPUT_KEY_R,
	[GLFW_KEY_S] = INPUT_KEY_S,
	[GLFW_KEY_T] = INPUT_KEY_T,
	[GLFW_KEY_U] = INPUT_KEY_U,
	[GLFW_KEY_V] = INPUT_KEY_V,
	[GLFW_KEY_W] = INPUT_KEY_W,
	[GLFW_KEY_X] = INPUT_KEY_X,
	[GLFW_KEY_Y] = INPUT_KEY_Y,
	[GLFW_KEY_Z] = INPUT_KEY_Z,
	[GLFW_KEY_LEFT_BRACKET] = INPUT_KEY_LEFT_BRACKET,
	[GLFW_KEY_BACKSLASH] = INPUT_KEY_BACKSLASH,
	[GLFW_KEY_RIGHT_BRACKET] = INPUT_KEY_RIGHT_BRACKET,
	[GLFW_KEY_GRAVE_ACCENT] = INPUT_KEY_GRAVE_ACCENT,

	[GLFW_KEY_ESCAPE] = INPUT_KEY_ESC,
	[GLFW_KEY_ENTER] = INPUT_KEY_ENTER,
	[GLFW_KEY_TAB] = INPUT_KEY_TAB,
	[GLFW_KEY_BACKSPACE] = INPUT_KEY_BACKSPACE,
	[GLFW_KEY_INSERT] = INPUT_KEY_INSERT,
	[GLFW_KEY_DELETE] = INPUT_KEY_DEL,
	[GLFW_KEY_RIGHT] = INPUT_KEY_RIGHT,
	[GLFW_KEY_LEFT] = INPUT_KEY_LEFT,
	[GLFW_KEY_DOWN] = INPUT_KEY_DOWN,
	[GLFW_KEY_UP] = INPUT_KEY_UP,
	[GLFW_KEY_PAGE_UP] = INPUT_KEY_PAGE_UP,
	[GLFW_KEY_PAGE_DOWN] = INPUT_KEY_PAGE_DOWN,
	[GLFW_KEY_HOME] = INPUT_KEY_HOME,
	[GLFW_KEY_END] = INPUT_KEY_END,
	[GLFW_KEY_CAPS_LOCK] = INPUT_KEY_CAPS_LOCK,
	[GLFW_KEY_SCROLL_LOCK] = INPUT_KEY_SCROLL_LOCK,
	[GLFW_KEY_NUM_LOCK] = INPUT_KEY_NUM_LOCK,
	[GLFW_KEY_PRINT_SCREEN] = INPUT_KEY_PRINT_SCREEN,
	[GLFW_KEY_PAUSE] = INPUT_KEY_PAUSE,
	[GLFW_KEY_F1] = INPUT_KEY_F1,
	[GLFW_KEY_F2] = INPUT_KEY_F2,
	[GLFW_KEY_F3] = INPUT_KEY_F3,
	[GLFW_KEY_F4] = INPUT_KEY_F4,
	[GLFW_KEY_F5] = INPUT_KEY_F5,
	[GLFW_KEY_F6] = INPUT_KEY_F6,
	[GLFW_KEY_F7] = INPUT_KEY_F7,
	[GLFW_KEY_F8] = INPUT_KEY_F8,
	[GLFW_KEY_F9] = INPUT_KEY_F9,
	[GLFW_KEY_F10] = INPUT_KEY_F10,
	[GLFW_KEY_F11] = INPUT_KEY_F11,
	[GLFW_KEY_F12] = INPUT_KEY_F12,
	[GLFW_KEY_F13] = INPUT_KEY_F13,
	[GLFW_KEY_F14] = INPUT_KEY_F14,
	[GLFW_KEY_F15] = INPUT_KEY_F15,
	[GLFW_KEY_F16] = INPUT_KEY_F16,
	[GLFW_KEY_F17] = INPUT_KEY_F17,
	[GLFW_KEY_F18] = INPUT_KEY_F18,
	[GLFW_KEY_F19] = INPUT_KEY_F19,
	[GLFW_KEY_F20] = INPUT_KEY_F20,
	[GLFW_KEY_F21] = INPUT_KEY_F21,
	[GLFW_KEY_F22] = INPUT_KEY_F22,
	[GLFW_KEY_F23] = INPUT_KEY_F23,
	[GLFW_KEY_F24] = INPUT_KEY_F24,
	[GLFW_KEY_F25] = INPUT_KEY_F25,
	[GLFW_KEY_KP_0] = INPUT_KEY_KP_0,
	[GLFW_KEY_KP_1] = INPUT_KEY_KP_1,
	[GLFW_KEY_KP_2] = INPUT_KEY_KP_2,
	[GLFW_KEY_KP_3] = INPUT_KEY_KP_3,
	[GLFW_KEY_KP_4] = INPUT_KEY_KP_4,
	[GLFW_KEY_KP_5] = INPUT_KEY_KP_5,
	[GLFW_KEY_KP_6] = INPUT_KEY_KP_6,
	[GLFW_KEY_KP_7] = INPUT_KEY_KP_7,
	[GLFW_KEY_KP_8] = INPUT_KEY_KP_8,
	[GLFW_KEY_KP_9] = INPUT_KEY_KP_9,
	[GLFW_KEY_KP_DECIMAL] = INPUT_KEY_KP_DEC,
	[GLFW_KEY_KP_DIVIDE] = INPUT_KEY_KP_DIV,
	[GLFW_KEY_KP_MULTIPLY] = INPUT_KEY_KP_MUL,
	[GLFW_KEY_KP_SUBTRACT] = INPUT_KEY_KP_SUB,
	[GLFW_KEY_KP_ADD] = INPUT_KEY_KP_ADD,
	[GLFW_KEY_KP_ENTER] = INPUT_KEY_KP_ENTER,
	[GLFW_KEY_KP_EQUAL] = INPUT_KEY_KP_EQ,
	[GLFW_KEY_LEFT_SHIFT] = INPUT_KEY_LEFT_SHIFT,
	[GLFW_KEY_LEFT_CONTROL] = INPUT_KEY_LEFT_CONTROL,
	[GLFW_KEY_LEFT_ALT] = INPUT_KEY_LEFT_ALT,
	[GLFW_KEY_LEFT_SUPER] = INPUT_KEY_LEFT_SUPER,
	[GLFW_KEY_RIGHT_SHIFT] = INPUT_KEY_RIGHT_SHIFT,
	[GLFW_KEY_RIGHT_CONTROL] = INPUT_KEY_RIGHT_CONTROL,
	[GLFW_KEY_RIGHT_ALT] = INPUT_KEY_RIGHT_ALT,
	[GLFW_KEY_RIGHT_SUPER] = INPUT_KEY_RIGHT_SUPER,
	[GLFW_KEY_MENU] = INPUT_KEY_MENU
};

static input_mouse_btn_t glfw_mouse_btn_code_to_edge[] = {
	[GLFW_MOUSE_BUTTON_1] = INPUT_MOUSE_BTN_LEFT,
	[GLFW_MOUSE_BUTTON_2] = INPUT_MOUSE_BTN_RIGHT,
	[GLFW_MOUSE_BUTTON_3] = INPUT_MOUSE_BTN_MIDDLE,
	[GLFW_MOUSE_BUTTON_4] = INPUT_MOUSE_BTN_4,
	[GLFW_MOUSE_BUTTON_5] = INPUT_MOUSE_BTN_5,
	[GLFW_MOUSE_BUTTON_6] = INPUT_MOUSE_BTN_6,
	[GLFW_MOUSE_BUTTON_7] = INPUT_MOUSE_BTN_7,
	[GLFW_MOUSE_BUTTON_8] = INPUT_MOUSE_BTN_8
};

static input_pad_btn_t glfw_pad_btn_code_to_edge[] = {
	[GLFW_GAMEPAD_BUTTON_A] = INPUT_PAD_A,
	[GLFW_GAMEPAD_BUTTON_B] = INPUT_PAD_B,
	[GLFW_GAMEPAD_BUTTON_X] = INPUT_PAD_X,
	[GLFW_GAMEPAD_BUTTON_Y] = INPUT_PAD_Y,
	[GLFW_GAMEPAD_BUTTON_LEFT_BUMPER] = INPUT_PAD_BUMPER_LEFT,
	[GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER] = INPUT_PAD_BUMPER_RIGHT,
	[GLFW_GAMEPAD_BUTTON_BACK] = INPUT_PAD_BACK,
	[GLFW_GAMEPAD_BUTTON_START] = INPUT_PAD_START,
	[GLFW_GAMEPAD_BUTTON_GUIDE] = INPUT_PAD_GUIDE,
	[GLFW_GAMEPAD_BUTTON_LEFT_THUMB] = INPUT_PAD_THUMB_LEFT,
	[GLFW_GAMEPAD_BUTTON_RIGHT_THUMB] = INPUT_PAD_THUMB_RIGHT,
	[GLFW_GAMEPAD_BUTTON_DPAD_UP] = INPUT_PAD_DPAD_UP,
	[GLFW_GAMEPAD_BUTTON_DPAD_RIGHT] = INPUT_PAD_DPAD_RIGHT,
	[GLFW_GAMEPAD_BUTTON_DPAD_DOWN] = INPUT_PAD_DPAD_DOWN,
	[GLFW_GAMEPAD_BUTTON_DPAD_LEFT] = INPUT_PAD_DPAD_LEFT
};

static void* glfw_allocate_cb(size_t size, void* user) {
	if (!user) {
		return NULL;
	}

	edge_allocator_t* alloc = (edge_allocator_t*)user;
	return edge_allocator_malloc(alloc, size);
}

static void* glfw_reallocate_cb(void* block, size_t size, void* user) {
	if (!user) {
		return NULL;
	}

	edge_allocator_t* alloc = (edge_allocator_t*)user;
	return edge_allocator_realloc(alloc, block, size);
}

static void glfw_deallocate_cb(void* block, void* user) {
	if (!user) {
		return;
	}

	edge_allocator_t* alloc = (edge_allocator_t*)user;
	edge_allocator_free(alloc, block);
}

static void window_error_cb(int error, const char* description) {
	EDGE_LOG_ERROR("GLFW error: %d. %s.", error, description);
}

static int glfw_init_counter = 0;

static void init_glfw_context(edge_allocator_t* alloc) {
	if (glfw_init_counter <= 0) {
		GLFWallocator glfw_allocator;
		glfw_allocator.allocate = glfw_allocate_cb;
		glfw_allocator.reallocate = glfw_reallocate_cb;
		glfw_allocator.deallocate = glfw_deallocate_cb;
		glfw_allocator.user = alloc;

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

	glfwSetErrorCallback(NULL);
}

static void window_close_cb(GLFWwindow* window) {
	platform_context_t* ctx = (platform_context_t*)glfwGetWindowUserPointer(window);
	if (!ctx) {
		return;
	}
	// TODO: DISPATCH EVENTS
}

static void window_size_cb(GLFWwindow* window, int width, int height) {
	platform_context_t* ctx = (platform_context_t*)glfwGetWindowUserPointer(window);
	if (!ctx) {
		return;
	}
	// TODO: DISPATCH EVENTS
}

static void window_focus_cb(GLFWwindow* window, int focused) {
	platform_context_t* ctx = (platform_context_t*)glfwGetWindowUserPointer(window);
	if (!ctx) {
		return;
	}
	// TODO: DISPATCH EVENTS
}

static void window_key_cb(GLFWwindow* window, int key, int scancode, int action, int mods) {
	platform_context_t* ctx = (platform_context_t*)glfwGetWindowUserPointer(window);
	if (!ctx) {
		return;
	}

	input_update_keyboard_state(&ctx->input_state, ctx->event_dispatcher, glfw_key_code_to_edge[key], action == GLFW_PRESS ? INPUT_KEY_ACTION_DOWN : INPUT_KEY_ACTION_UP);
}

static void window_cursor_cb(GLFWwindow* window, double xpos, double ypos) {
	platform_context_t* ctx = (platform_context_t*)glfwGetWindowUserPointer(window);
	if (!ctx) {
		return;
	}
	
	input_update_mouse_move_state(&ctx->input_state, ctx->event_dispatcher, (float)xpos, (float)ypos);
}

static void mouse_button_cb(GLFWwindow* window, int button, int action, int mods) {
	platform_context_t* ctx = (platform_context_t*)glfwGetWindowUserPointer(window);
	if (!ctx) {
		return;
	}
	
	input_update_mouse_btn_state(&ctx->input_state, ctx->event_dispatcher, glfw_mouse_btn_code_to_edge[button], action == GLFW_PRESS ? INPUT_KEY_ACTION_DOWN : INPUT_KEY_ACTION_UP);
}

static void mouse_scroll_cb(GLFWwindow* window, double xoffset, double yoffset) {
	platform_context_t* ctx = (platform_context_t*)glfwGetWindowUserPointer(window);
	if (!ctx) {
		return;
	}

	input_mouse_scroll_event_t evt;
	input_mouse_scroll_event_init(&evt, (float)xoffset, (float)yoffset);

	event_dispatcher_dispatch(ctx->event_dispatcher, (event_header_t*)&evt);
}

static void character_input_cb(GLFWwindow* window, uint32_t codepoint) {
	platform_context_t* ctx = (platform_context_t*)glfwGetWindowUserPointer(window);
	if (!ctx) {
		return;
	}

	input_text_input_event_t evt;
	input_text_input_event_init(&evt, codepoint);
	
	event_dispatcher_dispatch(ctx->event_dispatcher, (event_header_t*)&evt);
}

static void gamepad_connected_cb(int jid, int event) {
	const char* guid = glfwGetJoystickGUID(jid);

	int vendor_id = 0, product_id = 0;
	sscanf(guid, "%04x%04x", &vendor_id, &product_id);

	input_pad_connection_event_t evt;
	input_pad_connection_event_init(&evt, 
		jid, 
		vendor_id,
		product_id,
		0, 
		event == GLFW_CONNECTED,
		glfwGetJoystickName(jid)
	);

	event_dispatcher_dispatch(g_event_dispatcher, (event_header_t*)&evt);
}

platform_context_t* platform_context_create(platform_context_create_info_t* create_info) {
	if (!create_info || !create_info->alloc || !create_info->layout) {
		return NULL;
	}

	platform_context_t* ctx = (platform_context_t*)edge_allocator_calloc(create_info->alloc, 1, sizeof(platform_context_t));
	if (!ctx) {
		return NULL;
	}

	memset(&ctx->input_state, 0, sizeof(input_state_t));

	ctx->alloc = create_info->alloc;
	ctx->layout = create_info->layout;
	ctx->event_dispatcher = create_info->event_dispatcher;
	ctx->wnd = NULL;

	g_event_dispatcher = create_info->event_dispatcher;

#if EDGE_DEBUG
	if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
		if (!AllocConsole()) {
			EDGE_LOG_DEBUG("Failed to allocate console.");
		}
	}

	FILE* fp;
	freopen_s(&fp, "conin$", "r", stdin);
	freopen_s(&fp, "conout$", "w", stdout);
	freopen_s(&fp, "conout$", "w", stderr);
#endif

	edge_logger_t* logger = edge_logger_get_global();
	edge_logger_output_t* debug_output = edge_logger_create_debug_console_output(create_info->alloc, EDGE_LOG_FORMAT_DEFAULT);
	edge_logger_add_output(logger, debug_output);

	return ctx;
}

void platform_context_destroy(platform_context_t* ctx) {
	if (!ctx) {
		return;
	}

	// Destroy window if needed
	if (ctx->wnd) {
		struct edge_window* wnd = ctx->wnd;
		glfwSetWindowShouldClose(wnd->handle, GLFW_TRUE);
		glfwDestroyWindow(wnd->handle);

		edge_allocator_free(ctx->alloc, wnd);
		deinit_glfw_context();
	}

	g_event_dispatcher = NULL;

	edge_allocator_t* alloc = ctx->alloc;
	edge_allocator_free(alloc, ctx);
}

void platform_context_get_surface(platform_context_t* ctx, void* surface_info) {
	if (!ctx || !surface_info) {
		return;
	}

	VkWin32SurfaceCreateInfoKHR* create_info = (VkWin32SurfaceCreateInfoKHR*)surface_info;
	create_info->sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	create_info->pNext = NULL;
	create_info->flags = 0;
	create_info->hinstance = ctx->layout->hinst;
	create_info->hwnd = glfwGetWin32Window(ctx->wnd->handle);
}

bool platform_context_window_init(platform_context_t* ctx, window_create_info_t* create_info) {
	if (!ctx) {
		return false;
	}

	init_glfw_context(ctx->alloc);

	struct edge_window* wnd = (struct edge_window*)edge_allocator_malloc(ctx->alloc, sizeof(struct edge_window));
	if (!wnd) {
		deinit_glfw_context();
		return NULL;
	}

	wnd->mode = create_info->mode;
	wnd->resizable = create_info->resizable;
	wnd->vsync_mode = create_info->vsync_mode;
	wnd->should_close = false;

	switch (create_info->mode)
	{
	case WINDOW_MODE_FULLSCREEN: {
		GLFWmonitor* monitor = glfwGetPrimaryMonitor();
		const GLFWvidmode* mode = glfwGetVideoMode(monitor);
		wnd->handle = glfwCreateWindow(mode->width, mode->height, create_info->title, monitor, NULL);
		break;
	}
	case WINDOW_MODE_FULLSCREEN_BORDERLESS: {
		GLFWmonitor* monitor = glfwGetPrimaryMonitor();
		const GLFWvidmode* mode = glfwGetVideoMode(monitor);

		glfwWindowHint(GLFW_RED_BITS, mode->redBits);
		glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
		glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
		glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);

		wnd->handle = glfwCreateWindow(mode->width, mode->height, create_info->title, monitor, NULL);
		break;
	}
	default: {
		wnd->handle = glfwCreateWindow(create_info->width, create_info->height, create_info->title, NULL, NULL);
		break;
	}
	}

	if (!wnd->handle) {
		edge_allocator_free(ctx->alloc, wnd);
		deinit_glfw_context();
		return NULL;
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

	glfwSetWindowUserPointer(wnd->handle, ctx);

	ctx->wnd = wnd;

	return true;
}

bool platform_context_window_should_close(platform_context_t* ctx) {
    if (!ctx || !ctx->wnd) {
        return false;
    }

    return ctx->wnd->should_close;
}

void platform_context_window_process_events(platform_context_t* ctx, float delta_time) {
	if (!ctx || !ctx->wnd || !ctx->wnd->handle) {
		return;
	}

	struct edge_window* wnd = ctx->wnd;

	glfwPollEvents();
	wnd->should_close = glfwWindowShouldClose(wnd->handle);

	for (int jid = 0; jid < GLFW_JOYSTICK_LAST; ++jid) {
		GLFWgamepadstate state;
		if (glfwGetGamepadState(jid, &state)) {
			for (int btn = 0; btn < GLFW_GAMEPAD_BUTTON_LAST + 1; ++btn) {
				input_update_pad_btn_state(&ctx->input_state, ctx->event_dispatcher, jid, glfw_pad_btn_code_to_edge[btn], (input_key_action_t)state.buttons[btn]);
			}

			input_update_pad_axis_state(&ctx->input_state, ctx->event_dispatcher, jid, INPUT_PAD_AXIS_STICK_LEFT, state.axes[GLFW_GAMEPAD_AXIS_LEFT_X], state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y], 0.0f);
			input_update_pad_axis_state(&ctx->input_state, ctx->event_dispatcher, jid, INPUT_PAD_AXIS_STICK_RIGHT, state.axes[GLFW_GAMEPAD_AXIS_RIGHT_X], state.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y], 0.0f);
			input_update_pad_axis_state(&ctx->input_state, ctx->event_dispatcher, jid, INPUT_PAD_AXIS_TRIGGER_LEFT, state.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER], 0.0f, 0.0f);
			input_update_pad_axis_state(&ctx->input_state, ctx->event_dispatcher, jid, INPUT_PAD_AXIS_TRIGGER_RIGHT, state.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER], 0.0f, 0.0f);
		}
	}
}

void platform_context_window_show(platform_context_t* ctx) {
	if (!ctx || !ctx->wnd || !ctx->wnd->handle) {
		return;
	}

	glfwShowWindow(ctx->wnd->handle);
}

void platform_context_window_hide(platform_context_t* ctx) {
	if (!ctx || !ctx->wnd || !ctx->wnd->handle) {
		return;
	}

	glfwHideWindow(ctx->wnd->handle);
}

void platform_context_window_set_title(platform_context_t* ctx, const char* title) {
	if (!ctx || !ctx->wnd || !ctx->wnd->handle || !title) {
		return;
	}

	glfwSetWindowTitle(ctx->wnd->handle, title);
}

float platform_context_window_dpi_scale_factor(platform_context_t* ctx) {
	if (!ctx || !ctx->wnd || !ctx->wnd->handle) {
		return 1.0f;
	}

	GLFWmonitor* primary_monitor = glfwGetPrimaryMonitor();
	const GLFWvidmode* vidmode = glfwGetVideoMode(primary_monitor);

	int width_mm, height_mm;
	glfwGetMonitorPhysicalSize(primary_monitor, &width_mm, &height_mm);

	// As suggested by the GLFW monitor guide
	const float inch_to_mm = 25.0f;
	const float win_base_density = 96.0f;

	float dpi = (float)(vidmode->width / (width_mm / inch_to_mm));
	return dpi / win_base_density;
}

float platform_context_window_content_scale_factor(platform_context_t* ctx) {
	if (!ctx || !ctx->wnd || !ctx->wnd->handle) {
		return 1.0f;
	}

	int fb_width, fb_height;
	glfwGetFramebufferSize(ctx->wnd->handle, &fb_width, &fb_height);
	int win_width, win_height;
	glfwGetWindowSize(ctx->wnd->handle, &win_width, &win_height);

	// Needed for ui scale
	return (float)(fb_width) / win_width;
}

int APIENTRY WinMain(HINSTANCE hinst, HINSTANCE prev_hinst, PSTR cmd_line, INT cmd_show) {
	platform_layout_t platform_layout = { 0 };
	platform_layout.hinst = hinst;
	platform_layout.prev_hinst = prev_hinst;
	platform_layout.cmd_line = cmd_line;
	platform_layout.cmd_show = cmd_show;

	return edge_main(&platform_layout);
}