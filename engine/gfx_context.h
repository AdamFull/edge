#ifndef GFX_CONTEXT_H
#define GFX_CONTEXT_H

#include "gfx_interface.h"

namespace edge::gfx {
	constexpr usize MEMORY_BARRIERS_MAX = 16;
	constexpr usize BUFFER_BARRIERS_MAX = 32;
	constexpr usize IMAGE_BARRIERS_MAX = 32;

	struct ContextCreateInfo {
		const Allocator* alloc = nullptr;
		PlatformContext* platform_context = nullptr;
	};

	struct QueueRequest {
		QueueCapsFlags required_caps = {};
		QueueCapsFlags preferred_caps = {};
		QueueSelectionStrategy strategy = {};
		bool prefer_separate_family = false;
	};

	struct DescriptorLayoutBuilder {
		VkDescriptorSetLayoutBinding bindings[MAX_BINDING_COUNT] = {};
		VkDescriptorBindingFlagsEXT binding_flags[MAX_BINDING_COUNT] = {};
		u32 binding_count = 0u;
	};

	struct PipelineLayoutBuilder {
		VkPushConstantRange constant_ranges[8] = {};
		u32 constant_range_count = 0u;
		VkDescriptorSetLayout descriptor_layouts[MAX_BINDING_COUNT] = {};
		u32 descriptor_layout_count = 0u;
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

	struct BufferCreateInfo {
		VkDeviceSize size = 0ull;
		VkDeviceSize alignment = 0ull;
		BufferFlags flags = 0;
	};

	struct PipelineBarrierBuilder {
		VkMemoryBarrier2 memory_barriers[MEMORY_BARRIERS_MAX] = {};
		VkBufferMemoryBarrier2 buffer_barriers[BUFFER_BARRIERS_MAX] = {};
		VkImageMemoryBarrier2 image_barriers[IMAGE_BARRIERS_MAX] = {};

		u32 memory_barrier_count = 0u;
		u32 buffer_barrier_count = 0u;
		u32 image_barrier_count = 0u;

		VkDependencyFlags dependency_flags = 0;
	};

	struct GraphicsPipelineCreateInfo {
		const PipelineLayout* layout = nullptr;
		const PipelineCache* cache = nullptr;
	};

	struct ComputePipelineCreateInfo {
		const ShaderModule* shader_module = nullptr;
		const PipelineLayout* layout = nullptr;
		const PipelineCache* cache = nullptr;
	};

	bool context_init(const ContextCreateInfo* cteate_info);
	void context_shutdown();

	bool context_is_extension_enabled(const char* name);

	const VkPhysicalDeviceProperties* get_adapter_props();

	bool get_queue(const QueueRequest* create_info, Queue* queue);
	void release_queue(Queue* queue);
	VkQueue queue_handle(const Queue* queue);
	bool queue_submit(const Queue* queue, const Fence* fence, const VkSubmitInfo2KHR* submit_info);
	bool queue_present(const Queue* queue, const VkPresentInfoKHR* present_info);
	void queue_wait_idle(const Queue* queue);

	bool cmd_pool_create(const Queue* queue, CmdPool* cmd_pool);
	void cmd_pool_destroy(CmdPool* command_pool);

	bool cmd_buf_create(const CmdPool* cmd_pool, CmdBuf* cmd_buf);
	bool cmd_begin(const CmdBuf* cmd_buf);
	bool cmd_end(const CmdBuf* cmd_buf);
	bool cmd_reset(const CmdBuf* cmd_buf);
	void cmd_reset_query(const CmdBuf* cmd_buf, const QueryPool* query, u32 first_query, u32 query_count);
	void cmd_write_timestamp(const CmdBuf* cmd_buf, const QueryPool* query, VkPipelineStageFlagBits2 stage, u32 query_index);
	void cmd_bind_descriptor(const CmdBuf* cmd_buf, const PipelineLayout* layout, const DescriptorSet* descriptor, VkPipelineBindPoint bind_point);
	void cmd_pipeline_barrier(const CmdBuf* cmd_buf, const PipelineBarrierBuilder* builder);
	void cmd_buf_destroy(CmdBuf* cmd_buf);

	void updete_descriptors(const VkWriteDescriptorSet* writes, u32 count);

	bool query_pool_create(VkQueryType type, u32 count, QueryPool* query_pool);
	void query_pool_reset(QueryPool* query_pool);
	bool query_pool_get_data(const QueryPool* query_pool, u32 first_query, void* out_data);
	void query_pool_destroy(QueryPool* query_pool);

