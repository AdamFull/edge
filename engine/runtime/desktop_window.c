#include "platform.h"

#include <math.h>
#include <string.h>

#include <edge_allocator.h>
#include <edge_logger.h>

#include <GLFW/glfw3.h>

struct edge_window {
	edge_allocator_t* allocator;
	GLFWwindow* handle;

	window_mode_t mode;
	bool resizable;
	window_vsync_mode_t vsync_mode;

	bool should_close;

	GLFWgamepadstate gamepad_states[GLFW_JOYSTICK_LAST];
};

static void window_error_cb(int error, const char* description) {
	EDGE_LOG_ERROR("GLFW error: %d. %s.", error, description);
}

static void window_close_cb(GLFWwindow* window) {
	edge_window_t* edge_window = (edge_window_t*)glfwGetWindowUserPointer(window);
	if (!edge_window) {
		return;
	}
	// TODO: DISPATCH EVENTS
}

static void window_size_cb(GLFWwindow* window, int width, int height) {
	edge_window_t* edge_window = (edge_window_t*)glfwGetWindowUserPointer(window);
	if (!edge_window) {
		return;
	}
	// TODO: DISPATCH EVENTS
}

static void window_focus_cb(GLFWwindow* window, int focused) {
	edge_window_t* edge_window = (edge_window_t*)glfwGetWindowUserPointer(window);
	if (!edge_window) {
		return;
	}
	// TODO: DISPATCH EVENTS
}

static void window_key_cb(GLFWwindow* window, int key, int scancode, int action, int mods) {
	// TODO: DISPATCH EVENTS
}

static void window_cursor_cb(GLFWwindow* window, double xpos, double ypos) {
	// TODO: DISPATCH EVENTS
}

static void mouse_button_cb(GLFWwindow* window, int button, int action, int mods) {
	// TODO: DISPATCH EVENTS
}

static void mouse_scroll_cb(GLFWwindow* window, double xoffset, double yoffset) {
	// TODO: DISPATCH EVENTS
}

static void character_input_cb(GLFWwindow* window, uint32_t codepoint) {
	// TODO: DISPATCH EVENTS
}

static void gamepad_connected_cb(int jid, int event) {
	// TODO: DISPATCH EVENTS
}


static void* glfw_allocate(size_t size, void* user) {
	if (!user) {
		return NULL;
	}

	edge_allocator_t* alloc = (edge_allocator_t*)user;
	return edge_allocator_malloc(alloc, size);
}

static void* glfw_reallocate(void* block, size_t size, void* user) {
	if (!user) {
		return NULL;
	}

	edge_allocator_t* alloc = (edge_allocator_t*)user;
	return edge_allocator_realloc(alloc, block, size);
}

static void glfw_deallocate(void* block, void* user) {
	if (!user) {
		return;
	}

	edge_allocator_t* alloc = (edge_allocator_t*)user;
	edge_allocator_free(alloc, block);
}

static int glfw_init_counter = 0;

