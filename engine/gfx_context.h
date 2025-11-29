#ifndef GFX_CONTEXT_H
#define GFX_CONTEXT_H

#include "gfx_interface.h"

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

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
		VkDescriptorSetLayoutBinding bindings[16];
		VkDescriptorBindingFlagsEXT binding_flags[16];
		uint32_t binding_count;
	} gfx_descriptor_layout_builder_t;

	bool gfx_context_init(const gfx_context_create_info_t* cteate_info);
	void gfx_context_shutdown();

	const VkPhysicalDeviceProperties* gfx_get_adapter_props();

	bool gfx_get_queue(const gfx_queue_request_t* create_info, gfx_queue_t* queue);
	void gfx_release_queue(gfx_queue_t* queue);

	bool gfx_command_pool_create(const gfx_queue_t* queue, gfx_command_pool_t* cmd_pool);
	void gfx_command_pool_destroy(gfx_command_pool_t* command_pool);

	bool gfx_query_pool_create(VkQueryType type, uint32_t count, gfx_query_pool_t* query_pool);
	void gfx_query_pool_reset(gfx_query_pool_t* query_pool);
	void gfx_query_pool_destroy(gfx_query_pool_t* query_pool);

	void gfx_descriptor_layout_builder_add_binding(VkDescriptorSetLayoutBinding binding, VkDescriptorBindingFlags flags, gfx_descriptor_layout_builder_t* builder);
	bool gfx_descriptor_set_layout_create(const gfx_descriptor_layout_builder_t* builder, gfx_descriptor_set_layout_t* descriptor_set_layout);
	void gfx_descriptor_set_layout_destroy(gfx_descriptor_set_layout_t* descriptor_set_layout);

	bool gfx_descriptor_pool_create(uint32_t* descriptor_sizes, gfx_descriptor_pool_t* descriptor_pool);
	void gfx_descriptor_pool_destroy(gfx_descriptor_pool_t* descriptor_pool);

	bool gfx_descriptor_set_create(const gfx_descriptor_pool_t* pool, const gfx_descriptor_set_layout_t* layouts, gfx_descriptor_set_t* set);
	void gfx_descriptor_set_destroy(gfx_descriptor_set_t* set);

#ifdef __cplusplus
}
#endif

#endif // GFX_CONTEXT_H