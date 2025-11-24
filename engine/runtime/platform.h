#ifndef PLATFORM_CONTEXT_H
#define PLATFORM_CONTEXT_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

	typedef struct edge_allocator edge_allocator_t;
	typedef struct event_dispatcher event_dispatcher_t;

	typedef struct platform_layout platform_layout_t;
	typedef struct platform_context platform_context_t;

	typedef struct platform_context_create_info {
		edge_allocator_t* alloc;
		platform_layout_t* layout;
		event_dispatcher_t* event_dispatcher;
	} platform_context_create_info_t;

	typedef enum window_mode {
		WINDOW_MODE_FULLSCREEN,
		WINDOW_MODE_FULLSCREEN_BORDERLESS,
		WINDOW_MODE_DEFAULT
	} window_mode_t;

	typedef enum window_vsync_mode {
		WINDOW_VSYNC_MODE_OFF,
		WINDOW_VSYNC_MODE_ON,
		WINDOW_VSYNC_MODE_DEFAULT
	} window_vsync_mode_t;

	typedef struct window_create_info {
		const char* title;
		window_mode_t mode;
		bool resizable;
		window_vsync_mode_t vsync_mode;

		int width;
		int height;
	} window_create_info_t;

	platform_context_t* platform_context_create(platform_context_create_info_t* create_info);
	void platform_context_destroy(platform_context_t* ctx);

	void platform_context_get_surface(platform_context_t* ctx, void* surface_info);

	bool platform_context_window_init(platform_context_t* ctx, window_create_info_t* create_info);

    bool platform_context_window_should_close(platform_context_t* ctx);
	void platform_context_window_process_events(platform_context_t* ctx, float delta_time);

	void platform_context_window_show(platform_context_t* ctx);
	void platform_context_window_hide(platform_context_t* ctx);

	void platform_context_window_set_title(platform_context_t* ctx, const char* title);

	float platform_context_window_dpi_scale_factor(platform_context_t* ctx);
	float platform_context_window_content_scale_factor(platform_context_t* ctx);

#ifdef __cplusplus
}
#endif

#endif //PLATFORM_CONTEXT_H