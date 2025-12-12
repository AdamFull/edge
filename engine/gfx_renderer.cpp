#include "gfx_renderer.h"
#include "gfx_context.h"

#include <array.hpp>
#include <free_index_list.hpp>

#include <atomic>

#include <vulkan/vulkan.h>

namespace edge::gfx {
	constexpr usize RENDERER_FRAME_OVERLAP = 3;

	constexpr usize RENDERER_UAV_MAX = 16;

	constexpr usize RENDERER_SAMPLER_SLOT = 0;
	constexpr usize RENDERER_SRV_SLOT = 1;
	constexpr usize RENDERER_UAV_SLOT = 2;

	constexpr usize RENDERER_HANDLE_MAX = 65535;

	struct Resource {
		ResourceType type;
		union {
			Image image;
			Buffer buffer;
		};

		ImageView srv;
		u32 srv_index;

		ImageView uav[RENDERER_UAV_MAX];
		u32 uav_index;
	};

	struct RendererFrame {
		Semaphore image_available;
		Semaphore rendering_finished;
		Fence fence;

		CmdBuf cmd_buf;
		bool is_recording;

		Array<Resource> free_resources;
	};

	struct Renderer {
		const Allocator* alloc;
		const Queue* queue;

		CmdPool cmd_pool;

		QueryPool frame_timestamp;
		double timestamp_freq;
		double gpu_delta_time;

		DescriptorSetLayout descriptor_layout;
		DescriptorPool descriptor_pool;
		DescriptorSet descriptor_set;
		PipelineLayout pipeline_layout;

		Swapchain swapchain;
		Image swapchain_images[8];
		ImageView swapchain_image_views[8];
		u32 active_image_index;

		RendererFrame frames[RENDERER_FRAME_OVERLAP];
		RendererFrame* active_frame;
		u32 frame_number;

		HandlePool<Resource> resource_handle_pool;
		Handle backbuffer_handle;

		FreeIndexList sampler_indices_list;
		FreeIndexList srv_indices_list;
		FreeIndexList uav_indices_list;

		PipelineBarrierBuilder barrier_builder;

		Semaphore* acquired_semaphore;

		Array<VkWriteDescriptorSet> write_descriptor_sets;
		Array<VkDescriptorImageInfo> image_descriptors;
		Array<VkDescriptorBufferInfo> buffer_descriptors;
	};

	inline bool is_depth_format(VkFormat format) {
		return format == VK_FORMAT_D16_UNORM || format == VK_FORMAT_D32_SFLOAT;
	}

	inline bool is_depth_stencil_format(VkFormat format) {
		return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D16_UNORM_S8_UINT ||
			format == VK_FORMAT_D24_UNORM_S8_UINT;
	}

	inline bool renderer_frame_release_pending_resources(Renderer* renderer, RendererFrame* frame) {
		if (!renderer || !frame) {
			return false;
		}

		for (auto& resource : frame->free_resources) {
			if (resource.type != ResourceType::Unknown) {
				continue;
			}

			if (resource.type == ResourceType::Image) {
				image_view_destroy(&resource.srv);
				free_index_list_free(&renderer->srv_indices_list, resource.srv_index);

				for (i32 j = 0; j < resource.image.level_count; ++j) {
					image_view_destroy(&resource.uav[j]);
					free_index_list_free(&renderer->uav_indices_list, resource.uav_index + j);
				}

				image_destroy(&resource.image);
			} 
			else if (resource.type == ResourceType::Buffer) {
				buffer_destroy(&resource.buffer);
			}
		}

		array_clear(&frame->free_resources);

		return true;
	}

	inline bool renderer_frame_init(Renderer* renderer, RendererFrame* frame) {
		if (!renderer || !frame) {
			return false;
		}

		if (!semaphore_create(VK_SEMAPHORE_TYPE_BINARY, 0, &frame->image_available)) {
			return false;
		}

		if (!semaphore_create(VK_SEMAPHORE_TYPE_BINARY, 0, &frame->rendering_finished)) {
			return false;
		}

		if (!fence_create(VK_FENCE_CREATE_SIGNALED_BIT, &frame->fence)) {
			return false;
		}

		if (!cmd_buf_create(&renderer->cmd_pool, &frame->cmd_buf)) {
			return false;
		}

		if (!array_create(renderer->alloc, &frame->free_resources, 256)) {
			return false;
		}

		return true;
	}

