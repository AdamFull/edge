#ifndef GFX_CONTEXT_H
#define GFX_CONTEXT_H

#include "gfx_interface.h"

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

	typedef struct gfx_command_pool gfx_command_pool_t;
	typedef struct gfx_fence gfx_fence_t;
	typedef struct gfx_semaphore gfx_semaphore_t;

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

	gfx_context_t* gfx_context_create(const gfx_context_create_info_t* cteate_info);
	void gfx_context_destroy(gfx_context_t* ctx);

	gfx_queue_t* gfx_queue_request(const gfx_context_t* ctx, const gfx_queue_request_t* create_info);
	void gfx_queue_return(gfx_queue_t* queue);

	gfx_command_pool_t* gfx_command_pool_create(const gfx_context_t* ctx);
	void gfx_command_pool_destroy(gfx_command_pool_t* command_pool);

	gfx_fence_t* gfx_fence_create(const gfx_context_t* ctx, VkFenceCreateFlags flags);
	void gfx_fence_wait(const gfx_fence_t* fence, uint64_t timeout);
	void gfx_fence_destroy(gfx_fence_t* fence);

	const gfx_instance_t* gfx_context_get_instance(const gfx_context_t* ctx);
	const gfx_surface_t* gfx_context_get_surface(const gfx_context_t* ctx);
	const gfx_adapter_t* gfx_context_get_adapter(const gfx_context_t* ctx);
	const gfx_device_t* gfx_context_get_device(const gfx_context_t* ctx);
	const gfx_allocator_t* gfx_context_get_gpu_allocator(const gfx_context_t* ctx);

#ifdef __cplusplus
}
#endif

#endif // GFX_CONTEXT_H