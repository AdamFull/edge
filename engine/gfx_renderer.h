#ifndef GFX_RENDERER_H
#define GFX_RENDERER_H

#include "gfx_interface.h"
#include <handle_pool.hpp>

namespace edge::gfx {
	enum class ResourceType {
		Unknown = -1,
		Image,
		Buffer
	};

	struct RendererCreateInfo {
		const Allocator* alloc;
		const Queue* main_queue;
	};

	struct Resource;
	struct Renderer;

	Renderer* renderer_create(const RendererCreateInfo* create_info);
	void renderer_destroy(Renderer* renderer);

	Handle renderer_add_resource(Renderer* renderer);
	bool renderer_setup_image_resource(Renderer* renderer, Handle handle, const Image* resource);
	bool renderer_setup_buffer_resource(Renderer* renderer, Handle handle, const Buffer* resource);
	void renderer_free_resource(Renderer* renderer, Handle handle);

	bool renderer_frame_begin(Renderer* renderer);
	bool renderer_frame_end(Renderer* renderer);
}

#endif // GFX_RENDERER_H