	static void gfx_renderer_frame_destroy(Renderer* renderer, RendererFrame* frame) {
		if (!renderer || !frame) {
			return;
		}

		renderer_frame_release_pending_resources(renderer, frame);
		array_destroy(&frame->free_resources);

		cmd_buf_destroy(&frame->cmd_buf);
		fence_destroy(&frame->fence);
		semaphore_destroy(&frame->rendering_finished);
		semaphore_destroy(&frame->image_available);
	}

	Renderer* renderer_create(const RendererCreateInfo* create_info) {
		if (!create_info || !create_info->alloc || !create_info->main_queue) {
			return nullptr;
		}

		Renderer* renderer = allocate_zeroed<Renderer>(create_info->alloc);
		if (!renderer) {
			return nullptr;
		}

		renderer->alloc = create_info->alloc;
		renderer->queue = create_info->main_queue;

		if (!cmd_pool_create(renderer->queue, &renderer->cmd_pool)) {
			renderer_destroy(renderer);
			return nullptr;
		}

		if (!query_pool_create(VK_QUERY_TYPE_TIMESTAMP, 1, &renderer->frame_timestamp)) {
			renderer_destroy(renderer);
			return nullptr;
		}

		const VkPhysicalDeviceProperties* props = get_adapter_props();
		renderer->timestamp_freq = props->limits.timestampPeriod;

		DescriptorLayoutBuilder descriptor_layout_builder = {};

		VkDescriptorSetLayoutBinding samplers_binding = {
			.binding = RENDERER_SAMPLER_SLOT,
			.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
			.descriptorCount = RENDERER_HANDLE_MAX,
			.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT,
			.pImmutableSamplers = nullptr
		};

		VkDescriptorSetLayoutBinding srv_image_binding = {
			.binding = RENDERER_SRV_SLOT,
			.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
			.descriptorCount = RENDERER_HANDLE_MAX,
			.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT,
			.pImmutableSamplers = nullptr
		};

		VkDescriptorSetLayoutBinding uav_image_binding = {
			.binding = RENDERER_UAV_SLOT,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			.descriptorCount = RENDERER_HANDLE_MAX,
			.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT,
			.pImmutableSamplers = nullptr
		};

		VkDescriptorBindingFlags descriptor_binding_flags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

		descriptor_layout_builder_add_binding(samplers_binding, descriptor_binding_flags, &descriptor_layout_builder);
		descriptor_layout_builder_add_binding(srv_image_binding, descriptor_binding_flags, &descriptor_layout_builder);
		descriptor_layout_builder_add_binding(uav_image_binding, descriptor_binding_flags, &descriptor_layout_builder);

		if (!descriptor_set_layout_create(&descriptor_layout_builder, &renderer->descriptor_layout)) {
			renderer_destroy(renderer);
			return nullptr;
		}

		if (!descriptor_pool_create(renderer->descriptor_layout.descriptor_sizes, &renderer->descriptor_pool)) {
			renderer_destroy(renderer);
			return nullptr;
		}

		if (!descriptor_set_create(&renderer->descriptor_pool, &renderer->descriptor_layout, &renderer->descriptor_set)) {
			renderer_destroy(renderer);
			return nullptr;
		}

		PipelineLayoutBuilder pipeline_layout_builder = {};
		pipeline_layout_builder_add_layout(&pipeline_layout_builder, &renderer->descriptor_layout);
		pipeline_layout_builder_add_range(&pipeline_layout_builder, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT, 0u, props->limits.maxPushConstantsSize);

		if (!pipeline_layout_create(&pipeline_layout_builder, &renderer->pipeline_layout)) {
			renderer_destroy(renderer);
			return nullptr;
		}

		SwapchainCreateInfo swapchain_create_info = {};
		if (!swapchain_create(&swapchain_create_info, &renderer->swapchain)) {
			renderer_destroy(renderer);
			return nullptr;
		}

		if (!swapchain_get_images(&renderer->swapchain, renderer->swapchain_images)) {
			renderer_destroy(renderer);
			return nullptr;
		}

		for (i32 i = 0; i < renderer->swapchain.image_count; ++i) {
			VkImageSubresourceRange subresource_range = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0u,
				.levelCount = 1u,
				.baseArrayLayer = 0u,
				.layerCount = 1u
			};

			const Image* image = &renderer->swapchain_images[i];
			ImageView* image_view = &renderer->swapchain_image_views[i];

			if (!image_view_create(image, VK_IMAGE_VIEW_TYPE_2D, subresource_range, image_view)) {
				renderer_destroy(renderer);
				return nullptr;
			}
		}

