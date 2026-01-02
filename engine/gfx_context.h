#ifndef GFX_CONTEXT_H
#define GFX_CONTEXT_H

#include "gfx_interface.h"

namespace edge::gfx {
	struct PipelineBarrierBuilder;
	struct DescriptorLayoutBuilder;
	struct PipelineLayoutBuilder;

	void context_set_object_name(const char* name, VkObjectType type, u64 handle) noexcept;

	template<typename T>
	struct VulkanHandle {
		T handle = VK_NULL_HANDLE;

		void set_name(const char* name) const noexcept {
			context_set_object_name(name, VkObjectTraits<T>::object_type, (u64)handle);
		}

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

	struct ContextCreateInfo {
		const Allocator* alloc = nullptr;
		IRuntime* runtime = nullptr;
	};

	bool context_init(const ContextCreateInfo* cteate_info);
	void context_shutdown();

	bool context_is_extension_enabled(const char* name);

	const VkPhysicalDeviceProperties* get_adapter_props();

	void updete_descriptors(const VkWriteDescriptorSet* writes, u32 count);
}

#endif // GFX_CONTEXT_H