#ifndef GFX_CONTEXT_H
#define GFX_CONTEXT_H

#include "gfx_enum.h"

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

	typedef struct edge_allocator edge_allocator_t;
	typedef struct platform_context platform_context_t;

	typedef struct gfx_context gfx_context_t;

	typedef struct gfx_instance {
		VkInstance handle;
		VkDebugUtilsMessengerEXT debug_messenger;

		const char* enabled_layers[32];
		uint32_t enabled_layer_count;

		const char* enabled_extensions[32];
		uint32_t enabled_extension_count;

		bool validation_enabled;
		bool synchronization_validation_enabled;
	} gfx_instance_t;

	typedef struct gfx_adapter {
		VkPhysicalDevice handle;

		VkPhysicalDeviceProperties properties;
		VkPhysicalDeviceFeatures features;

		const char* enabled_extensions[64];
		uint32_t enabled_extension_count;
	} gfx_adapter_t;

	typedef struct gfx_device {
		VkDevice handle;

		bool get_memory_requirements_2_enabled;
		bool memory_budget_enabled;
		bool memory_priority_enabled;
		bool bind_memory_enabled;
		bool amd_device_coherent_memory_enabled;
	} gfx_device_t;

	typedef struct gfx_allocator {
		VmaAllocator handle;
	} gfx_allocator_t;

	typedef struct gfx_context_create_info {
		const edge_allocator_t* alloc;
		platform_context_t* platform_context;
	} gfx_context_create_info_t;

	gfx_context_t* gfx_context_create(const gfx_context_create_info_t* cteate_info);
	void gfx_context_destroy(gfx_context_t* context);

#ifdef __cplusplus
}
#endif

#endif // GFX_CONTEXT_H