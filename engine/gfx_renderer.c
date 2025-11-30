#include "gfx_renderer.h"
#include "gfx_context.h"

#include <edge_allocator.h>
#include <edge_vector.h>

#include <vulkan/vulkan.h>
#include <volk.h>

#define GFX_RENDERER_FRAME_OVERLAP 3

struct gfx_renderer_frame {
	gfx_semaphore_t image_available;
	gfx_semaphore_t rendering_finished;
	gfx_fence_t fence;

	gfx_cmd_buf_t cmd_buf;
	bool is_recording;
};

struct gfx_renderer {
	const edge_allocator_t* alloc;
	const gfx_queue_t* queue;

	gfx_cmd_pool_t cmd_pool;

	gfx_query_pool_t frame_timestamp;
	double timestamp_freq;
	double gpu_delta_time;

	gfx_descriptor_set_layout_t descriptor_layout;
	gfx_descriptor_pool_t descriptor_pool;
	gfx_descriptor_set_t descriptor_set;
	gfx_pipeline_layout_t pipeline_layout;

	gfx_swapchain_t swapchain;
	gfx_image_t swapchain_images[8];
	u32 active_image_index;

	struct gfx_renderer_frame frames[GFX_RENDERER_FRAME_OVERLAP];
	struct gfx_renderer_frame* active_frame;
	u32 frame_number;

	gfx_semaphore_t* acquired_semaphore;

	edge_vector_t* write_descriptor_sets;
	edge_vector_t* image_descriptors;
	edge_vector_t* buffer_descriptors;
};

static void gfx_image_access_state_by_layout(VkImageLayout layout, VkPipelineStageFlags2KHR* out_stage, VkAccessFlags2* out_access) {
	switch (layout)
	{
	case VK_IMAGE_LAYOUT_UNDEFINED:
		*out_stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		*out_access = VK_ACCESS_2_NONE;
		break;
	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		*out_stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		*out_access = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		break;
	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		*out_stage = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
		*out_access = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		break;
	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
		*out_stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
		*out_access = VK_ACCESS_2_SHADER_READ_BIT;
		break;
	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		*out_stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
		*out_access = VK_ACCESS_2_SHADER_READ_BIT;
		break;
	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		*out_stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		*out_access = VK_ACCESS_2_TRANSFER_READ_BIT;
		break;
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		*out_stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		*out_access = VK_ACCESS_2_TRANSFER_WRITE_BIT;
		break;
	case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
		*out_stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		*out_access = VK_ACCESS_2_NONE;
		break;
	default:
		*out_stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		*out_access = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
		break;
	}
}

