#ifndef GFX_RENDERER_H
#define GFX_RENDERER_H

#include "gfx_interface.h"
#include <edge_handle_pool.h>

#ifdef __cplusplus
extern "C" {
#endif

	typedef struct edge_allocator edge_allocator_t;

	typedef enum {
		GFX_RESOURCE_TYPE_UNKNOWN,
		GFX_RESOURCE_TYPE_IMAGE,
		GFX_RESOURCE_TYPE_BUFFER
	} gfx_resource_type_t;

	typedef struct gfx_renderer gfx_renderer_t;
	typedef struct gfx_resource gfx_resource_t;

	typedef struct {
		const edge_allocator_t* alloc;
		const gfx_queue_t* main_queue;
	} gfx_renderer_create_info_t;

	gfx_renderer_t* gfx_renderer_create(const gfx_renderer_create_info_t* create_info);
	void gfx_renderer_destroy(gfx_renderer_t* renderer);

	edge_handle_t gfx_renderer_add_resource(gfx_renderer_t* renderer);
	bool gfx_renderer_setup_image_resource(gfx_renderer_t* renderer, edge_handle_t handle, const gfx_image_t* resource);
	bool gfx_renderer_setup_buffer_resource(gfx_renderer_t* renderer, edge_handle_t handle, const gfx_buffer_t* resource);
	void gfx_renderer_free_resource(gfx_renderer_t* renderer, edge_handle_t handle);

	bool gfx_renderer_frame_begin(gfx_renderer_t* renderer);
	bool gfx_renderer_frame_end(gfx_renderer_t* renderer);

#ifdef __cplusplus
}
#endif

#endif // GFX_RENDERER_H