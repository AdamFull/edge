#ifndef GFX_CONTEXT_H
#define GFX_CONTEXT_H

#include "gfx_enum.h"

#include <vulkan/vulkan.h>
#include <volk.h>
#include <vk_mem_alloc.h>

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

	typedef struct edge_allocator edge_allocator_t;
	typedef struct platform_context platform_context_t;

	typedef struct gfx_context gfx_context_t;

	gfx_context_t* gfx_context_create(const edge_allocator_t* alloc, platform_context_t* platform_ctx);
	void gfx_context_destroy(gfx_context_t* context);

#ifdef __cplusplus
}
#endif

#endif // GFX_CONTEXT_H