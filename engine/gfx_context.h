#ifndef GFX_CONTEXT_H
#define GFX_CONTEXT_H

#include "gfx_interface.h"

#include <stdbool.h>

#define GFX_MEMORY_BARRIERS_MAX 16
#define GFX_BUFFER_BARRIERS_MAX 32
#define GFX_IMAGE_BARRIERS_MAX 32

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

	typedef struct {
		VkDescriptorSetLayoutBinding bindings[GFX_MAX_BINDING_COUNT];
		VkDescriptorBindingFlagsEXT binding_flags[GFX_MAX_BINDING_COUNT];
		u32 binding_count;
	} gfx_descriptor_layout_builder_t;

	typedef struct {
		VkPushConstantRange constant_ranges[8];
		u32 constant_range_count;
		VkDescriptorSetLayout descriptor_layouts[GFX_MAX_BINDING_COUNT];
		u32 descriptor_layout_count;
	} gfx_pipeline_layout_builder_t;

	typedef enum {
		GFX_SWAPCHAIN_BUFFERING_AUTO,
		GFX_SWAPCHAIN_BUFFERING_DOUBLE,
		GFX_SWAPCHAIN_BUFFERING_TRIPLE,
	} gfx_swapchain_buffering_t;

	typedef struct {
		VkFormat preferred_format;
		VkColorSpaceKHR preferred_color_space;

		bool vsync_enable;
		bool hdr_enable;
	} gfx_swapchain_create_info_t;

	typedef struct {
		VkExtent3D extent;
		u32 level_count;
		u32 layer_count;
		u32 face_count;
		VkImageUsageFlags usage_flags;
		VkFormat format;
	} gfx_image_create_info_t;

	typedef struct {
		VkDeviceSize size;
		VkDeviceSize alignment;
		gfx_buffer_flags_t flags;
	} gfx_buffer_create_info_t;

	typedef struct {
		VkMemoryBarrier2 memory_barriers[GFX_MEMORY_BARRIERS_MAX];
		VkBufferMemoryBarrier2 buffer_barriers[GFX_BUFFER_BARRIERS_MAX];
		VkImageMemoryBarrier2 image_barriers[GFX_IMAGE_BARRIERS_MAX];

		u32 memory_barrier_count;
		u32 buffer_barrier_count;
		u32 image_barrier_count;

		VkDependencyFlags dependency_flags;
	} gfx_pipeline_barrier_builder_t;

	bool gfx_context_init(const gfx_context_create_info_t* cteate_info);
	void gfx_context_shutdown();

	const VkPhysicalDeviceProperties* gfx_get_adapter_props();

	bool gfx_get_queue(const gfx_queue_request_t* create_info, gfx_queue_t* queue);
	void gfx_release_queue(gfx_queue_t* queue);
	VkQueue get_queue_handle(const gfx_queue_t* queue);
	bool gfx_queue_submit(const gfx_queue_t* queue, const gfx_fence_t* fence, const VkSubmitInfo2KHR* submit_info);
	bool gfx_queue_present(const gfx_queue_t* queue, const VkPresentInfoKHR* present_info);
	void gfx_queue_wait_idle(const gfx_queue_t* queue);

	bool gfx_cmd_pool_create(const gfx_queue_t* queue, gfx_cmd_pool_t* cmd_pool);
	void gfx_cmd_pool_destroy(gfx_cmd_pool_t* command_pool);

	bool gfx_cmd_buf_create(const gfx_cmd_pool_t* cmd_pool, gfx_cmd_buf_t* cmd_buf);
	bool gfx_cmd_begin(const gfx_cmd_buf_t* cmd_buf);
	void gfx_cmd_end(const gfx_cmd_buf_t* cmd_buf);
	bool gfx_cmd_reset(const gfx_cmd_buf_t* cmd_buf);
	void gfx_cmd_reset_query(const gfx_cmd_buf_t* cmd_buf, const gfx_query_pool_t* query, u32 first_query, u32 query_count);
	void gfx_cmd_write_timestamp(const gfx_cmd_buf_t* cmd_buf, const gfx_query_pool_t* query, VkPipelineStageFlagBits2 stage, u32 query_index);
	void gfx_cmd_bind_descriptor(const gfx_cmd_buf_t* cmd_buf, const gfx_pipeline_layout_t* layout, const gfx_descriptor_set_t* descriptor, VkPipelineBindPoint bind_point);
	void gfx_cmd_pipeline_barrier(const gfx_cmd_buf_t* cmd_buf, const gfx_pipeline_barrier_builder_t* builder);
	void gfx_cmd_buf_destroy(gfx_cmd_buf_t* cmd_buf);

	void gfx_updete_descriptors(const VkWriteDescriptorSet* writes, u32 count);

	bool gfx_query_pool_create(VkQueryType type, u32 count, gfx_query_pool_t* query_pool);
	void gfx_query_pool_reset(gfx_query_pool_t* query_pool);
	bool gfx_query_pool_get_data(const gfx_query_pool_t* query_pool, u32 first_query, void* out_data);
	void gfx_query_pool_destroy(gfx_query_pool_t* query_pool);

	void gfx_descriptor_layout_builder_add_binding(VkDescriptorSetLayoutBinding binding, VkDescriptorBindingFlags flags, gfx_descriptor_layout_builder_t* builder);
	bool gfx_descriptor_set_layout_create(const gfx_descriptor_layout_builder_t* builder, gfx_descriptor_set_layout_t* descriptor_set_layout);
	void gfx_descriptor_set_layout_destroy(gfx_descriptor_set_layout_t* descriptor_set_layout);

	bool gfx_descriptor_pool_create(u32* descriptor_sizes, gfx_descriptor_pool_t* descriptor_pool);
	void gfx_descriptor_pool_destroy(gfx_descriptor_pool_t* descriptor_pool);

	bool gfx_descriptor_set_create(const gfx_descriptor_pool_t* pool, const gfx_descriptor_set_layout_t* layouts, gfx_descriptor_set_t* set);
	void gfx_descriptor_set_destroy(gfx_descriptor_set_t* set);

	void gfx_pipeline_layout_builder_add_range(gfx_pipeline_layout_builder_t* builder, VkShaderStageFlags flaags, u32 offset, u32 size);
	void gfx_pipeline_layout_builder_add_layout(gfx_pipeline_layout_builder_t* builder, const gfx_descriptor_set_layout_t* layout);

	bool gfx_pipeline_layout_create(const gfx_pipeline_layout_builder_t* builder, gfx_pipeline_layout_t* pipeline_layout);
	void gfx_pipeline_layout_destroy(gfx_pipeline_layout_t* pipeline_layout);

	bool gfx_swapchain_create(const gfx_swapchain_create_info_t* create_info, gfx_swapchain_t* swapchain);
	bool gfx_swapchain_update(gfx_swapchain_t* swapchain);
	bool gfx_swapchain_is_outdated(const gfx_swapchain_t* swapchain);
	bool gfx_swapchain_get_images(const gfx_swapchain_t* swapchain, gfx_image_t* image_out);
	bool gfx_swapchain_acquire_next_image(const gfx_swapchain_t* swapchain, u64 timeout, const gfx_semaphore_t* semaphore, u32* next_image_idx);
	void gfx_swapchain_destroy(gfx_swapchain_t* swapchain);

	void gfx_device_memory_setup(gfx_device_memory_t* mem);
	bool gfx_device_memory_is_mapped(const gfx_device_memory_t* mem);
	void* gfx_device_memory_map(gfx_device_memory_t* mem);
	void gfx_device_memory_unmap(gfx_device_memory_t* mem);
	void gfx_device_memory_flush(gfx_device_memory_t* mem, VkDeviceSize offset, VkDeviceSize size);
	void gfx_device_memory_update(gfx_device_memory_t* mem, const void* data, VkDeviceSize size, VkDeviceSize offset);

	bool gfx_image_create(const gfx_image_create_info_t* create_info, gfx_image_t* image);
	void gfx_image_destroy(gfx_image_t* image);

	bool gfx_buffer_create(const gfx_buffer_create_info_t* create_info, gfx_buffer_t* buffer);
	void gfx_buffer_destroy(gfx_buffer_t* buffer);

	bool gfx_semaphore_create(VkSemaphoreType type, u64 value, gfx_semaphore_t* semaphore);
	void gfx_semaphore_destroy(gfx_semaphore_t* semaphore);

	bool gfx_fence_create(VkFenceCreateFlags flags, gfx_fence_t* fence);
	bool gfx_fence_wait(const gfx_fence_t* fence, u64 timeout);
	void gfx_fence_reset(const gfx_fence_t* fence);
	void gfx_fence_destroy(gfx_fence_t* fence);

	bool gfx_pipeline_barrier_add_memory(gfx_pipeline_barrier_builder_t* builder, VkPipelineStageFlags2 src_stage_mask, VkAccessFlags2 src_access_mask,
		VkPipelineStageFlags2 dst_stage_mask, VkAccessFlags2 dst_access_mask);
	bool gfx_pipeline_barrier_add_buffer(gfx_pipeline_barrier_builder_t* builder, const gfx_buffer_t* buffer, VkPipelineStageFlags2 src_stage_mask,
		VkAccessFlags2 src_access_mask, VkPipelineStageFlags2 dst_stage_mask, VkAccessFlags2 dst_access_mask, VkDeviceSize offset, VkDeviceSize size);
	bool gfx_pipeline_barrier_add_image(gfx_pipeline_barrier_builder_t* builder, const gfx_image_t* image, VkImageLayout new_layout, VkImageSubresourceRange subresource_range);

#ifdef __cplusplus
}
#endif

#endif // GFX_CONTEXT_H