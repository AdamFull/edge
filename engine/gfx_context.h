#ifndef GFX_CONTEXT_H
#define GFX_CONTEXT_H

#include "gfx_interface.h"

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

	typedef struct {
		const edge_allocator_t* alloc;
		platform_context_t* platform_context;
	} gfx_context_create_info_t;

	typedef struct {
		gfx_queue_caps_flags_t required_caps;
		gfx_queue_caps_flags_t preferred_caps;
		gfx_queue_selection_strategy_t strategy;
		bool prefer_separate_family;
	} gfx_queue_request_t;

	bool gfx_context_init(const gfx_context_create_info_t* cteate_info);
	void gfx_context_shutdown();

	bool gfx_get_queue(const gfx_queue_request_t* create_info, gfx_queue_t* queue);
	void gfx_release_queue(gfx_queue_t* queue);

	bool gfx_command_pool_create(const gfx_queue_t* queue, gfx_command_pool_t* cmd_pool);
	void gfx_command_pool_destroy(gfx_command_pool_t* command_pool);

	gfx_fence_t* gfx_fence_create(VkFenceCreateFlags flags);
	void gfx_fence_wait(const gfx_fence_t* fence, uint64_t timeout);
	void gfx_fence_destroy(gfx_fence_t* fence);

#ifdef __cplusplus
}
#endif

#endif // GFX_CONTEXT_H