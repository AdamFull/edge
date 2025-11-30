#ifndef GFX_RENDERER_H
#define GFX_RENDERER_H

#include "gfx_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

	typedef struct edge_allocator edge_allocator_t;

	typedef struct gfx_renderer gfx_renderer_t;

	typedef struct {
		const edge_allocator_t* alloc;
		const gfx_queue_t* main_queue;
	} gfx_renderer_create_info_t;

	gfx_renderer_t* gfx_renderer_create(const gfx_renderer_create_info_t* create_info);
	void gfx_renderer_destroy(gfx_renderer_t* renderer);

	bool gfx_renderer_frame_begin(gfx_renderer_t* renderer);
	bool gfx_renderer_frame_end(gfx_renderer_t* renderer);

#ifdef __cplusplus
}
#endif

#endif // GFX_RENDERER_H