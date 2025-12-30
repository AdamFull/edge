#ifndef GFX_INTERFACE_H
#define GFX_INTERFACE_H

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <span.hpp>

namespace edge {
	struct Allocator;
	struct PlatformContext;
	struct Window;
}

namespace edge::gfx {
	constexpr u64 MAX_BINDING_COUNT = 16;
	constexpr u64 DESCRIPTOR_SIZES_COUNT = 11;

	constexpr usize MEMORY_BARRIERS_MAX = 16;
	constexpr usize BUFFER_BARRIERS_MAX = 32;
	constexpr usize IMAGE_BARRIERS_MAX = 32;

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

	struct QueueRequest {
		QueueCapsFlags required_caps = {};
		QueueCapsFlags preferred_caps = {};
		QueueSelectionStrategy strategy = {};
		bool prefer_separate_family = false;
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

	struct BufferCreateInfo {
		VkDeviceSize size = 0ull;
		VkDeviceSize alignment = 0ull;
		BufferFlags flags = 0;
	};

	struct SwapchainCreateInfo {
		VkFormat preferred_format = VK_FORMAT_UNDEFINED;
		VkColorSpaceKHR preferred_color_space = {};

		bool vsync_enable = false;
		bool hdr_enable = false;
	};

	struct ImageCreateInfo {
		VkExtent3D extent = { 1u, 1u, 1u };
		u32 level_count = 1u;
		u32 layer_count = 1u;
		u32 face_count = 1u;
		VkImageUsageFlags usage_flags = 0;
		VkFormat format = VK_FORMAT_UNDEFINED;
	};

	struct Fence;
	struct Semaphore;
	struct Queue;
	struct QueryPool;
	struct PipelineLayout;
	struct DescriptorSetLayout;
	struct DescriptorPool;
	struct DescriptorSet;
	struct PipelineCache;
	struct ShaderModule;
	struct Pipeline;
	struct Sampler;
	struct DeviceMemory;
	struct Image;
	struct ImageView;
	struct Buffer;
	struct Swapchain;
	struct CmdPool;
	struct CmdBuf;

	template<typename T>
	struct VkObjectTraits;

	template<>
	struct VkObjectTraits<VkCommandPool> {
		static constexpr VkObjectType object_type = VK_OBJECT_TYPE_COMMAND_POOL;
		static constexpr const char* name = "VkCommandPool";
	};

	template<>
	struct VkObjectTraits<VkCommandBuffer> {
		static constexpr VkObjectType object_type = VK_OBJECT_TYPE_COMMAND_BUFFER;
		static constexpr const char* name = "VkCommandBuffer";
	};

	template<>
	struct VkObjectTraits<VkQueryPool> {
		static constexpr VkObjectType object_type = VK_OBJECT_TYPE_QUERY_POOL;
		static constexpr const char* name = "VkQueryPool";
	};

	template<>
	struct VkObjectTraits<VkDescriptorSetLayout> {
		static constexpr VkObjectType object_type = VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT;
		static constexpr const char* name = "VkDescriptorSetLayout";
	};

	template<>
	struct VkObjectTraits<VkDescriptorPool> {
		static constexpr VkObjectType object_type = VK_OBJECT_TYPE_DESCRIPTOR_POOL;
		static constexpr const char* name = "VkDescriptorPool";
	};

	template<>
	struct VkObjectTraits<VkDescriptorSet> {
		static constexpr VkObjectType object_type = VK_OBJECT_TYPE_DESCRIPTOR_SET;
		static constexpr const char* name = "VkDescriptorSet";
	};

	template<>
	struct VkObjectTraits<VkPipelineLayout> {
		static constexpr VkObjectType object_type = VK_OBJECT_TYPE_PIPELINE_LAYOUT;
		static constexpr const char* name = "VkPipelineLayout";
	};

	template<>
	struct VkObjectTraits<VkSwapchainKHR> {
		static constexpr VkObjectType object_type = VK_OBJECT_TYPE_SWAPCHAIN_KHR;
		static constexpr const char* name = "VkSwapchainKHR";
	};

	template<>
	struct VkObjectTraits<VmaAllocation> {
		static constexpr VkObjectType object_type = VK_OBJECT_TYPE_DEVICE_MEMORY;
		static constexpr const char* name = "VmaAllocation";
	};

	template<>
	struct VkObjectTraits<VkImage> {
		static constexpr VkObjectType object_type = VK_OBJECT_TYPE_IMAGE;
		static constexpr const char* name = "VkImage";
	};

	template<>
	struct VkObjectTraits<VkImageView> {
		static constexpr VkObjectType object_type = VK_OBJECT_TYPE_IMAGE_VIEW;
		static constexpr const char* name = "VkImageView";
	};

	template<>
	struct VkObjectTraits<VkBuffer> {
		static constexpr VkObjectType object_type = VK_OBJECT_TYPE_BUFFER;
		static constexpr const char* name = "VkBuffer";
	};

	template<>
	struct VkObjectTraits<VkPipelineCache> {
		static constexpr VkObjectType object_type = VK_OBJECT_TYPE_PIPELINE_CACHE;
		static constexpr const char* name = "VkPipelineCache";
	};

	template<>
	struct VkObjectTraits<VkShaderModule> {
		static constexpr VkObjectType object_type = VK_OBJECT_TYPE_SHADER_MODULE;
		static constexpr const char* name = "VkShaderModule";
	};

	template<>
	struct VkObjectTraits<VkPipeline> {
		static constexpr VkObjectType object_type = VK_OBJECT_TYPE_PIPELINE;
		static constexpr const char* name = "VkPipeline";
	};

	template<>
	struct VkObjectTraits<VkSampler> {
		static constexpr VkObjectType object_type = VK_OBJECT_TYPE_SAMPLER;
		static constexpr const char* name = "VkSampler";
	};

	template<>
	struct VkObjectTraits<VkSemaphore> {
		static constexpr VkObjectType object_type = VK_OBJECT_TYPE_SEMAPHORE;
		static constexpr const char* name = "VkSemaphore";
	};

	template<>
	struct VkObjectTraits<VkQueue> {
		static constexpr VkObjectType object_type = VK_OBJECT_TYPE_QUEUE;
		static constexpr const char* name = "VkQueue";
	};
}

#endif