static void init_glfw_context(edge_allocator_t* allocator) {
	if (glfw_init_counter <= 0) {
		GLFWallocator glfw_allocator;
		glfw_allocator.allocate = glfw_allocate;
		glfw_allocator.reallocate = glfw_reallocate;
		glfw_allocator.deallocate = glfw_deallocate;
		glfw_allocator.user = allocator;

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

edge_window_t* edge_window_create(edge_window_create_info_t* create_info) {
	if (!create_info || !create_info->allocator || !create_info->platform_layout) {
		return NULL;
	}

	edge_window_t* window = (edge_window_t*)edge_allocator_malloc(create_info->allocator, sizeof(edge_window_t));
	if (!window) {
		return NULL;
	}

	init_glfw_context(create_info->allocator);

	window->allocator = create_info->allocator;
	window->mode = create_info->mode;
	window->resizable = create_info->resizable;
	window->vsync_mode = create_info->vsync_mode;
	window->should_close = false;

	switch (create_info->mode)
	{
	case EDGE_WINDOW_MODE_FULLSCREEN: {
		GLFWmonitor* monitor = glfwGetPrimaryMonitor();
		const GLFWvidmode* mode = glfwGetVideoMode(monitor);
		window->handle = glfwCreateWindow(mode->width, mode->height, create_info->title, monitor, NULL);
		break;
	}
	case EDGE_WINDOW_MODE_FULLSCREEN_BORDERLESS: {
		GLFWmonitor* monitor = glfwGetPrimaryMonitor();
		const GLFWvidmode* mode = glfwGetVideoMode(monitor);

		glfwWindowHint(GLFW_RED_BITS, mode->redBits);
		glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
		glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
		glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);

		window->handle = glfwCreateWindow(mode->width, mode->height, create_info->title, monitor, NULL);
		break;
	}
	default: {
		window->handle = glfwCreateWindow(create_info->width, create_info->height, create_info->title, NULL, NULL);
		break;
	}
	}

	if (!window->handle) {
		edge_allocator_free(create_info->allocator, window);
		deinit_glfw_context();
		return NULL;
	}

	glfwSetWindowCloseCallback(window->handle, window_close_cb);
	glfwSetWindowSizeCallback(window->handle, window_size_cb);
	glfwSetWindowFocusCallback(window->handle, window_focus_cb);

	// For input
	glfwSetKeyCallback(window->handle, window_key_cb);
	glfwSetCursorPosCallback(window->handle, window_cursor_cb);
	glfwSetMouseButtonCallback(window->handle, mouse_button_cb);
	glfwSetScrollCallback(window->handle, mouse_scroll_cb);
	glfwSetCharCallback(window->handle, character_input_cb);
	glfwSetJoystickCallback(gamepad_connected_cb);

	glfwSetInputMode(window->handle, GLFW_STICKY_KEYS, 1);
	glfwSetInputMode(window->handle, GLFW_STICKY_MOUSE_BUTTONS, 1);

	glfwSetWindowUserPointer(window->handle, window);

	return window;
}

void edge_window_destroy(edge_window_t* window) {
	if (!window) {
		return;
	}

	if (window->handle) {
		glfwSetWindowShouldClose(window->handle, GLFW_TRUE);
		glfwDestroyWindow(window->handle);
	}

	edge_allocator_t* alloc = window->allocator;
	edge_allocator_free(alloc, window);

	deinit_glfw_context();
}

void edge_window_process_events(edge_window_t* window, float delta_time) {
	if (!window) {
		return;
	}

	glfwPollEvents();
	window->should_close = glfwWindowShouldClose(window->handle);

	const float axis_threshold = 0.01f;

	for (int jid = 0; jid < GLFW_JOYSTICK_LAST; ++jid) {
		GLFWgamepadstate state;
		if (glfwGetGamepadState(jid, &state)) {
			GLFWgamepadstate* prev_state = &window->gamepad_states[jid];

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
			memset(&window->gamepad_states[jid], 0, sizeof(GLFWgamepadstate));
		}
	}
}

void edge_window_show(edge_window_t* window) {
	if (!window) {
		return;
	}

	glfwShowWindow(window->handle);
}

void edge_window_hide(edge_window_t* window) {
	if (!window) {
		return;
	}

	glfwHideWindow(window->handle);
}

void edge_window_set_title(edge_window_t* window, const char* title) {
	if (!window || !title) {
		return;
	}

	glfwSetWindowTitle(window->handle, title);
}

float edge_window_dpi_scale_factor(edge_window_t* window) {
	if (!window) {
		return 1.0f;
	}

	// TODO: select current active monitor
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

float edge_window_content_scale_factor(edge_window_t* window) {
	if (!window) {
		return 1.0f;
	}

	int fb_width, fb_height;
	glfwGetFramebufferSize(window->handle, &fb_width, &fb_height);
	int win_width, win_height;
	glfwGetWindowSize(window->handle, &win_width, &win_height);

	// Needed for ui scale
	return (float)(fb_width) / win_width;
}