#ifndef EDGE_PLATFORM_H
#define EDGE_PLATFORM_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

	typedef struct edge_allocator edge_allocator_t;

	typedef struct edge_platform_layout edge_platform_layout_t;
	typedef struct edge_platform_context edge_platform_context_t;

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
		const char* title;
		window_mode_t mode;
		bool resizable;
		window_vsync_mode_t vsync_mode;

		int width;
		int height;
	} edge_window_create_info_t;

	edge_platform_context_t* edge_platform_create(edge_allocator_t* allocator, edge_platform_layout_t* layout);
	void edge_platform_destroy(edge_platform_context_t* ctx);

	bool edge_platform_window_init(edge_platform_context_t* ctx, edge_window_create_info_t* create_info);

    bool edge_platform_window_should_close(edge_platform_context_t* ctx);
	void edge_platform_window_process_events(edge_platform_context_t* ctx, float delta_time);

	void edge_platform_window_show(edge_platform_context_t* ctx);
	void edge_platform_window_hide(edge_platform_context_t* ctx);

	void edge_platform_window_set_title(edge_platform_context_t* ctx, const char* title);

	float edge_platform_window_dpi_scale_factor(edge_platform_context_t* ctx);
	float edge_platform_window_content_scale_factor(edge_platform_context_t* ctx);

#ifdef __cplusplus
}
#endif

#endif //EDGE_PLATFORM_H