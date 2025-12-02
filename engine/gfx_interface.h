#ifndef GFX_INTERFACE_H
#define GFX_INTERFACE_H

#include <stdint.h>
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include "edge_base.h"

#define GFX_MAX_BINDING_COUNT 16
#define GFX_DESCRIPTOR_SIZES_COUNT 11

#ifdef __cplusplus
extern "C" {
#endif
	// External deps
	typedef struct edge_allocator edge_allocator_t;
	typedef struct platform_context platform_context_t;

	// Primitives
	typedef struct gfx_queue {
		i32 family_index;
		i32 queue_index;
	} gfx_queue_t;

	typedef struct gfx_cmd_pool {
		VkCommandPool handle;
	} gfx_cmd_pool_t;

	typedef struct gfx_cmd_buf {
		VkCommandBuffer handle;
		const gfx_cmd_pool_t* pool;
	} gfx_cmd_buf_t;

	typedef struct gfx_query_pool {
		VkQueryPool handle;
		VkQueryType type;
		u32 max_query;
		bool host_reset_enabled;
	} gfx_query_pool_t;

	typedef struct gfx_descriptor_set_layout {
		VkDescriptorSetLayout handle;
		u32 descriptor_sizes[GFX_DESCRIPTOR_SIZES_COUNT];
	} gfx_descriptor_set_layout_t;

	typedef struct {
		VkDescriptorPool handle;
		u32 descriptor_sizes[GFX_DESCRIPTOR_SIZES_COUNT];
	} gfx_descriptor_pool_t;

	typedef struct {
		VkDescriptorSet handle;
		const gfx_descriptor_pool_t* pool;
	} gfx_descriptor_set_t;

	typedef struct {
		VkPipelineLayout handle;
	} gfx_pipeline_layout_t;

	typedef struct {
		VkSwapchainKHR handle;
		VkFormat format;
		VkColorSpaceKHR color_space;
		u32 image_count;
		VkExtent2D extent;
		VkPresentModeKHR present_mode;
		VkCompositeAlphaFlagBitsKHR composite_alpha;
	} gfx_swapchain_t;

	typedef struct {
		VmaAllocation handle;
		VmaAllocationInfo info;

		bool coherent;
		bool persistent;
	} gfx_device_memory_t;

	typedef struct {
		VkImage handle;
		gfx_device_memory_t memory;

		VkExtent3D extent;
		u32 level_count;
		u32 layer_count;
		u32 face_count;
		VkImageUsageFlags usage_flags;
		VkFormat format;
		VkImageLayout layout;
	} gfx_image_t;

	typedef struct {
		VkImageView handle;
		VkImageViewType type;
		VkImageSubresourceRange range;
	} gfx_image_view_t;

	typedef struct {
		VkPipelineCache handle;
	} gfx_pipeline_cache_t;

	typedef struct {
		VkShaderModule handle;
	} gfx_shader_module_t;

	typedef struct gfx_pipeline {
		VkPipeline handle;
		VkPipelineBindPoint bind_point;
	} gfx_pipeline_t;

	typedef enum {
		GFX_BUFFER_FLAG_NONE = 0,
		GFX_BUFFER_FLAG_READBACK = 0x01,
		GFX_BUFFER_FLAG_STAGING = 0x02,
		GFX_BUFFER_FLAG_DYNAMIC = 0x04,
		GFX_BUFFER_FLAG_VERTEX = 0x08,
		GFX_BUFFER_FLAG_INDEX = 0x10,
		GFX_BUFFER_FLAG_UNIFORM = 0x20,
		GFX_BUFFER_FLAG_STORAGE = 0x40,
		GFX_BUFFER_FLAG_INDIRECT = 0x80,
		GFX_BUFFER_FLAG_DEVICE_ADDRESS = 0x100,
		GFX_BUFFER_FLAG_ACCELERATION_BUILD = 0x200,
		GFX_BUFFER_FLAG_ACCELERATION_STORE = 0x400,
		GFX_BUFFER_FLAG_SHADER_BINDING_TABLE = 0x800,
	} gfx_buffer_flag_t;

	typedef u16 gfx_buffer_flags_t;

	typedef struct {
		VkBuffer handle;
		gfx_device_memory_t memory;

		gfx_buffer_flags_t flags;
		VkDeviceAddress address;
	} gfx_buffer_t;

	typedef struct gfx_fence {
		VkFence handle;
	} gfx_fence_t;

	typedef struct gfx_semaphore {
		VkSemaphore handle;
		VkSemaphoreType type;
		u64 value;
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