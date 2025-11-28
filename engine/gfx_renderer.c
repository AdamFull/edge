#include "gfx_renderer.h"
#include "gfx_context.h"

#include <edge_allocator.h>
#include <edge_vector.h>

#include <vulkan/vulkan.h>

struct gfx_renderer {
	const edge_allocator_t* alloc;
	const gfx_context_t* ctx;
	const gfx_queue_t* queue;

	edge_vector_t* write_descriptor_sets;
	edge_vector_t* image_descriptors;
	edge_vector_t* buffer_descriptors;
};

gfx_renderer_t* gfx_renderer_create(const gfx_renderer_create_info_t* create_info) {
	if (!create_info || !create_info->alloc || !create_info->ctx || !create_info->main_queue) {
		return NULL;
	}

	gfx_renderer_t* renderer = (gfx_renderer_t*)edge_allocator_malloc(create_info->alloc, sizeof(gfx_renderer_t));
	if (!renderer) {
		return NULL;
	}

	renderer->alloc = create_info->alloc;
	renderer->ctx = create_info->ctx;
	renderer->queue = create_info->main_queue;

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

	const edge_allocator_t* alloc = renderer->alloc;
	edge_allocator_free(alloc, renderer);
}