#ifndef EDGE_PLATFORM_H
#define EDGE_PLATFORM_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

	typedef struct edge_allocator edge_allocator_t;

	typedef struct edge_platform_layout edge_platform_layout_t;
	typedef struct edge_window edge_window_t;
	typedef struct edge_input edge_input_t;

	typedef enum window_mode {
		EDGE_WINDOW_MODE_FULLSCREEN,
		EDGE_WINDOW_MODE_FULLSCREEN_BORDERLESS,
		EDGE_WINDOW_MODE_DEFAULT
	} window_mode_t;

	typedef enum window_vsync_mode {
		EDGE_WINDOW_VSYNC_MODE_OFF,
		EDGE_WINDOW_VSYNC_MODE_ON,
		EDGE_WINDOW_VSYNC_MODE_DEFAULT
	} window_vsync_mode_t;

	typedef struct edge_window_create_info {
		edge_allocator_t* allocator;
		edge_platform_layout_t* platform_layout;

		const char* title;
		window_mode_t mode;
		bool resizable;
		window_vsync_mode_t vsync_mode;

		int width;
		int height;
	} edge_window_create_info_t;

	edge_window_t* edge_window_create(edge_window_create_info_t* create_info);
	void edge_window_destroy(edge_window_t* window);

	void edge_window_process_events(edge_window_t* window, float delta_time);

	void edge_window_show(edge_window_t* window);
	void edge_window_hide(edge_window_t* window);

	void edge_window_set_title(edge_window_t* window, const char* title);

	float edge_window_dpi_scale_factor(edge_window_t* window);
	float edge_window_content_scale_factor(edge_window_t* window);

#ifdef __cplusplus
}
#endif

#endif //EDGE_PLATFORM_H