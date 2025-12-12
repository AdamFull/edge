#ifndef GFX_INTERFACE_H
#define GFX_INTERFACE_H

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <stddef.hpp>

namespace edge {
	struct Allocator;
	struct PlatformContext;
}

namespace edge::gfx {
	constexpr u64 MAX_BINDING_COUNT = 16;
	constexpr u64 DESCRIPTOR_SIZES_COUNT = 11;

	// Primitives
	struct Queue {
		i32 family_index;
		i32 queue_index;
	};

	struct CmdPool {
		VkCommandPool handle;
	};

	struct CmdBuf {
		VkCommandBuffer handle;
		const CmdPool* pool;
	};

	struct QueryPool {
		VkQueryPool handle;
		VkQueryType type;
		u32 max_query;
		bool host_reset_enabled;
	};

	struct DescriptorSetLayout {
		VkDescriptorSetLayout handle;
		u32 descriptor_sizes[DESCRIPTOR_SIZES_COUNT];
	};

	struct DescriptorPool {
		VkDescriptorPool handle;
		u32 descriptor_sizes[DESCRIPTOR_SIZES_COUNT];
	};

	struct DescriptorSet {
		VkDescriptorSet handle;
		const DescriptorPool* pool;
	};

	struct PipelineLayout {
		VkPipelineLayout handle;
	};

	struct Swapchain {
		VkSwapchainKHR handle;
		VkFormat format;
		VkColorSpaceKHR color_space;
		u32 image_count;
		VkExtent2D extent;
		VkPresentModeKHR present_mode;
		VkCompositeAlphaFlagBitsKHR composite_alpha;
	};

	struct DeviceMemory {
		VmaAllocation handle;
		VmaAllocationInfo info;

		bool coherent;
		bool persistent;
	};

	struct Image {
		VkImage handle;
		DeviceMemory memory;

		VkExtent3D extent;
		u32 level_count;
		u32 layer_count;
		u32 face_count;
		VkImageUsageFlags usage_flags;
		VkFormat format;
		VkImageLayout layout;
	};

	struct ImageView {
		VkImageView handle;
		VkImageViewType type;
		VkImageSubresourceRange range;
	};

	struct PipelineCache {
		VkPipelineCache handle;
	};

	struct ShaderModule {
		VkShaderModule handle;
	};

	struct Pipeline {
		VkPipeline handle;
		VkPipelineBindPoint bind_point;
	};

	enum BufferFlag {
		BUFFER_FLAG_NONE = 0,
		BUFFER_FLAG_READBACK = 0x01,
		BUFFER_FLAG_STAGING = 0x02,
		BUFFER_FLAG_DYNAMIC = 0x04,
		BUFFER_FLAG_VERTEX = 0x08,
		BUFFER_FLAG_INDEX = 0x10,
		BUFFER_FLAG_UNIFORM = 0x20,
		BUFFER_FLAG_STORAGE = 0x40,
		BUFFER_FLAG_INDIRECT = 0x80,
		BUFFER_FLAG_DEVICE_ADDRESS = 0x100,
		BUFFER_FLAG_ACCELERATION_BUILD = 0x200,
		BUFFER_FLAG_ACCELERATION_STORE = 0x400,
		BUFFER_FLAG_SHADER_BINDING_TABLE = 0x800,
	};

	using BufferFlags = u16;

	struct Buffer {
		VkBuffer handle;
		DeviceMemory memory;

		BufferFlags flags;
		VkDeviceAddress address;
	};

	struct Fence {
		VkFence handle;
	};

	struct Semaphore {
		VkSemaphore handle;
		VkSemaphoreType type;
		u64 value;
	};

	enum QueueCapsFlag {
		QUEUE_CAPS_NONE = 0,
		QUEUE_CAPS_GRAPHICS = 0x01,	// Graphics operations
		QUEUE_CAPS_COMPUTE = 0x02,	// Compute shader dispatch
		QUEUE_CAPS_TRANSFER = 0x04,	// Transfer/copy operations (implicit in Graphics/Compute)
		QUEUE_CAPS_PRESENT = 0x08,	// Surface presentation support
		QUEUE_CAPS_SPARSE_BINDING = 0x10,	// Sparse memory binding
		QUEUE_CAPS_PROTECTED = 0x20,	// Protected memory operations
		QUEUE_CAPS_VIDEO_DECODE = 0x40,	// Video decode operations
		QUEUE_CAPS_VIDEO_ENCODE = 0x80,	// Video encode operations
	};
	using QueueCapsFlags = u16;

	enum QueueSelectionStrategy {
		QUEUE_SELECTION_STRATEGY_EXACT,					// Must match exactly the requested capabilities
		QUEUE_SELECTION_STRATEGY_MINIMAL,				// Must have at least these capabilities
		QUEUE_SELECTION_STRATEGY_PREFER_DEDICATED,		// Prefer queues with only requested capabilities
		QUEUE_SELECTION_STRATEGY_PREFER_SHARED			// Prefer queues with additional capabilities
	};
}

#endif