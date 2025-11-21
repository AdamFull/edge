#include "platform.h"

#include <edge_allocator.h>
#include <edge_logger.h>

#include <math.h>

// TODO: GLFW is common between windows and linux
#include <GLFW/glfw3.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>

extern int edge_main(edge_platform_layout_t* platform_layout);

struct edge_platform_layout {
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

	GLFWgamepadstate gamepad_states[GLFW_JOYSTICK_LAST];
};

struct edge_platform_context {
	edge_allocator_t* alloc;
	edge_platform_layout_t* layout;

	struct edge_window* wnd;
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
	edge_platform_context_t* ctx = (edge_platform_context_t*)glfwGetWindowUserPointer(window);
	if (!ctx) {
		return;
	}
	// TODO: DISPATCH EVENTS
}

static void window_size_cb(GLFWwindow* window, int width, int height) {
	edge_platform_context_t* ctx = (edge_platform_context_t*)glfwGetWindowUserPointer(window);
	if (!ctx) {
		return;
	}
	// TODO: DISPATCH EVENTS
}

static void window_focus_cb(GLFWwindow* window, int focused) {
	edge_platform_context_t* ctx = (edge_platform_context_t*)glfwGetWindowUserPointer(window);
	if (!ctx) {
		return;
	}
	// TODO: DISPATCH EVENTS
}

static void window_key_cb(GLFWwindow* window, int key, int scancode, int action, int mods) {
	edge_platform_context_t* ctx = (edge_platform_context_t*)glfwGetWindowUserPointer(window);
	if (!ctx) {
		return;
	}
	// TODO: DISPATCH EVENTS
}

static void window_cursor_cb(GLFWwindow* window, double xpos, double ypos) {
	edge_platform_context_t* ctx = (edge_platform_context_t*)glfwGetWindowUserPointer(window);
	if (!ctx) {
		return;
	}
	// TODO: DISPATCH EVENTS
}

static void mouse_button_cb(GLFWwindow* window, int button, int action, int mods) {
	edge_platform_context_t* ctx = (edge_platform_context_t*)glfwGetWindowUserPointer(window);
	if (!ctx) {
		return;
	}
	// TODO: DISPATCH EVENTS
}

static void mouse_scroll_cb(GLFWwindow* window, double xoffset, double yoffset) {
	edge_platform_context_t* ctx = (edge_platform_context_t*)glfwGetWindowUserPointer(window);
	if (!ctx) {
		return;
	}
	// TODO: DISPATCH EVENTS
}

static void character_input_cb(GLFWwindow* window, uint32_t codepoint) {
	edge_platform_context_t* ctx = (edge_platform_context_t*)glfwGetWindowUserPointer(window);
	if (!ctx) {
		return;
	}
	// TODO: DISPATCH EVENTS
}

static void gamepad_connected_cb(int jid, int event) {
	// TODO: DISPATCH EVENTS
}

edge_platform_context_t* edge_platform_create(edge_allocator_t* alloc, edge_platform_layout_t* layout) {
	if (!alloc || !layout) {
		return NULL;
	}

	edge_platform_context_t* ctx = (edge_platform_context_t*)edge_allocator_calloc(alloc, 1, sizeof(edge_platform_context_t));
	if (!ctx) {
		return NULL;
	}

	ctx->alloc = alloc;
	ctx->layout = layout;
	ctx->wnd = NULL;

	// TODO: Init terminal

	edge_logger_t* logger = edge_logger_get_global();
	edge_logger_output_t* debug_output = edge_logger_create_debug_console_output(alloc, EDGE_LOG_FORMAT_DEFAULT);
	edge_logger_add_output(logger, debug_output);

	return ctx;
}

void edge_platform_destroy(edge_platform_context_t* ctx) {
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

	edge_allocator_t* alloc = ctx->alloc;
	edge_allocator_free(alloc, ctx);
}

