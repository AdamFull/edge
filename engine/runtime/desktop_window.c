#include "platform.h"

#include <edge_allocator.h>

#include <GLFW/glfw3.h>

struct edge_window {
	edge_allocator_t* allocator;
	GLFWwindow* handle;

	window_mode_t mode;
	bool resizable;
	window_vsync_mode_t vsync_mode;
};

static void window_error_cb(int error, const char* description) {
	// TODO: logger
}

static void window_close_cb(GLFWwindow* window) {
	edge_window_t* edge_window = (edge_window_t*)glfwGetWindowUserPointer(window);
	if (!edge_window) {
		return;
	}
}

static void window_size_cb(GLFWwindow* window, int width, int height) {
	edge_window_t* edge_window = (edge_window_t*)glfwGetWindowUserPointer(window);
	if (!edge_window) {
		return;
	}
}

static void window_focus_cb(GLFWwindow* window, int focused) {
	edge_window_t* edge_window = (edge_window_t*)glfwGetWindowUserPointer(window);
	if (!edge_window) {
		return;
	}
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