	void descriptor_layout_builder_add_binding(VkDescriptorSetLayoutBinding binding, VkDescriptorBindingFlags flags, DescriptorLayoutBuilder* builder);
	bool descriptor_set_layout_create(const DescriptorLayoutBuilder* builder, DescriptorSetLayout* descriptor_set_layout);
	void descriptor_set_layout_destroy(DescriptorSetLayout* descriptor_set_layout);

	bool descriptor_pool_create(u32* descriptor_sizes, DescriptorPool* descriptor_pool);
	void descriptor_pool_destroy(DescriptorPool* descriptor_pool);

	bool descriptor_set_create(const DescriptorPool* pool, const DescriptorSetLayout* layouts, DescriptorSet* set);
	void descriptor_set_destroy(DescriptorSet* set);

	void pipeline_layout_builder_add_range(PipelineLayoutBuilder* builder, VkShaderStageFlags flaags, u32 offset, u32 size);
	void pipeline_layout_builder_add_layout(PipelineLayoutBuilder* builder, const DescriptorSetLayout* layout);

	bool pipeline_layout_create(const PipelineLayoutBuilder* builder, PipelineLayout* pipeline_layout);
	void pipeline_layout_destroy(PipelineLayout* pipeline_layout);

	bool pipeline_cache_create(const u8* data, size_t data_size, PipelineCache* pipeline_cache);
	void pipeline_cache_destroy(PipelineCache* pipeline_cache);

	bool shader_module_create(const u32* code, size_t size, ShaderModule* shader_module);
	void shader_module_destroy(ShaderModule* shader_module);

	bool pipeline_graphics_create(const VkGraphicsPipelineCreateInfo* create_info, Pipeline* pipeline);
	bool pipeline_compute_create(const ComputePipelineCreateInfo* create_info, Pipeline* pipeline);
	void pipeline_destroy(Pipeline* pipeline);

	bool swapchain_create(const SwapchainCreateInfo* create_info, Swapchain* swapchain);
	bool swapchain_update(Swapchain* swapchain);
	bool swapchain_is_outdated(const Swapchain* swapchain);
	bool swapchain_get_images(const Swapchain* swapchain, Image* image_out);
	bool swapchain_acquire_next_image(const Swapchain* swapchain, u64 timeout, const Semaphore* semaphore, u32* next_image_idx);
	void swapchain_destroy(Swapchain* swapchain);

	void device_memory_setup(DeviceMemory* mem);
	bool device_memory_is_mapped(const DeviceMemory* mem);
	void* device_memory_map(DeviceMemory* mem);
	void device_memory_unmap(DeviceMemory* mem);
	void device_memory_flush(DeviceMemory* mem, VkDeviceSize offset, VkDeviceSize size);
	void device_memory_update(DeviceMemory* mem, const void* data, VkDeviceSize size, VkDeviceSize offset);

	bool image_create(const ImageCreateInfo* create_info, Image* image);
	void image_destroy(Image* image);

	bool image_view_create(const Image* image, VkImageViewType type, VkImageSubresourceRange subresource_range, ImageView* view);
	void image_view_destroy(ImageView* view);

	bool buffer_create(const BufferCreateInfo* create_info, Buffer* buffer);
	void buffer_destroy(Buffer* buffer);

	bool semaphore_create(VkSemaphoreType type, u64 value, Semaphore* semaphore);
	void semaphore_destroy(Semaphore* semaphore);

	bool fence_create(VkFenceCreateFlags flags, Fence* fence);
	bool fence_wait(const Fence* fence, u64 timeout);
	void fence_reset(const Fence* fence);
	void fence_destroy(Fence* fence);

	bool pipeline_barrier_add_memory(PipelineBarrierBuilder* builder, VkPipelineStageFlags2 src_stage_mask, VkAccessFlags2 src_access_mask,
		VkPipelineStageFlags2 dst_stage_mask, VkAccessFlags2 dst_access_mask);
	bool pipeline_barrier_add_buffer(PipelineBarrierBuilder* builder, const Buffer* buffer, VkPipelineStageFlags2 src_stage_mask,
		VkAccessFlags2 src_access_mask, VkPipelineStageFlags2 dst_stage_mask, VkAccessFlags2 dst_access_mask, VkDeviceSize offset, VkDeviceSize size);
	bool pipeline_barrier_add_image(PipelineBarrierBuilder* builder, const Image* image, VkImageLayout new_layout, VkImageSubresourceRange subresource_range);
	void pipeline_barrier_builder_reset(PipelineBarrierBuilder* builder);
}

#endif // GFX_CONTEXT_H