bool edge_platform_window_init(edge_platform_context_t* ctx, edge_window_create_info_t* create_info) {
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
	case EDGE_WINDOW_MODE_FULLSCREEN: {
		GLFWmonitor* monitor = glfwGetPrimaryMonitor();
		const GLFWvidmode* mode = glfwGetVideoMode(monitor);
		wnd->handle = glfwCreateWindow(mode->width, mode->height, create_info->title, monitor, NULL);
		break;
	}
	case EDGE_WINDOW_MODE_FULLSCREEN_BORDERLESS: {
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

bool edge_platform_window_should_close(edge_platform_context_t* ctx) {
    if (!ctx || !ctx->wnd) {
        return false;
    }

    return ctx->wnd->should_close;
}

void edge_platform_window_process_events(edge_platform_context_t* ctx, float delta_time) {
	if (!ctx || !ctx->wnd || !ctx->wnd->handle) {
		return;
	}

	struct edge_window* wnd = ctx->wnd;

	glfwPollEvents();
	wnd->should_close = glfwWindowShouldClose(wnd->handle);

	const float axis_threshold = 0.01f;
	for (int jid = 0; jid < GLFW_JOYSTICK_LAST; ++jid) {
		GLFWgamepadstate state;
		if (glfwGetGamepadState(jid, &state)) {
			GLFWgamepadstate* prev_state = &wnd->gamepad_states[jid];

			for (int btn = 0; btn < GLFW_GAMEPAD_BUTTON_LAST + 1; ++btn) {
				bool current_state = state.buttons[btn] == GLFW_PRESS;
				bool prev_button_state = prev_state->buttons[btn] == GLFW_PRESS;

				if (current_state != prev_button_state) {
					// TODO: DISPATCH EVENT
				}
			}

			float left_x_diff = fabs(state.axes[GLFW_GAMEPAD_AXIS_LEFT_X] - prev_state->axes[GLFW_GAMEPAD_AXIS_LEFT_X]);
			float left_y_diff = fabs(state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y] - prev_state->axes[GLFW_GAMEPAD_AXIS_LEFT_Y]);
			if (left_x_diff > axis_threshold || left_y_diff > axis_threshold) {
				// TODO: DISPATCH EVENT
			}

			float right_x_diff = fabs(state.axes[GLFW_GAMEPAD_AXIS_RIGHT_X] - prev_state->axes[GLFW_GAMEPAD_AXIS_RIGHT_X]);
			float right_y_diff = fabs(state.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y] - prev_state->axes[GLFW_GAMEPAD_AXIS_RIGHT_Y]);
			if (right_x_diff > axis_threshold || right_y_diff > axis_threshold) {
				// TODO: DISPATCH EVENT
			}

			float left_trigger_diff = fabs(state.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER] - prev_state->axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER]);
			if (left_trigger_diff > axis_threshold) {
				// TODO: DISPATCH EVENT
			}

			float right_trigger_diff = fabs(state.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] - prev_state->axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER]);
			if (right_trigger_diff > axis_threshold) {
				// TODO: DISPATCH EVENT
			}

			*prev_state = state;
		}
		else {
			memset(&wnd->gamepad_states[jid], 0, sizeof(GLFWgamepadstate));
		}
	}
}

void edge_platform_window_show(edge_platform_context_t* ctx) {
	if (!ctx || !ctx->wnd || !ctx->wnd->handle) {
		return;
	}

	glfwShowWindow(ctx->wnd->handle);
}

void edge_platform_window_hide(edge_platform_context_t* ctx) {
	if (!ctx || !ctx->wnd || !ctx->wnd->handle) {
		return;
	}

	glfwHideWindow(ctx->wnd->handle);
}

void edge_platform_window_set_title(edge_platform_context_t* ctx, const char* title) {
	if (!ctx || !ctx->wnd || !ctx->wnd->handle || !title) {
		return;
	}

	glfwSetWindowTitle(ctx->wnd->handle, title);
}

float edge_platform_window_dpi_scale_factor(edge_platform_context_t* ctx) {
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

float edge_platform_window_content_scale_factor(edge_platform_context_t* ctx) {
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
	edge_platform_layout_t platform_layout = { 0 };
	platform_layout.hinst = hinst;
	platform_layout.prev_hinst = prev_hinst;
	platform_layout.cmd_line = cmd_line;
	platform_layout.cmd_show = cmd_show;

	return edge_main(&platform_layout);
}