static VkImageMemoryBarrier2KHR gfx_make_image_barrier(const gfx_image_t* image, VkImageLayout new_layout, VkImageSubresourceRange range) {
	VkImageMemoryBarrier2KHR barrier = { 0 };
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR;
	gfx_image_access_state_by_layout(image->layout, &barrier.srcStageMask, &barrier.srcAccessMask);
	gfx_image_access_state_by_layout(new_layout, &barrier.dstStageMask, &barrier.dstAccessMask);
	barrier.oldLayout = image->layout;
	barrier.newLayout = new_layout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image->handle;
	barrier.subresourceRange = range;

	return barrier;
}

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

	if (!gfx_cmd_pool_create(renderer->queue, &renderer->cmd_pool)) {
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

	if (!gfx_descriptor_set_create(&renderer->descriptor_pool, &renderer->descriptor_layout, &renderer->descriptor_set)) {
		gfx_renderer_destroy(renderer);
		return NULL;
	}

	gfx_pipeline_layout_builder_t pipeline_layout_builder = { 0 };
	gfx_pipeline_layout_builder_add_layout(&renderer->descriptor_layout, &pipeline_layout_builder);
	gfx_pipeline_layout_builder_add_range(VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT, 0u, props->limits.maxPushConstantsSize, &pipeline_layout_builder);

	if (!gfx_pipeline_layout_create(&pipeline_layout_builder, &renderer->pipeline_layout)) {
		gfx_renderer_destroy(renderer);
		return NULL;
	}

	gfx_swapchain_create_info_t swapchain_create_info = { 0 };
	if (!gfx_swapchain_create(&swapchain_create_info, &renderer->swapchain)) {
		gfx_renderer_destroy(renderer);
		return NULL;
	}

	if (!gfx_swapchain_get_images(&renderer->swapchain, renderer->swapchain_images)) {
		gfx_renderer_destroy(renderer);
		return NULL;
	}

	for (i32 i = 0; i < GFX_RENDERER_FRAME_OVERLAP; ++i) {
		struct gfx_renderer_frame* frame = &renderer->frames[i];

		if (!gfx_semaphore_create(VK_SEMAPHORE_TYPE_BINARY, 0, &frame->image_available)) {
			gfx_renderer_destroy(renderer);
			return NULL;
		}

		if (!gfx_semaphore_create(VK_SEMAPHORE_TYPE_BINARY, 0, &frame->rendering_finished)) {
			gfx_renderer_destroy(renderer);
			return NULL;
		}

		if (!gfx_fence_create(VK_FENCE_CREATE_SIGNALED_BIT, &frame->fence)) {
			gfx_renderer_destroy(renderer);
			return NULL;
		}

		if (!gfx_cmd_buf_create(&renderer->cmd_pool, &frame->cmd_buf)) {
			gfx_renderer_destroy(renderer);
			return NULL;
		}
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

	for (i32 i = 0; i < GFX_RENDERER_FRAME_OVERLAP; ++i) {
		struct gfx_renderer_frame* frame = &renderer->frames[i];

		gfx_cmd_buf_destroy(&frame->cmd_buf);
		gfx_fence_destroy(&frame->fence);
		gfx_semaphore_destroy(&frame->rendering_finished);
		gfx_semaphore_destroy(&frame->image_available);
	}

	gfx_swapchain_destroy(&renderer->swapchain);
	gfx_pipeline_layout_destroy(&renderer->pipeline_layout);
	gfx_descriptor_set_destroy(&renderer->descriptor_set);
	gfx_descriptor_pool_destroy(&renderer->descriptor_pool);
	gfx_descriptor_set_layout_destroy(&renderer->descriptor_layout);
	gfx_query_pool_destroy(&renderer->frame_timestamp);
	gfx_cmd_pool_destroy(&renderer->cmd_pool);

	const edge_allocator_t* alloc = renderer->alloc;
	edge_allocator_free(alloc, renderer);
}

bool gfx_renderer_frame_begin(gfx_renderer_t* renderer) {
	if (!renderer) {
		return false;
	}

	bool surface_updated = false;
	if (gfx_swapchain_is_outdated(&renderer->swapchain)) {
		gfx_queue_wait_idle(renderer->queue);

		gfx_swapchain_update(&renderer->swapchain);

		renderer->active_frame = NULL;
		renderer->active_image_index = 0;
		surface_updated = true;
	}

	struct gfx_renderer_frame* current_frame = &renderer->frames[renderer->frame_number % GFX_RENDERER_FRAME_OVERLAP];
	if (!current_frame->is_recording) {
		gfx_fence_wait(&current_frame->fence, 1000000000ull);
		gfx_fence_reset(&current_frame->fence);

		gfx_cmd_reset(&current_frame->cmd_buf);
		current_frame->is_recording = gfx_cmd_begin(&current_frame->cmd_buf);
		// TODO: Free resources
	}

	if (!current_frame->is_recording) {
		return false;
	}

	renderer->acquired_semaphore = &current_frame->image_available;

	if (!gfx_swapchain_acquire_next_image(&renderer->swapchain, 1000000000ull, renderer->acquired_semaphore, &renderer->active_image_index)) {
		return false;
	}

	renderer->active_frame = current_frame;

	u64 timestamps[2] = { 0 };
	if (gfx_query_pool_get_data(&renderer->frame_timestamp, 0, timestamps)) {
		u64 elapsed_time = timestamps[1] - timestamps[0];
		if (timestamps[1] <= timestamps[0]) {
			elapsed_time = 0ull;
		}

		renderer->gpu_delta_time = (double)elapsed_time * renderer->timestamp_freq / 1000000.0;
	}

	gfx_cmd_reset_query(&current_frame->cmd_buf, &renderer->frame_timestamp, 0u, 2u);
	gfx_cmd_write_timestamp(&current_frame->cmd_buf, &renderer->frame_timestamp, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0u);

	gfx_cmd_bind_descriptor(&current_frame->cmd_buf, &renderer->pipeline_layout, &renderer->descriptor_set, VK_PIPELINE_BIND_POINT_GRAPHICS);
	gfx_cmd_bind_descriptor(&current_frame->cmd_buf, &renderer->pipeline_layout, &renderer->descriptor_set, VK_PIPELINE_BIND_POINT_COMPUTE);

	return true;
}

bool gfx_renderer_frame_end(gfx_renderer_t* renderer) {
	if (!renderer) {
		return false;
	}

	struct gfx_renderer_frame* current_frame = renderer->active_frame;
	if (!current_frame || !current_frame->is_recording) {
		return false;
	}

	gfx_image_t* current_image = &renderer->swapchain_images[renderer->active_image_index];

	VkImageSubresourceRange subresource_range = { 
		.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		.baseMipLevel = 0u,
		.levelCount = 1u,
		.baseArrayLayer = 0u,
		.layerCount = 1u
	};

	VkImageMemoryBarrier2KHR image_barrier = gfx_make_image_barrier(current_image, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, subresource_range);

	VkDependencyInfo dependency_info = {
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.pNext = NULL,
		.imageMemoryBarrierCount = 1u,
		.pImageMemoryBarriers = &image_barrier
	};

	vkCmdPipelineBarrier2KHR(current_frame->cmd_buf.handle, &dependency_info);

	if (!edge_vector_empty(renderer->write_descriptor_sets)) {
		gfx_updete_descriptors((const VkWriteDescriptorSet*)edge_vector_data(renderer->write_descriptor_sets), edge_vector_size(renderer->write_descriptor_sets));

		edge_vector_clear(renderer->write_descriptor_sets);
		edge_vector_clear(renderer->image_descriptors);
		edge_vector_clear(renderer->buffer_descriptors);
	}

	gfx_cmd_write_timestamp(&current_frame->cmd_buf, &renderer->frame_timestamp, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 1u);
	gfx_cmd_end(&current_frame->cmd_buf);

	renderer->active_frame = NULL;
	current_frame->is_recording = false;

	VkSemaphoreSubmitInfo wait_semaphores[2] = { 0 };
	wait_semaphores[0].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	wait_semaphores[0].semaphore = renderer->acquired_semaphore->handle;
	wait_semaphores[0].stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

	VkSemaphoreSubmitInfo signal_semaphores[2] = { 0 };
	signal_semaphores[0].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	signal_semaphores[0].semaphore = current_frame->rendering_finished.handle;
	signal_semaphores[0].stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

	VkCommandBufferSubmitInfo cmd_submit_info = { 0 };
	cmd_submit_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	cmd_submit_info.commandBuffer = current_frame->cmd_buf.handle;

	VkSubmitInfo2KHR submit_info = { 0 };
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR;
	submit_info.waitSemaphoreInfoCount = 1u;
	submit_info.pWaitSemaphoreInfos = wait_semaphores;
	submit_info.commandBufferInfoCount = 1u;
	submit_info.pCommandBufferInfos = &cmd_submit_info;
	submit_info.signalSemaphoreInfoCount = 1u;
	submit_info.pSignalSemaphoreInfos = signal_semaphores;

	if (!gfx_queue_submit(renderer->queue, &current_frame->fence, &submit_info)) {
		return false;
	}

	VkPresentInfoKHR present_info = { 0 };
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.waitSemaphoreCount = 1u;
	present_info.pWaitSemaphores = &signal_semaphores[0].semaphore;
	present_info.swapchainCount = 1u;
	present_info.pSwapchains = &renderer->swapchain.handle;
	present_info.pImageIndices = &renderer->active_image_index;

	if (!gfx_queue_present(renderer->queue, &present_info)) {
		return false;
	}

	renderer->frame_number++;

	return true;
}