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

	typedef enum {
		GFX_SWAPCHAIN_BUFFERING_AUTO,
		GFX_SWAPCHAIN_BUFFERING_DOUBLE,
		GFX_SWAPCHAIN_BUFFERING_TRIPLE,
	} gfx_swapchain_buffering_t;

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

	struct PipelineBarrierBuilder;
	struct DescriptorLayoutBuilder;
	struct PipelineLayoutBuilder;

	template<typename T>
	struct VulkanHandle {
		T handle = VK_NULL_HANDLE;

		explicit operator bool() const noexcept { return handle != VK_NULL_HANDLE; }
		explicit operator T() const noexcept { return handle; }
	};

	struct Fence : VulkanHandle<VkFence> {
		bool create(VkFenceCreateFlags flags) noexcept;
		void destroy() noexcept;
		bool wait(u64 timeout) noexcept;
		void reset() noexcept;
	};

	struct Semaphore : VulkanHandle<VkSemaphore> {
		VkSemaphoreType type = {};
		u64 value = 0ull;

		bool create(VkSemaphoreType type, u64 value) noexcept;
		void destroy() noexcept;
	};

	// Primitives
	struct Queue {
		i32 family_index = -1;
		i32 queue_index = -1;

		operator bool() const noexcept { return family_index != -1 && queue_index != -1; }

		bool request(QueueRequest create_info) noexcept;
		void release() noexcept;

		VkQueue get_handle() noexcept;
		bool submit(Fence fence, const VkSubmitInfo2KHR* submit_info) noexcept;
		bool present(const VkPresentInfoKHR* present_info) noexcept;
		void wait_idle() noexcept;
	};

	struct QueryPool : VulkanHandle<VkQueryPool> {
		VkQueryType type = {};
		u32 max_query = 0u;
		bool host_reset_enabled = false;

		bool create(VkQueryType type, u32 count) noexcept;
		void destroy() noexcept;
		void reset() noexcept;
		bool get_data(u32 first_query, void* out_data) noexcept;
	};

	struct PipelineLayout : VulkanHandle<VkPipelineLayout> {
		bool create(const PipelineLayoutBuilder& builder) noexcept;
		void destroy() noexcept;
	};

	struct DescriptorSetLayout : VulkanHandle<VkDescriptorSetLayout> {
		u32 descriptor_sizes[DESCRIPTOR_SIZES_COUNT];

		bool create(const DescriptorLayoutBuilder& builder) noexcept;
		void destroy() noexcept;
	};

	struct DescriptorPool : VulkanHandle<VkDescriptorPool> {
		u32 descriptor_sizes[DESCRIPTOR_SIZES_COUNT];

		bool create(u32* descriptor_sizes) noexcept;
		void destroy() noexcept;
	};

	struct DescriptorSet : VulkanHandle<VkDescriptorSet> {
		DescriptorPool pool = {};

		bool create(DescriptorPool pool, const DescriptorSetLayout* layouts) noexcept;
		void destroy() noexcept;
	};

	struct PipelineCache : VulkanHandle<VkPipelineCache> {
		bool create(const u8* data, usize data_size) noexcept;
		void destroy() noexcept;
	};

	struct ShaderModule : VulkanHandle<VkShaderModule> {
		bool create(const u32* code, usize size) noexcept;
		void destroy() noexcept;
	};

	struct GraphicsPipelineCreateInfo {
		PipelineLayout layout = {};
		PipelineCache cache = {};
	};

	struct ComputePipelineCreateInfo {
		ShaderModule shader_module = {};
		PipelineLayout layout = {};
		PipelineCache cache = {};
	};

	struct Pipeline : VulkanHandle<VkPipeline> {
		VkPipelineBindPoint bind_point = {};

		bool create(const VkGraphicsPipelineCreateInfo* create_info) noexcept;
		bool create(ComputePipelineCreateInfo create_info) noexcept;
		void destroy() noexcept;
	};

	struct Sampler : VulkanHandle<VkSampler> {
		bool create(const VkSamplerCreateInfo& create_info) noexcept;
		void destroy() noexcept;
	};

	struct DeviceMemory : VulkanHandle<VmaAllocation> {
		VkDeviceSize size = 0;
		void* mapped = nullptr;

		bool coherent = false;
		bool persistent = false;

		void setup() noexcept;
		bool is_mapped() noexcept;
		void* map() noexcept;
		void unmap() noexcept;
		void flush(VkDeviceSize offset, VkDeviceSize size) noexcept;
		void update(const void* data, VkDeviceSize size, VkDeviceSize offset) noexcept;
	};

	// TODO: COMPACT DATA
	struct Image : VulkanHandle<VkImage> {
		DeviceMemory memory = {};

		VkExtent3D extent = { 1u, 1u, 1u };
		u32 level_count = 1u;
		u32 layer_count = 1u;
		u32 face_count = 1u;
		VkImageUsageFlags usage_flags = 0;
		VkFormat format = VK_FORMAT_UNDEFINED;
		VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;

		bool create(ImageCreateInfo create_info) noexcept;
		void destroy() noexcept;
	};

	struct ImageView : VulkanHandle<VkImageView> {
		VkImageViewType type = {};
		VkImageSubresourceRange range = {};

		bool create(Image image, VkImageViewType type, VkImageSubresourceRange subresource_range) noexcept;
		void destroy() noexcept;
	};

	struct Buffer : VulkanHandle<VkBuffer> {
		DeviceMemory memory = {};

		BufferFlags flags = 0;
		VkDeviceAddress address = 0;
		BufferLayout layout = BufferLayout::Undefined;

		bool create(BufferCreateInfo create_info) noexcept;
		void destroy() noexcept;
	};

	struct BufferView {
		Buffer buffer = {};
		VkDeviceSize local_offset = 0;
		VkDeviceSize size = 0;

		operator bool() const noexcept { return buffer && size != 0; }

		void write(Span<const u8> data, VkDeviceSize offset) noexcept;
	};

	struct Swapchain : VulkanHandle<VkSwapchainKHR> {
		VkFormat format = VK_FORMAT_UNDEFINED;
		VkColorSpaceKHR color_space = {};
		u32 image_count = 1u;
		VkExtent2D extent = { 1u, 1u };
		VkPresentModeKHR present_mode = {};
		VkCompositeAlphaFlagBitsKHR composite_alpha = {};

		bool create(SwapchainCreateInfo create_info) noexcept;
		void destroy() noexcept;

		bool update() noexcept;
		bool is_outdated() noexcept;
		bool get_images(Image* image_out) noexcept;
		bool acquire_next_image(u64 timeout, Semaphore semaphore, u32* next_image_idx) noexcept;
	};

	struct CmdPool : VulkanHandle<VkCommandPool> {
		bool create(Queue queue) noexcept;
		void destroy() noexcept;
	};

	struct CmdBuf : VulkanHandle<VkCommandBuffer> {
		CmdPool pool = {};

		bool create(CmdPool cmd_pool) noexcept;
		void destroy() noexcept;

		bool begin() noexcept;
		bool end() noexcept;
		void begin_marker(const char* name, u32 color) noexcept;
		void end_marker() noexcept;
		bool reset() noexcept;
		void reset_query(QueryPool query, u32 first_query, u32 query_count) noexcept;
		void write_timestamp(QueryPool query, VkPipelineStageFlagBits2 stage, u32 query_index) noexcept;
		void bind_descriptor(PipelineLayout layout, DescriptorSet descriptor, VkPipelineBindPoint bind_point) noexcept;
		void pipeline_barrier(const PipelineBarrierBuilder& builder) noexcept;
		void begin_rendering(const VkRenderingInfoKHR& rendering_info) noexcept;
		void end_rendering() noexcept;
		void bind_index_buffer(Buffer buffer, VkIndexType type) noexcept;
		void bind_pipeline(Pipeline pipeline) noexcept;
		void set_viewport(VkViewport viewport) noexcept;
		void set_viewport(f32 x, f32 y, f32 width, f32 height, f32 depth_min = 0.0f, f32 depth_max = 1.0f) noexcept;
		void set_scissor(VkRect2D rect) noexcept;
		void set_scissor(i32 off_x, i32 off_y, u32 width, u32 height) noexcept;
		void push_constants(PipelineLayout layout, VkShaderStageFlags flags, u32 offset, u32 size, const void* data) noexcept;
		void draw_indexed(u32 idx_cnt, u32 inst_cnt, u32 first_idx, i32 vtx_offset, u32 first_inst) noexcept;
	};

	struct DescriptorLayoutBuilder {
		VkDescriptorSetLayoutBinding bindings[MAX_BINDING_COUNT] = {};
		VkDescriptorBindingFlagsEXT binding_flags[MAX_BINDING_COUNT] = {};
		u32 binding_count = 0u;

		void add_binding(VkDescriptorSetLayoutBinding binding, VkDescriptorBindingFlags flags) noexcept;
	};

	struct PipelineBarrierBuilder {
		VkMemoryBarrier2 memory_barriers[MEMORY_BARRIERS_MAX] = {};
		VkBufferMemoryBarrier2 buffer_barriers[BUFFER_BARRIERS_MAX] = {};
		VkImageMemoryBarrier2 image_barriers[IMAGE_BARRIERS_MAX] = {};

		u32 memory_barrier_count = 0u;
		u32 buffer_barrier_count = 0u;
		u32 image_barrier_count = 0u;

		VkDependencyFlags dependency_flags = 0;

		bool add_memory(VkPipelineStageFlags2 src_stage_mask, VkAccessFlags2 src_access_mask,
			VkPipelineStageFlags2 dst_stage_mask, VkAccessFlags2 dst_access_mask) noexcept;
		bool add_buffer(Buffer buffer, BufferLayout new_layout, VkDeviceSize offset, VkDeviceSize size) noexcept;
		bool add_image(Image image, VkImageLayout new_layout, VkImageSubresourceRange subresource_range) noexcept;
		void reset() noexcept;
	};

	struct PipelineLayoutBuilder {
		VkPushConstantRange constant_ranges[8] = {};
		u32 constant_range_count = 0u;
		VkDescriptorSetLayout descriptor_layouts[MAX_BINDING_COUNT] = {};
		u32 descriptor_layout_count = 0u;

		void add_range(VkShaderStageFlags flaags, u32 offset, u32 size) noexcept;
		void add_layout(DescriptorSetLayout layout) noexcept;
	};


	// Some handle type traits

	template<typename T>
	struct HandleTraits;

	template<>
	struct HandleTraits<CmdPool> {
		using VulkanType = VkCommandPool;
		static constexpr VkObjectType object_type = VK_OBJECT_TYPE_COMMAND_POOL;
		static constexpr const char* name = "CmdPool";
	};

	template<>
	struct HandleTraits<CmdBuf> {
		using VulkanType = VkCommandBuffer;
		static constexpr VkObjectType object_type = VK_OBJECT_TYPE_COMMAND_BUFFER;
		static constexpr const char* name = "CmdBuf";
	};

	template<>
	struct HandleTraits<QueryPool> {
		using VulkanType = VkQueryPool;
		static constexpr VkObjectType object_type = VK_OBJECT_TYPE_QUERY_POOL;
		static constexpr const char* name = "QueryPool";
	};

	template<>
	struct HandleTraits<DescriptorSetLayout> {
		using VulkanType = VkDescriptorSetLayout;
		static constexpr VkObjectType object_type = VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT;
		static constexpr const char* name = "DescriptorSetLayout";
	};

	template<>
	struct HandleTraits<DescriptorPool> {
		using VulkanType = VkDescriptorPool;
		static constexpr VkObjectType object_type = VK_OBJECT_TYPE_DESCRIPTOR_POOL;
		static constexpr const char* name = "DescriptorPool";
	};

	template<>
	struct HandleTraits<DescriptorSet> {
		using VulkanType = VkDescriptorSet;
		static constexpr VkObjectType object_type = VK_OBJECT_TYPE_DESCRIPTOR_SET;
		static constexpr const char* name = "DescriptorSet";
	};

	template<>
	struct HandleTraits<PipelineLayout> {
		using VulkanType = VkPipelineLayout;
		static constexpr VkObjectType object_type = VK_OBJECT_TYPE_PIPELINE_LAYOUT;
		static constexpr const char* name = "PipelineLayout";
	};

	template<>
	struct HandleTraits<Swapchain> {
		using VulkanType = VkSwapchainKHR;
		static constexpr VkObjectType object_type = VK_OBJECT_TYPE_SWAPCHAIN_KHR;
		static constexpr const char* name = "Swapchain";
	};

	template<>
	struct HandleTraits<DeviceMemory> {
		using VulkanType = VmaAllocation;
		static constexpr VkObjectType object_type = VK_OBJECT_TYPE_DEVICE_MEMORY;
		static constexpr const char* name = "DeviceMemory";
	};

	template<>
	struct HandleTraits<Image> {
		using VulkanType = VkImage;
		static constexpr VkObjectType object_type = VK_OBJECT_TYPE_IMAGE;
		static constexpr const char* name = "Image";
	};

	template<>
	struct HandleTraits<ImageView> {
		using VulkanType = VkImageView;
		static constexpr VkObjectType object_type = VK_OBJECT_TYPE_IMAGE_VIEW;
		static constexpr const char* name = "ImageView";
	};

	template<>
	struct HandleTraits<Buffer> {
		using VulkanType = VkBuffer;
		static constexpr VkObjectType object_type = VK_OBJECT_TYPE_BUFFER;
		static constexpr const char* name = "Buffer";
	};

	template<>
	struct HandleTraits<PipelineCache> {
		using VulkanType = VkPipelineCache;
		static constexpr VkObjectType object_type = VK_OBJECT_TYPE_PIPELINE_CACHE;
		static constexpr const char* name = "PipelineCache";
	};

	template<>
	struct HandleTraits<ShaderModule> {
		using VulkanType = VkShaderModule;
		static constexpr VkObjectType object_type = VK_OBJECT_TYPE_SHADER_MODULE;
		static constexpr const char* name = "ShaderModule";
	};

	template<>
	struct HandleTraits<Pipeline> {
		using VulkanType = VkPipeline;
		static constexpr VkObjectType object_type = VK_OBJECT_TYPE_PIPELINE;
		static constexpr const char* name = "Pipeline";
	};

	template<>
	struct HandleTraits<Sampler> {
		using VulkanType = VkSampler;
		static constexpr VkObjectType object_type = VK_OBJECT_TYPE_SAMPLER;
		static constexpr const char* name = "Sampler";
	};

	template<>
	struct HandleTraits<Fence> {
		using VulkanType = VkFence;
		static constexpr VkObjectType object_type = VK_OBJECT_TYPE_FENCE;
		static constexpr const char* name = "Fence";
	};

	template<>
	struct HandleTraits<Semaphore> {
		using VulkanType = VkSemaphore;
		static constexpr VkObjectType object_type = VK_OBJECT_TYPE_SEMAPHORE;
		static constexpr const char* name = "Semaphore";
	};

	template<>
	struct HandleTraits<Queue> {
		using VulkanType = VkQueue;
		static constexpr VkObjectType object_type = VK_OBJECT_TYPE_QUEUE;
		static constexpr const char* name = "Queue";
	};
}

#endif