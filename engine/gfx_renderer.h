#ifndef GFX_RENDERER_H
#define GFX_RENDERER_H

#ifdef __cplusplus
extern "C" {
#endif

	typedef struct edge_allocator edge_allocator_t;

	typedef struct gfx_context gfx_context_t;
	typedef struct gfx_renderer gfx_renderer_t;

	typedef struct {
		const edge_allocator_t* alloc;
		const gfx_context_t* ctx;
		const gfx_queue_t* main_queue;
	} gfx_renderer_create_info_t;

	gfx_renderer_t* gfx_renderer_create(const edge_allocator_t* alloc, const gfx_context_t* gfx_context);
	void gfx_renderer_destroy(gfx_renderer_t* renderer);

#ifdef __cplusplus
}
#endif

#endif // GFX_RENDERER_H