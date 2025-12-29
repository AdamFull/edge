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
		i32 family_index = -1;
		i32 queue_index = -1;

		operator bool() const noexcept { return family_index != -1 && queue_index != -1; }
	};

	struct CmdPool {
		VkCommandPool handle = VK_NULL_HANDLE;

		operator bool() const noexcept { return handle != VK_NULL_HANDLE; }
	};

	struct CmdBuf {
		VkCommandBuffer handle = VK_NULL_HANDLE;
		CmdPool pool = {};

		operator bool() const noexcept { return handle != VK_NULL_HANDLE && pool; }
	};

	struct QueryPool {
		VkQueryPool handle = VK_NULL_HANDLE;
		VkQueryType type = {};
		u32 max_query = 0u;
		bool host_reset_enabled = false;

		operator bool() const noexcept { return handle != VK_NULL_HANDLE; }
	};

	struct DescriptorSetLayout {
		VkDescriptorSetLayout handle = VK_NULL_HANDLE;
		u32 descriptor_sizes[DESCRIPTOR_SIZES_COUNT];

		operator bool() const noexcept { return handle != VK_NULL_HANDLE; }
	};

	struct DescriptorPool {
		VkDescriptorPool handle = VK_NULL_HANDLE;
		u32 descriptor_sizes[DESCRIPTOR_SIZES_COUNT];

		operator bool() const noexcept { return handle != VK_NULL_HANDLE; }
	};

	struct DescriptorSet {
		VkDescriptorSet handle = VK_NULL_HANDLE;
		DescriptorPool pool = {};

		operator bool() const noexcept { return handle != VK_NULL_HANDLE && pool; }
	};

	struct PipelineLayout {
		VkPipelineLayout handle = VK_NULL_HANDLE;

		operator bool() const noexcept { return handle != VK_NULL_HANDLE; }
	};

	struct Swapchain {
		VkSwapchainKHR handle = VK_NULL_HANDLE;
		VkFormat format = VK_FORMAT_UNDEFINED;
		VkColorSpaceKHR color_space = {};
		u32 image_count = 1u;
		VkExtent2D extent = { 1u, 1u };
		VkPresentModeKHR present_mode = {};
		VkCompositeAlphaFlagBitsKHR composite_alpha = {};

		operator bool() const noexcept { return handle != VK_NULL_HANDLE; }
	};

	struct DeviceMemory {
		VmaAllocation handle = VK_NULL_HANDLE;
		VkDeviceSize size = 0;
		void* mapped = nullptr;

		bool coherent = false;
		bool persistent = false;

		operator bool() const noexcept { return handle != VK_NULL_HANDLE; }
	};

	// TODO: COMPACT DATA
	struct Image {
		VkImage handle = VK_NULL_HANDLE;
		DeviceMemory memory = {};

		VkExtent3D extent = { 1u, 1u, 1u };
		u32 level_count = 1u;
		u32 layer_count = 1u;
		u32 face_count = 1u;
		VkImageUsageFlags usage_flags = 0;
		VkFormat format = VK_FORMAT_UNDEFINED;
		VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;

		operator bool() const noexcept { return handle != VK_NULL_HANDLE; }
	};

	struct ImageView {
		VkImageView handle = VK_NULL_HANDLE;
		VkImageViewType type = {};
		VkImageSubresourceRange range = {};

		operator bool() const noexcept { return handle != VK_NULL_HANDLE; }
	};

	struct PipelineCache {
		VkPipelineCache handle = VK_NULL_HANDLE;

		operator bool() const noexcept { return handle != VK_NULL_HANDLE; }
	};

	struct ShaderModule {
		VkShaderModule handle = VK_NULL_HANDLE;

		operator bool() const noexcept { return handle != VK_NULL_HANDLE; }
	};

	struct Pipeline {
		VkPipeline handle = VK_NULL_HANDLE;
		VkPipelineBindPoint bind_point = {};

		operator bool() const noexcept { return handle != VK_NULL_HANDLE; }
	};

	struct Sampler {
		VkSampler handle = VK_NULL_HANDLE;
		operator bool() const noexcept { return handle != VK_NULL_HANDLE; }
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

	enum class BufferLayout {
		Undefined,
		General,

		TransferSrc,
		TransferDst,

		VertexBuffer,
		IndexBuffer,

		UniformBuffer,

		StorageBufferRead,
		StorageBufferWrite,
		StorageBufferRW,

		IndirectBuffer,

		HostRead,
		HostWrite,

		ShaderRead,
		ShaderWrite,
		ShaderRW
	};

	struct Buffer {
		VkBuffer handle = VK_NULL_HANDLE;
		DeviceMemory memory = {};

		BufferFlags flags = 0;
		VkDeviceAddress address = 0;
		BufferLayout layout = BufferLayout::Undefined;

		operator bool() const noexcept { return handle != VK_NULL_HANDLE; }
	};

	struct BufferView {
		Buffer buffer = {};
		VkDeviceSize offset = 0;
		VkDeviceSize size = 0;

		operator bool() const noexcept { return buffer && size != 0; }
	};

	struct Fence {
		VkFence handle = VK_NULL_HANDLE;

		operator bool() const noexcept { return handle != VK_NULL_HANDLE; }
	};

	struct Semaphore {
		VkSemaphore handle = VK_NULL_HANDLE;
		VkSemaphoreType type = {};
		u64 value = 0ull;

		operator bool() const noexcept { return handle != VK_NULL_HANDLE; }
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