		for (i32 i = 0; i < RENDERER_FRAME_OVERLAP; ++i) {
			RendererFrame* frame = &renderer->frames[i];
			if (!renderer_frame_init(renderer, frame)) {
				renderer_destroy(renderer);
				return nullptr;
			}
		}

		if (!handle_pool_create(create_info->alloc, &renderer->resource_handle_pool, RENDERER_HANDLE_MAX * 2)) {
			renderer_destroy(renderer);
			return nullptr;
		}

		Resource backbuffer_resource = {
			.type = ResourceType::Image
		};
		renderer->backbuffer_handle = handle_pool_allocate_with_data(&renderer->resource_handle_pool, backbuffer_resource);

		if (!free_index_list_create(create_info->alloc, &renderer->sampler_indices_list, RENDERER_HANDLE_MAX)) {
			renderer_destroy(renderer);
			return nullptr;
		}

		if (!free_index_list_create(create_info->alloc, &renderer->srv_indices_list, RENDERER_HANDLE_MAX)) {
			renderer_destroy(renderer);
			return nullptr;
		}

		if (!free_index_list_create(create_info->alloc, &renderer->uav_indices_list, RENDERER_HANDLE_MAX)) {
			renderer_destroy(renderer);
			return nullptr;
		}

		if (!array_create(create_info->alloc, &renderer->write_descriptor_sets, 256)) {
			renderer_destroy(renderer);
			return nullptr;
		}

		if (!array_create(create_info->alloc, &renderer->image_descriptors, 256)) {
			renderer_destroy(renderer);
			return nullptr;
		}

		if (!array_create(create_info->alloc, &renderer->buffer_descriptors, 256)) {
			renderer_destroy(renderer);
			return nullptr;
		}

