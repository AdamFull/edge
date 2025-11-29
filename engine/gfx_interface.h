#ifndef GFX_INTERFACE_H
#define GFX_INTERFACE_H

#include <stdint.h>
#include <vulkan/vulkan.h>

#define GFX_DESCRIPTOR_SIZES_COUNT 11

#ifdef __cplusplus
extern "C" {
#endif
	// External deps
	typedef struct edge_allocator edge_allocator_t;
	typedef struct platform_context platform_context_t;

	// Primitives
	typedef struct gfx_queue {
		int32_t family_index;
		int32_t queue_index;
	} gfx_queue_t;

	typedef struct gfx_command_pool {
		VkCommandPool handle;
	} gfx_command_pool_t;

	typedef struct gfx_query_pool {
		VkQueryPool handle;
		VkQueryType type;
		uint32_t max_query;
	} gfx_query_pool_t;

	typedef struct gfx_descriptor_set_layout {
		VkDescriptorSetLayout handle;
		uint32_t descriptor_sizes[GFX_DESCRIPTOR_SIZES_COUNT];
	} gfx_descriptor_set_layout_t;

	typedef struct {
		VkDescriptorPool handle;
		uint32_t descriptor_sizes[GFX_DESCRIPTOR_SIZES_COUNT];
	} gfx_descriptor_pool_t;

	typedef struct {
		VkDescriptorSet handle;
		const gfx_descriptor_pool_t* pool;
	} gfx_descriptor_set_t;

	typedef struct gfx_fence {
		VkFence handle;
	} gfx_fence_t;

	typedef struct gfx_semaphore {
		VkSemaphore handle;
	} gfx_semaphore_t;

	enum gfx_queue_caps_flag {
		GFX_QUEUE_CAPS_NONE				= 0,
		GFX_QUEUE_CAPS_GRAPHICS			= 0x01,	// Graphics operations
		GFX_QUEUE_CAPS_COMPUTE			= 0x02,	// Compute shader dispatch
		GFX_QUEUE_CAPS_TRANSFER			= 0x04,	// Transfer/copy operations (implicit in Graphics/Compute)
		GFX_QUEUE_CAPS_PRESENT			= 0x08,	// Surface presentation support
		GFX_QUEUE_CAPS_SPARSE_BINDING	= 0x10,	// Sparse memory binding
		GFX_QUEUE_CAPS_PROTECTED		= 0x20,	// Protected memory operations
		GFX_QUEUE_CAPS_VIDEO_DECODE		= 0x40,	// Video decode operations
		GFX_QUEUE_CAPS_VIDEO_ENCODE		= 0x80,	// Video encode operations
	};

	typedef uint16_t gfx_queue_caps_flags_t;

	typedef enum {
		GFX_QUEUE_SELECTION_STRATEGY_EXACT,					// Must match exactly the requested capabilities
		GFX_QUEUE_SELECTION_STRATEGY_MINIMAL,				// Must have at least these capabilities
		GFX_QUEUE_SELECTION_STRATEGY_PREFER_DEDICATED,		// Prefer queues with only requested capabilities
		GFX_QUEUE_SELECTION_STRATEGY_PREFER_SHARED			// Prefer queues with additional capabilities
	} gfx_queue_selection_strategy_t;

#ifdef __cplusplus
}
#endif

#endif