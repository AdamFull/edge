#ifndef GFX_CONTEXT_H
#define GFX_CONTEXT_H

#include "gfx_interface.h"

#include <stdbool.h>

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
		VkBufferUsageFlags usage_flags;
	} gfx_buffer_create_info_t;

	bool gfx_context_init(const gfx_context_create_info_t* cteate_info);
	void gfx_context_shutdown();

	const VkPhysicalDeviceProperties* gfx_get_adapter_props();

	bool gfx_get_queue(const gfx_queue_request_t* create_info, gfx_queue_t* queue);
	void gfx_release_queue(gfx_queue_t* queue);

	bool gfx_command_pool_create(const gfx_queue_t* queue, gfx_command_pool_t* cmd_pool);
	void gfx_command_pool_destroy(gfx_command_pool_t* command_pool);

	bool gfx_query_pool_create(VkQueryType type, u32 count, gfx_query_pool_t* query_pool);
	void gfx_query_pool_reset(gfx_query_pool_t* query_pool);
	void gfx_query_pool_destroy(gfx_query_pool_t* query_pool);

	void gfx_descriptor_layout_builder_add_binding(VkDescriptorSetLayoutBinding binding, VkDescriptorBindingFlags flags, gfx_descriptor_layout_builder_t* builder);
	bool gfx_descriptor_set_layout_create(const gfx_descriptor_layout_builder_t* builder, gfx_descriptor_set_layout_t* descriptor_set_layout);
	void gfx_descriptor_set_layout_destroy(gfx_descriptor_set_layout_t* descriptor_set_layout);

	bool gfx_descriptor_pool_create(u32* descriptor_sizes, gfx_descriptor_pool_t* descriptor_pool);
	void gfx_descriptor_pool_destroy(gfx_descriptor_pool_t* descriptor_pool);

	bool gfx_descriptor_set_create(const gfx_descriptor_pool_t* pool, const gfx_descriptor_set_layout_t* layouts, gfx_descriptor_set_t* set);
	void gfx_descriptor_set_destroy(gfx_descriptor_set_t* set);

	void gfx_pipeline_layout_builder_add_range(VkShaderStageFlags flaags, u32 offset, u32 size, gfx_pipeline_layout_builder_t* builder);
	void gfx_pipeline_layout_builder_add_layout(const gfx_descriptor_set_layout_t* layout, gfx_pipeline_layout_builder_t* builder);

	bool gfx_pipeline_layout_create(const gfx_pipeline_layout_builder_t* builder, gfx_pipeline_layout_t* pipeline_layout);
	void gfx_pipeline_layout_destroy(gfx_pipeline_layout_t* pipeline_layout);

	bool gfx_swapchain_create(const gfx_swapchain_create_info_t* create_info, gfx_swapchain_t* swapchain);
	bool gfx_swapchain_update(gfx_swapchain_t* swapchain);
	void gfx_swapchain_destroy(gfx_swapchain_t* swapchain);

	bool gfx_image_create(const gfx_image_create_info_t* create_info, gfx_image_t* image);
	void gfx_image_destroy(gfx_image_t* image);

	bool gfx_buffer_create(const gfx_buffer_create_info_t* create_info, gfx_buffer_t* buffer);
	void gfx_buffer_destroy(gfx_buffer_t* buffer);

#ifdef __cplusplus
}
#endif

#endif // GFX_CONTEXT_H