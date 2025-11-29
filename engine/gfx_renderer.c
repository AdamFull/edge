#include "gfx_renderer.h"
#include "gfx_context.h"

#include <edge_allocator.h>
#include <edge_vector.h>

#include <vulkan/vulkan.h>

struct gfx_renderer {
	const edge_allocator_t* alloc;
	const gfx_queue_t* queue;

	gfx_command_pool_t cmd_pool;

	gfx_query_pool_t frame_timestamp;
	double timestamp_freq;

	gfx_descriptor_set_layout_t descriptor_layout;
	gfx_descriptor_pool_t descriptor_pool;
	gfx_descriptor_set_t descriptor_set;

	edge_vector_t* write_descriptor_sets;
	edge_vector_t* image_descriptors;
	edge_vector_t* buffer_descriptors;
};

gfx_renderer_t* gfx_renderer_create(const gfx_renderer_create_info_t* create_info) {
	if (!create_info || !create_info->alloc || !create_info->main_queue) {
		return NULL;
	}

	gfx_renderer_t* renderer = (gfx_renderer_t*)edge_allocator_malloc(create_info->alloc, sizeof(gfx_renderer_t));
	if (!renderer) {
		return NULL;
	}

	memset(renderer, 0, sizeof(gfx_renderer_t));

	renderer->alloc = create_info->alloc;
	renderer->queue = create_info->main_queue;

	if (!gfx_command_pool_create(renderer->queue, &renderer->cmd_pool)) {
		gfx_renderer_destroy(renderer);
		return NULL;
	}

	if (!gfx_query_pool_create(VK_QUERY_TYPE_TIMESTAMP, 1, &renderer->frame_timestamp)) {
		gfx_renderer_destroy(renderer);
		return NULL;
	}

	gfx_query_pool_reset(&renderer->frame_timestamp);

	const VkPhysicalDeviceProperties* props = gfx_get_adapter_props();
	renderer->timestamp_freq = props->limits.timestampPeriod;
	
	gfx_descriptor_layout_builder_t descriptor_layout_builder = { 0 };

	VkDescriptorSetLayoutBinding samplers_binding = {
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
		.descriptorCount = 65535,
		.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT,
		.pImmutableSamplers = NULL
	};

	VkDescriptorSetLayoutBinding srv_image_binding = {
		.binding = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
		.descriptorCount = 65535,
		.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT,
		.pImmutableSamplers = NULL
	};

	VkDescriptorSetLayoutBinding uav_image_binding = {
		.binding = 2,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		.descriptorCount = 65535,
		.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT,
		.pImmutableSamplers = NULL
	};

	VkDescriptorBindingFlags descriptor_binding_flags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

	gfx_descriptor_layout_builder_add_binding(samplers_binding, descriptor_binding_flags, &descriptor_layout_builder);
	gfx_descriptor_layout_builder_add_binding(srv_image_binding, descriptor_binding_flags, &descriptor_layout_builder);
	gfx_descriptor_layout_builder_add_binding(uav_image_binding, descriptor_binding_flags, &descriptor_layout_builder);

	if (!gfx_descriptor_set_layout_create(&descriptor_layout_builder, &renderer->descriptor_layout)) {
		gfx_renderer_destroy(renderer);
		return NULL;
	}

	if (!gfx_descriptor_pool_create(renderer->descriptor_layout.descriptor_sizes, &renderer->descriptor_pool)) {
		gfx_renderer_destroy(renderer);
		return NULL;
	}

	renderer->write_descriptor_sets = edge_vector_create(create_info->alloc, sizeof(VkWriteDescriptorSet), 256);
	if (!renderer->write_descriptor_sets) {
		gfx_renderer_destroy(renderer);
		return NULL;
	}

	renderer->image_descriptors = edge_vector_create(create_info->alloc, sizeof(VkDescriptorImageInfo), 256);
	if (!renderer->image_descriptors) {
		gfx_renderer_destroy(renderer);
		return NULL;
	}

	renderer->buffer_descriptors = edge_vector_create(create_info->alloc, sizeof(VkDescriptorBufferInfo), 256);
	if (!renderer->buffer_descriptors) {
		gfx_renderer_destroy(renderer);
		return NULL;
	}

	return renderer;
}

void gfx_renderer_destroy(gfx_renderer_t* renderer) {
	if (!renderer) {
		return;
	}

	if (renderer->write_descriptor_sets) {
		edge_vector_destroy(renderer->write_descriptor_sets);
	}

	if (renderer->image_descriptors) {
		edge_vector_destroy(renderer->image_descriptors);
	}

	if (renderer->buffer_descriptors) {
		edge_vector_destroy(renderer->buffer_descriptors);
	}

	gfx_command_pool_destroy(&renderer->cmd_pool);

	const edge_allocator_t* alloc = renderer->alloc;
	edge_allocator_free(alloc, renderer);
}