		return renderer;
	}

	void renderer_destroy(Renderer* renderer) {
		if (!renderer) {
			return;
		}

		queue_wait_idle(renderer->queue);

		array_destroy(&renderer->write_descriptor_sets);
		array_destroy(&renderer->image_descriptors);
		array_destroy(&renderer->buffer_descriptors);

		for (auto entry : renderer->resource_handle_pool) {
			Resource* resource = entry.element;
			if (resource->type == ResourceType::Unknown) {
				continue;
			}

			// TODO: Ignore backbuffer
			if (resource->type == ResourceType::Image) {
				image_view_destroy(&resource->srv);
				free_index_list_free(&renderer->srv_indices_list, resource->srv_index);

				for (i32 mip = 0; mip < resource->image.level_count; ++mip) {
					image_view_destroy(&resource->uav[mip]);
					free_index_list_free(&renderer->uav_indices_list, resource->uav_index + mip);
				}

				image_destroy(&resource->image);
			}
			else if (resource->type == ResourceType::Buffer) {
				buffer_destroy(&resource->buffer);
			}
		}

		free_index_list_destroy(&renderer->uav_indices_list);
		free_index_list_destroy(&renderer->srv_indices_list);
		free_index_list_destroy(&renderer->sampler_indices_list);

		handle_pool_destroy(&renderer->resource_handle_pool);

		for (i32 i = 0; i < RENDERER_FRAME_OVERLAP; ++i) {
			RendererFrame* frame = &renderer->frames[i];
			gfx_renderer_frame_destroy(renderer, frame);
		}

		for (i32 i = 0; i < renderer->swapchain.image_count; ++i) {
			ImageView* image_view = &renderer->swapchain_image_views[i];
			image_view_destroy(image_view);
		}

		swapchain_destroy(&renderer->swapchain);
		pipeline_layout_destroy(&renderer->pipeline_layout);
		descriptor_set_destroy(&renderer->descriptor_set);
		descriptor_pool_destroy(&renderer->descriptor_pool);
		descriptor_set_layout_destroy(&renderer->descriptor_layout);
		query_pool_destroy(&renderer->frame_timestamp);
		cmd_pool_destroy(&renderer->cmd_pool);

		const Allocator* alloc = renderer->alloc;
		deallocate(alloc, renderer);
	}

	Handle renderer_add_resource(Renderer* renderer) {
		if (!renderer) {
			return HANDLE_INVALID;
		}

		if (handle_pool_is_full(&renderer->resource_handle_pool)) {
			return HANDLE_INVALID;
		}

		return handle_pool_allocate(&renderer->resource_handle_pool);
	}

	bool renderer_setup_image_resource(Renderer* renderer, Handle handle, const Image* image) {
		if (!renderer || !image) {
			return false;
		}

		Resource* resource = handle_pool_get(&renderer->resource_handle_pool, handle);
#if 0
		if (!resource || resource->type != ResourceType::Image) {
			return false;
		}
#else
		resource->type = ResourceType::Image;
#endif

		memcpy(&resource->image, image, sizeof(Image));

		Image* image_source = &resource->image;

		VkImageAspectFlags image_aspect = (image_source->usage_flags & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

		VkImageViewType view_type = {};
		if (image_source->extent.depth > 1) {
			view_type = VK_IMAGE_VIEW_TYPE_3D;
		}
		else if (image_source->extent.height > 1) {
			if (image_source->layer_count > 1) {
				view_type = (image_source->face_count > 1) ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
			}
			else {
				view_type = (image_source->face_count > 1) ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;
			}
		}
		else if (image_source->extent.width > 1) {
			view_type = image_source->layer_count > 1 ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D;
		}

		if (image_source->usage_flags & VK_IMAGE_USAGE_SAMPLED_BIT) {
			VkImageSubresourceRange srv_subresource_range = {
				.aspectMask = image_aspect,
				.baseMipLevel = 0u,
				.levelCount = image_source->level_count,
				.baseArrayLayer = 0u,
				.layerCount = image_source->layer_count * image_source->face_count
			};

			if (!image_view_create(image_source, view_type, srv_subresource_range, &resource->srv)) {
				return false;
			}

			if (!free_index_list_allocate(&renderer->srv_indices_list, &resource->srv_index)) {
				return false;
			}
		}

		if (image_source->usage_flags & VK_IMAGE_USAGE_STORAGE_BIT) {
			VkImageSubresourceRange uav_subresource_range = {
				.aspectMask = image_aspect,
				.baseMipLevel = 0u,
				.levelCount = 1u,
				.baseArrayLayer = 0u,
				.layerCount = image_source->layer_count * image_source->face_count
			};

			for (i32 i = 0; i < image_source->level_count; ++i) {
				uav_subresource_range.baseMipLevel = i;

				if (!image_view_create(image_source, view_type, uav_subresource_range, &resource->uav[i])) {
					return false;
				}

				u32 uav_index;
				if (!free_index_list_allocate(&renderer->uav_indices_list, &uav_index)) {
					return false;
				}

				if (i == 0) {
					resource->uav_index = uav_index;
				}
			}
		}

		bool is_attachment = (image_source->usage_flags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) || (image_source->usage_flags & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
		if (!is_attachment && image_source->layout != VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR) {
			PipelineBarrierBuilder builder = {};

			VkImageSubresourceRange subresource_range = {
				.aspectMask = image_aspect,
				.baseMipLevel = 0u,
				.levelCount = image_source->level_count,
				.baseArrayLayer = 0u,
				.layerCount = image_source->layer_count
			};

			// TODO: Make batching updates
			pipeline_barrier_add_image(&builder, image_source, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR, subresource_range);
			cmd_pipeline_barrier(&renderer->active_frame->cmd_buf, &builder);

			image_source->layout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR;
		}

		VkWriteDescriptorSet descriptor_write = {};
		descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptor_write.dstSet = renderer->descriptor_set.handle;
		descriptor_write.descriptorCount = 1u;

		if (image_source->usage_flags & VK_IMAGE_USAGE_SAMPLED_BIT) {
			VkDescriptorImageInfo image_descriptor = {};
			image_descriptor.imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR;
			image_descriptor.imageView = resource->srv.handle;
			array_push_back(&renderer->image_descriptors, image_descriptor);

			descriptor_write.dstBinding = RENDERER_SRV_SLOT;
			descriptor_write.dstArrayElement = resource->srv_index;
			descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			descriptor_write.pImageInfo = array_back(&renderer->image_descriptors);
			array_push_back(&renderer->write_descriptor_sets, descriptor_write);
		}

		if (image_source->usage_flags & VK_IMAGE_USAGE_STORAGE_BIT) {
			descriptor_write.dstBinding = RENDERER_UAV_SLOT;
			descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

			for (i32 mip = 0; mip < image_source->level_count; ++mip) {
				VkDescriptorImageInfo image_descriptor = {};
				image_descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
				image_descriptor.imageView = resource->uav[mip].handle;
				array_push_back(&renderer->image_descriptors, image_descriptor);

				descriptor_write.dstArrayElement = resource->uav_index + mip;
				descriptor_write.pImageInfo = array_back(&renderer->image_descriptors);
				array_push_back(&renderer->write_descriptor_sets, descriptor_write);
			}
		}

		return true;
	}

	bool renderer_setup_buffer_resource(Renderer* renderer, Handle handle, const Buffer* buffer) {
		if (!renderer || !buffer) {
			return false;
		}

		return true;
	}

	void renderer_free_resource(Renderer* renderer, Handle handle) {
		if (!renderer) {
			return;
		}

		if (handle_pool_is_valid(&renderer->resource_handle_pool, handle)) {
			if (renderer->active_frame) {
				Resource* resource = handle_pool_get(&renderer->resource_handle_pool, handle);
				if (resource->type != ResourceType::Unknown) {
					array_push_back(&renderer->active_frame->free_resources, *resource);
				}
			}

			handle_pool_free(&renderer->resource_handle_pool, handle);
		}
	}

	bool renderer_frame_begin(Renderer* renderer) {
		if (!renderer) {
			return false;
		}

		bool surface_updated = false;
		if (swapchain_is_outdated(&renderer->swapchain)) {

			if (renderer->queue) {
				queue_wait_idle(renderer->queue);
			}

			if (!swapchain_update(&renderer->swapchain)) {
				return false;
			}

			if (!swapchain_get_images(&renderer->swapchain, renderer->swapchain_images)) {
				return false;
			}

			for (i32 i = 0; i < renderer->swapchain.image_count; ++i) {
				VkImageSubresourceRange subresource_range = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.baseMipLevel = 0u,
					.levelCount = 1u,
					.baseArrayLayer = 0u,
					.layerCount = 1u
				};

				const Image* image = &renderer->swapchain_images[i];
				ImageView* image_view = &renderer->swapchain_image_views[i];

				if (image_view->handle != VK_NULL_HANDLE) {
					image_view_destroy(image_view);
					image_view->handle = VK_NULL_HANDLE;
				}

				if (!image_view_create(image, VK_IMAGE_VIEW_TYPE_2D, subresource_range, image_view)) {
					return false;
				}
			}

			renderer->active_frame = nullptr;
			renderer->active_image_index = 0;
			surface_updated = true;
		}

		RendererFrame* current_frame = &renderer->frames[renderer->frame_number % RENDERER_FRAME_OVERLAP];
		if (!current_frame->is_recording) {
			fence_wait(&current_frame->fence, 1000000000ull);
			fence_reset(&current_frame->fence);

			cmd_reset(&current_frame->cmd_buf);
			current_frame->is_recording = cmd_begin(&current_frame->cmd_buf);

			renderer_frame_release_pending_resources(renderer, current_frame);
		}

		if (!current_frame->is_recording) {
			return false;
		}

		renderer->acquired_semaphore = &current_frame->image_available;

		if (!swapchain_acquire_next_image(&renderer->swapchain, 1000000000ull, renderer->acquired_semaphore, &renderer->active_image_index)) {
			return false;
		}

		renderer->active_frame = current_frame;

		// Update backbuffer resource
		Resource* backbuffer_resource = handle_pool_get(&renderer->resource_handle_pool, renderer->backbuffer_handle);
		backbuffer_resource->image = renderer->swapchain_images[renderer->active_image_index];

		if (renderer->frame_number > 0) {
			u64 timestamps[2] = {};
			if (query_pool_get_data(&renderer->frame_timestamp, 0, timestamps)) {
				u64 elapsed_time = timestamps[1] - timestamps[0];
				if (timestamps[1] <= timestamps[0]) {
					elapsed_time = 0ull;
				}

				renderer->gpu_delta_time = (double)elapsed_time * renderer->timestamp_freq / 1000000.0;
			}
		}

		cmd_reset_query(&current_frame->cmd_buf, &renderer->frame_timestamp, 0u, 2u);
		cmd_write_timestamp(&current_frame->cmd_buf, &renderer->frame_timestamp, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0u);

		cmd_bind_descriptor(&current_frame->cmd_buf, &renderer->pipeline_layout, &renderer->descriptor_set, VK_PIPELINE_BIND_POINT_GRAPHICS);
		cmd_bind_descriptor(&current_frame->cmd_buf, &renderer->pipeline_layout, &renderer->descriptor_set, VK_PIPELINE_BIND_POINT_COMPUTE);

		return true;
	}

	bool renderer_frame_end(Renderer* renderer) {
		if (!renderer) {
			return false;
		}

		RendererFrame* current_frame = renderer->active_frame;
		if (!current_frame || !current_frame->is_recording) {
			return false;
		}

		Resource* backbuffer_resource = handle_pool_get(&renderer->resource_handle_pool, renderer->backbuffer_handle);
		if (backbuffer_resource) {
			if (backbuffer_resource->image.layout != VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
				PipelineBarrierBuilder barrier_builder = {};

				VkImageSubresourceRange subresource_range = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0u,
				.levelCount = 1u,
				.baseArrayLayer = 0u,
				.layerCount = 1u
				};

				pipeline_barrier_add_image(&barrier_builder, &backbuffer_resource->image, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, subresource_range);
				cmd_pipeline_barrier(&current_frame->cmd_buf, &barrier_builder);

				backbuffer_resource->image.layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			}
		}

		if (!array_empty(&renderer->write_descriptor_sets)) {
			updete_descriptors(array_data(&renderer->write_descriptor_sets), array_size(&renderer->write_descriptor_sets));

			array_clear(&renderer->write_descriptor_sets);
			array_clear(&renderer->image_descriptors);
			array_clear(&renderer->buffer_descriptors);
		}

		cmd_write_timestamp(&current_frame->cmd_buf, &renderer->frame_timestamp, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 1u);
		cmd_end(&current_frame->cmd_buf);

		renderer->active_frame = nullptr;
		current_frame->is_recording = false;

		VkSemaphoreSubmitInfo wait_semaphores[2] = {};
		wait_semaphores[0].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
		wait_semaphores[0].semaphore = renderer->acquired_semaphore->handle;
		wait_semaphores[0].stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

		VkSemaphoreSubmitInfo signal_semaphores[2] = {};
		signal_semaphores[0].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
		signal_semaphores[0].semaphore = current_frame->rendering_finished.handle;
		signal_semaphores[0].stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

		VkCommandBufferSubmitInfo cmd_submit_info = {};
		cmd_submit_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
		cmd_submit_info.commandBuffer = current_frame->cmd_buf.handle;

		VkSubmitInfo2KHR submit_info = {};
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR;
		submit_info.waitSemaphoreInfoCount = 1u;
		submit_info.pWaitSemaphoreInfos = wait_semaphores;
		submit_info.commandBufferInfoCount = 1u;
		submit_info.pCommandBufferInfos = &cmd_submit_info;
		submit_info.signalSemaphoreInfoCount = 1u;
		submit_info.pSignalSemaphoreInfos = signal_semaphores;

		if (!queue_submit(renderer->queue, &current_frame->fence, &submit_info)) {
			return false;
		}

		VkPresentInfoKHR present_info = {};
		present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		present_info.waitSemaphoreCount = 1u;
		present_info.pWaitSemaphores = &signal_semaphores[0].semaphore;
		present_info.swapchainCount = 1u;
		present_info.pSwapchains = &renderer->swapchain.handle;
		present_info.pImageIndices = &renderer->active_image_index;

		if (!queue_present(renderer->queue, &present_info)) {
			return false;
		}

		renderer->frame_number++;

		return true;
	}
}