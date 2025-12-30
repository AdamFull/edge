#include "gfx_renderer.h"
#include "gfx_context.h"

#include <array.hpp>
#include <math.hpp>
#include <logger.hpp>

#include <atomic>
#include <utility>

#include <vulkan/vulkan.h>
#include <volk.h>

namespace edge::gfx {
	static bool is_depth_format(VkFormat format) {
		return format == VK_FORMAT_D16_UNORM || format == VK_FORMAT_D32_SFLOAT;
	}

	static bool is_depth_stencil_format(VkFormat format) {
		return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D16_UNORM_S8_UINT ||
			format == VK_FORMAT_D24_UNORM_S8_UINT;
	}

	bool RendererFrame::create(NotNull<Renderer*> renderer) noexcept {
		BufferCreateInfo buffer_create_info = {
				.size = RENDERER_UPDATE_STAGING_ARENA_SIZE,
				.alignment = 1,
				.flags = BUFFER_FLAG_STAGING
		};

		if (!staging_memory.create(buffer_create_info)) {
			return false;
		}

		temp_staging_memory.reserve(renderer->alloc, 128);

		if (!image_available.create(VK_SEMAPHORE_TYPE_BINARY, 0)) {
			return false;
		}

		if (!rendering_finished.create(VK_SEMAPHORE_TYPE_BINARY, 0)) {
			return false;
		}

		if (!fence.create(VK_FENCE_CREATE_SIGNALED_BIT)) {
			return false;
		}

		if (!cmd.create(renderer->cmd_pool)) {
			return false;
		}

		if (!free_resources.reserve(renderer->alloc, 256)) {
			return false;
		}

		return true;
	}

	void RendererFrame::destroy(NotNull<Renderer*> renderer) noexcept {
		staging_memory.destroy();

		for (auto& buffer : temp_staging_memory) {
			buffer.destroy();
		}
		temp_staging_memory.destroy(renderer->alloc);

		release_resources(renderer);
		free_resources.destroy(renderer->alloc);

		cmd.destroy();
		fence.destroy();
		rendering_finished.destroy();
		image_available.destroy();
	}

	void RendererFrame::release_resources(NotNull<Renderer*> renderer) noexcept {
		for (auto& resource : free_resources) {
			if (resource.type == ResourceType::Image) {
				resource.srv.destroy();
				renderer->srv_indices_list.free(resource.srv_index);

				for (i32 j = 0; j < resource.image.level_count; ++j) {
					resource.uav[j].destroy();
					renderer->uav_indices_list.free(resource.uav_index + j);
				}

				resource.image.destroy();
			}
			else if (resource.type == ResourceType::Buffer) {
				resource.buffer.destroy();
			}
		}

		free_resources.clear();
	}

	bool RendererFrame::begin(NotNull<Renderer*> renderer) noexcept {
		if (is_recording) {
			return false;
		}

		fence.wait(1000000000ull);
		fence.reset();
		cmd.reset();

		is_recording = cmd.begin();

		release_resources(renderer);

		staging_offset = 0;

		for (Buffer& buffer : temp_staging_memory) {
			buffer.destroy();
		}
		temp_staging_memory.clear();

		return is_recording;
	}

	BufferView RendererFrame::try_allocate_staging_memory(NotNull<const Allocator*> alloc, VkDeviceSize required_memory, VkDeviceSize required_alignment) noexcept {
		if (!is_recording) {
			return {};
		}

		VkDeviceSize aligned_requested_size = align_up(required_memory, required_alignment);
		VkDeviceSize available_size = staging_memory.memory.size - staging_offset;

		if (staging_memory.memory.size < aligned_requested_size || available_size < aligned_requested_size) {
			BufferCreateInfo create_info = {
				.size = required_memory,
				.alignment = required_alignment,
				.flags = BUFFER_FLAG_STAGING
			};

			Buffer new_buffer;
			if (!new_buffer.create(create_info) || !temp_staging_memory.push_back(alloc, new_buffer)) {
				return {};
			}

			return BufferView{ .buffer = new_buffer, .local_offset = 0, .size = aligned_requested_size };
		}

		return BufferView{ 
			.buffer = staging_memory, 
			.local_offset = std::exchange(staging_offset, staging_offset + aligned_requested_size),
			.size = aligned_requested_size 
		};
	}

	bool BufferUpdateInfo::write(NotNull<const Allocator*> alloc, Span<const u8> data, VkDeviceSize dst_offset) noexcept {
		VkDeviceSize available_size = buffer_view.size - offset;
		if (data.size() > available_size) {
			return false;
		}

		buffer_view.write(data, offset);
		copy_regions.push_back(alloc, {
			.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2_KHR,
			.srcOffset = buffer_view.local_offset + std::exchange(offset, offset + data.size()),
			.dstOffset = dst_offset,
			.size = data.size()
			});

		return true;
	}

	bool ImageUpdateInfo::write(NotNull<const Allocator*> alloc, const ImageSubresourceData& subresource_info) noexcept {
		VkDeviceSize available_size = buffer_view.size - offset;
		if (subresource_info.data.size() > available_size) {
			return false;
		}

		buffer_view.write(subresource_info.data, offset);
		copy_regions.push_back(alloc, {
			.sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2_KHR,
			.bufferOffset = buffer_view.local_offset + std::exchange(offset, offset + subresource_info.data.size()),
			.imageSubresource = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.mipLevel = subresource_info.mip_level,
				.baseArrayLayer = subresource_info.array_layer,
				.layerCount = 1u
				},
			.imageOffset = subresource_info.offset,
			.imageExtent = subresource_info.extent
			});

		return true;
	}

	Renderer* renderer_create(RendererCreateInfo create_info) {
		if (!create_info.alloc || !create_info.main_queue) {
			return nullptr;
		}

		Renderer* renderer = create_info.alloc->allocate<Renderer>(create_info.alloc);
		if (!renderer) {
			return nullptr;
		}

		renderer->alloc = create_info.alloc;
		renderer->direct_queue = create_info.main_queue;

		if (!renderer->cmd_pool.create(renderer->direct_queue)) {
			renderer_destroy(renderer);
			return nullptr;
		}

		if (!renderer->frame_timestamp.create(VK_QUERY_TYPE_TIMESTAMP, 1)) {
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

		descriptor_layout_builder.add_binding(samplers_binding, descriptor_binding_flags);
		descriptor_layout_builder.add_binding(srv_image_binding, descriptor_binding_flags);
		descriptor_layout_builder.add_binding(uav_image_binding, descriptor_binding_flags);

		if (!renderer->descriptor_layout.create(descriptor_layout_builder)) {
			renderer_destroy(renderer);
			return nullptr;
		}

		if (!renderer->descriptor_pool.create(renderer->descriptor_layout.descriptor_sizes)) {
			renderer_destroy(renderer);
			return nullptr;
		}

		if (!renderer->descriptor_set.create(renderer->descriptor_pool, &renderer->descriptor_layout)) {
			renderer_destroy(renderer);
			return nullptr;
		}

		PipelineLayoutBuilder pipeline_layout_builder = {};
		pipeline_layout_builder.add_layout(renderer->descriptor_layout);
		pipeline_layout_builder.add_range(VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT, 0u, props->limits.maxPushConstantsSize);

		if (!renderer->pipeline_layout.create(pipeline_layout_builder)) {
			renderer_destroy(renderer);
			return nullptr;
		}

		SwapchainCreateInfo swapchain_create_info = {};
		if (!renderer->swapchain.create(swapchain_create_info)) {
			renderer_destroy(renderer);
			return nullptr;
		}

		if (!renderer->swapchain.get_images(renderer->swapchain_images)) {
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

			if (!renderer->swapchain_image_views[i].create(renderer->swapchain_images[i], VK_IMAGE_VIEW_TYPE_2D, subresource_range)) {
				renderer_destroy(renderer);
				return nullptr;
			}
		}

		for (i32 i = 0; i < RENDERER_FRAME_OVERLAP; ++i) {
			if (!renderer->frames[i].create(renderer)) {
				renderer_destroy(renderer);
				return nullptr;
			}
		}

		if (!renderer->resource_handle_pool.create(create_info.alloc, RENDERER_HANDLE_MAX * 2)) {
			renderer_destroy(renderer);
			return nullptr;
		}

		Resource backbuffer_resource = {
			.type = ResourceType::Image
		};
		renderer->backbuffer_handle = renderer->resource_handle_pool.allocate_with_data(backbuffer_resource);

		if (!renderer->sampler_indices_list.create(create_info.alloc, RENDERER_HANDLE_MAX)) {
			renderer_destroy(renderer);
			return nullptr;
		}

		if (!renderer->srv_indices_list.create(create_info.alloc, RENDERER_HANDLE_MAX)) {
			renderer_destroy(renderer);
			return nullptr;
		}

		if (!renderer->uav_indices_list.create(create_info.alloc, RENDERER_HANDLE_MAX)) {
			renderer_destroy(renderer);
			return nullptr;
		}

		if (!renderer->write_descriptor_sets.reserve(create_info.alloc, 256)) {
			renderer_destroy(renderer);
			return nullptr;
		}

		if (!renderer->image_descriptors.reserve(create_info.alloc, 256)) {
			renderer_destroy(renderer);
			return nullptr;
		}

		if (!renderer->buffer_descriptors.reserve(create_info.alloc, 256)) {
			renderer_destroy(renderer);
			return nullptr;
		}

		VkSamplerCreateInfo sampler_create_info = {
			.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			.magFilter = VK_FILTER_LINEAR,
			.minFilter = VK_FILTER_LINEAR,
			.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
			.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			.mipLodBias = 1.0f,
			.anisotropyEnable = VK_TRUE,
			.maxAnisotropy = 4.0f
		};

		if (!renderer->default_sampler.create(sampler_create_info)) {
			renderer_destroy(renderer);
			return nullptr;
		}

		VkDescriptorImageInfo sampler_descriptor = {
			.sampler = renderer->default_sampler.handle
		};
		renderer->image_descriptors.push_back(renderer->alloc, sampler_descriptor);

		VkWriteDescriptorSet sampler_write = {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = renderer->descriptor_set.handle,
			.dstBinding = RENDERER_SAMPLER_SLOT,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
			.pImageInfo = renderer->image_descriptors.back()
		};
		renderer->write_descriptor_sets.push_back(renderer->alloc, sampler_write);

		return renderer;
	}

	void renderer_destroy(Renderer* renderer) {
		renderer->direct_queue.wait_idle();

		renderer->default_sampler.destroy();

		renderer->write_descriptor_sets.destroy(renderer->alloc);
		renderer->image_descriptors.destroy(renderer->alloc);
		renderer->buffer_descriptors.destroy(renderer->alloc);

		Resource* backbuffer_resource = renderer->get_resource(renderer->backbuffer_handle);
		if (backbuffer_resource) {
			backbuffer_resource->image = {};
			backbuffer_resource->srv = {};
		}

		for (auto entry : renderer->resource_handle_pool) {
			Resource* resource = entry.element;
			if (resource->type == ResourceType::Unknown) {
				continue;
			}

			// TODO: Ignore backbuffer
			if (resource->type == ResourceType::Image) {
				resource->srv.destroy();
				renderer->srv_indices_list.free(resource->srv_index);

				for (i32 mip = 0; mip < resource->image.level_count; ++mip) {
					resource->uav[mip].destroy();
					renderer->uav_indices_list.free(resource->uav_index + mip);
				}

				resource->image.destroy();
			}
			else if (resource->type == ResourceType::Buffer) {
				resource->buffer.destroy();
			}
		}

		renderer->uav_indices_list.destroy(renderer->alloc);
		renderer->srv_indices_list.destroy(renderer->alloc);
		renderer->sampler_indices_list.destroy(renderer->alloc);

		renderer->resource_handle_pool.destroy(renderer->alloc);

		for (i32 i = 0; i < RENDERER_FRAME_OVERLAP; ++i) {
			renderer->frames[i].destroy(renderer);
		}

		for (i32 i = 0; i < renderer->swapchain.image_count; ++i) {
			renderer->swapchain_image_views[i].destroy();
		}

		renderer->swapchain.destroy();
		renderer->pipeline_layout.destroy();
		renderer->descriptor_set.destroy();
		renderer->descriptor_pool.destroy();
		renderer->descriptor_layout.destroy();
		renderer->frame_timestamp.destroy();
		renderer->cmd_pool.destroy();

		const Allocator* alloc = renderer->alloc;
		alloc->deallocate(renderer);
	}

	Handle Renderer::add_resource() noexcept {
		if (resource_handle_pool.is_full()) {
			return HANDLE_INVALID;
		}
		return resource_handle_pool.allocate();
	}

	bool Renderer::setup_resource(Handle handle, Image image) noexcept {
		Resource* resource = resource_handle_pool.get(handle);
		resource->type = ResourceType::Image;
		resource->image = image;

		Image& image_source = resource->image;

		VkImageAspectFlags image_aspect = (image_source.usage_flags & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

		VkImageViewType view_type = {};
		if (image_source.extent.depth > 1) {
			view_type = VK_IMAGE_VIEW_TYPE_3D;
		}
		else if (image_source.extent.height > 1) {
			if (image_source.layer_count > 1) {
				view_type = (image_source.face_count > 1) ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
			}
			else {
				view_type = (image_source.face_count > 1) ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;
			}
		}
		else if (image_source.extent.width > 1) {
			view_type = image_source.layer_count > 1 ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D;
		}

		if (image_source.usage_flags & VK_IMAGE_USAGE_SAMPLED_BIT) {
			const VkImageSubresourceRange srv_subresource_range = {
				.aspectMask = image_aspect,
				.baseMipLevel = 0u,
				.levelCount = image_source.level_count,
				.baseArrayLayer = 0u,
				.layerCount = image_source.layer_count * image_source.face_count
			};

			if (!resource->srv.create(image_source, view_type, srv_subresource_range)) {
				return false;
			}

			if (!srv_indices_list.allocate(&resource->srv_index)) {
				return false;
			}
		}

		if (image_source.usage_flags & VK_IMAGE_USAGE_STORAGE_BIT) {
			VkImageSubresourceRange uav_subresource_range = {
				.aspectMask = image_aspect,
				.baseMipLevel = 0u,
				.levelCount = 1u,
				.baseArrayLayer = 0u,
				.layerCount = image_source.layer_count * image_source.face_count
			};

			for (i32 i = 0; i < image_source.level_count; ++i) {
				uav_subresource_range.baseMipLevel = i;

				if (!resource->uav[i].create(image_source, view_type, uav_subresource_range)) {
					return false;
				}

				u32 uav_index;
				if (!uav_indices_list.allocate(&uav_index)) {
					return false;
				}

				if (i == 0) {
					resource->uav_index = uav_index;
				}
			}
		}

		bool is_attachment = (image_source.usage_flags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) || (image_source.usage_flags & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
		if (!is_attachment && image_source.layout != VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR) {
			PipelineBarrierBuilder builder = {};

			VkImageSubresourceRange subresource_range = {
				.aspectMask = image_aspect,
				.baseMipLevel = 0u,
				.levelCount = image_source.level_count,
				.baseArrayLayer = 0u,
				.layerCount = image_source.layer_count
			};

			// TODO: Make batching updates
			barrier_builder.add_image(image_source, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR, subresource_range);
			active_frame->cmd.pipeline_barrier(builder);

			image_source.layout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR;
		}

		VkWriteDescriptorSet descriptor_write = {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = descriptor_set.handle,
			.descriptorCount = 1u
		};

		if (image_source.usage_flags & VK_IMAGE_USAGE_SAMPLED_BIT) {
			const VkDescriptorImageInfo image_descriptor = {
				.imageView = resource->srv.handle,
				.imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR
			};
			image_descriptors.push_back(alloc, image_descriptor);

			descriptor_write.dstBinding = RENDERER_SRV_SLOT;
			descriptor_write.dstArrayElement = resource->srv_index;
			descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			descriptor_write.pImageInfo = image_descriptors.back();
			write_descriptor_sets.push_back(alloc, descriptor_write);
		}

		if (image_source.usage_flags & VK_IMAGE_USAGE_STORAGE_BIT) {
			descriptor_write.dstBinding = RENDERER_UAV_SLOT;
			descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

			for (i32 mip = 0; mip < image_source.level_count; ++mip) {
				const VkDescriptorImageInfo image_descriptor = {
					.imageView = resource->uav[mip].handle,
					.imageLayout = VK_IMAGE_LAYOUT_GENERAL
				};
				image_descriptors.push_back(alloc, image_descriptor);

				descriptor_write.dstArrayElement = resource->uav_index + mip;
				descriptor_write.pImageInfo = image_descriptors.back();
				write_descriptor_sets.push_back(alloc, descriptor_write);
			}
		}

		return true;
	}

	bool Renderer::setup_resource(Handle handle, Buffer buffer) noexcept {
		Resource* resource = resource_handle_pool.get(handle);
		resource->type = ResourceType::Buffer;
		resource->buffer = buffer;
		return true;
	}

	void Renderer::update_resource(Handle handle, Image image) noexcept {
		if (!resource_handle_pool.is_valid(handle)) {
			return;
		}

		if (active_frame) {
			Resource* resource = resource_handle_pool.get(handle);
			if (resource->type != ResourceType::Unknown) {
				active_frame->free_resources.push_back(alloc, *resource);
			}
		}

		setup_resource(handle, image);
	}

	void Renderer::update_resource(Handle handle, Buffer buffer) noexcept {
		if (!resource_handle_pool.is_valid(handle)) {
			return;
		}

		if (active_frame) {
			Resource* resource = resource_handle_pool.get(handle);
			if (resource->type != ResourceType::Unknown) {
				active_frame->free_resources.push_back(alloc, *resource);
			}
		}

		setup_resource(handle, buffer);
	}

	Resource* Renderer::get_resource(Handle handle) noexcept {
		return resource_handle_pool.get(handle);
	}

	void Renderer::free_resource(Handle handle) noexcept {
		if (resource_handle_pool.is_valid(handle)) {
			if (active_frame) {
				Resource* resource = resource_handle_pool.get(handle);
				if (resource->type != ResourceType::Unknown) {
					active_frame->free_resources.push_back(alloc, *resource);
				}
			}

			resource_handle_pool.free(alloc, handle);
		}
	}

	bool Renderer::frame_begin() noexcept {
		bool surface_updated = false;
		if (swapchain.is_outdated()) {

			if (direct_queue) {
				direct_queue.wait_idle();
			}

			if (!swapchain.update()) {
				return false;
			}

			if (!swapchain.get_images(swapchain_images)) {
				return false;
			}

			for (i32 i = 0; i < swapchain.image_count; ++i) {
				VkImageSubresourceRange subresource_range = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.baseMipLevel = 0u,
					.levelCount = 1u,
					.baseArrayLayer = 0u,
					.layerCount = 1u
				};

				Image& image = swapchain_images[i];
				ImageView& image_view = swapchain_image_views[i];

				image_view.destroy();
				if (!image_view.create(image, VK_IMAGE_VIEW_TYPE_2D, subresource_range)) {
					return false;
				}
			}

			active_frame = nullptr;
			active_image_index = 0;
			surface_updated = true;
		}

		RendererFrame& current_frame = frames[frame_number % RENDERER_FRAME_OVERLAP];
		if (!current_frame.begin(this)) {
			return false;
		}

		acquired_semaphore = current_frame.image_available;

		if (!swapchain.acquire_next_image(1000000000ull, acquired_semaphore, &active_image_index)) {
			return false;
		}

		active_frame = &current_frame;

		// Update backbuffer resource
		Resource* backbuffer_resource = resource_handle_pool.get(backbuffer_handle);
		backbuffer_resource->image = swapchain_images[active_image_index];
		backbuffer_resource->srv = swapchain_image_views[active_image_index];

		if (frame_number > 0) {
			u64 timestamps[2] = {};
			if (frame_timestamp.get_data(0, timestamps)) {
				u64 elapsed_time = timestamps[1] - timestamps[0];
				if (timestamps[1] <= timestamps[0]) {
					elapsed_time = 0ull;
				}

				gpu_delta_time = (double)elapsed_time * timestamp_freq / 1000000.0;
			}
		}

		current_frame.cmd.reset_query(frame_timestamp, 0u, 2u);
		current_frame.cmd.write_timestamp(frame_timestamp, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0u);

		current_frame.cmd.bind_descriptor(pipeline_layout, descriptor_set, VK_PIPELINE_BIND_POINT_GRAPHICS);
		current_frame.cmd.bind_descriptor(pipeline_layout, descriptor_set, VK_PIPELINE_BIND_POINT_COMPUTE);

		return true;
	}

	bool Renderer::frame_end() noexcept {
		if (!active_frame || !active_frame->is_recording) {
			return false;
		}

		Resource* backbuffer_resource = resource_handle_pool.get(backbuffer_handle);
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

				barrier_builder.add_image(backbuffer_resource->image, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, subresource_range);
				active_frame->cmd.pipeline_barrier(barrier_builder);

				backbuffer_resource->image.layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			}
		}

		if (!write_descriptor_sets.empty()) {
			updete_descriptors(write_descriptor_sets.data(), write_descriptor_sets.size());

			write_descriptor_sets.clear();
			image_descriptors.clear();
			buffer_descriptors.clear();
		}

		active_frame->cmd.write_timestamp(frame_timestamp, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 1u);
		active_frame->cmd.end();

		VkSemaphoreSubmitInfo wait_semaphores[2] = {};
		wait_semaphores[0].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
		wait_semaphores[0].semaphore = acquired_semaphore.handle;
		wait_semaphores[0].stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

		VkSemaphoreSubmitInfo signal_semaphores[2] = {};
		signal_semaphores[0].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
		signal_semaphores[0].semaphore = active_frame->rendering_finished.handle;
		signal_semaphores[0].stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

		i32 cmd_buffer_count = 0;
		VkCommandBufferSubmitInfo cmd_buffer_submit_infos[6];
		cmd_buffer_submit_infos[cmd_buffer_count++] = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
			.commandBuffer = active_frame->cmd.handle
		};

		const VkSubmitInfo2KHR submit_info = {
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR,
			.waitSemaphoreInfoCount = 1,
			.pWaitSemaphoreInfos = wait_semaphores,
			.commandBufferInfoCount = (u32)cmd_buffer_count,
			.pCommandBufferInfos = cmd_buffer_submit_infos,
			.signalSemaphoreInfoCount = 1,
			.pSignalSemaphoreInfos = signal_semaphores
		};

		if (!direct_queue.submit(active_frame->fence, &submit_info)) {
			active_frame->is_recording = false;
			active_frame = nullptr;
			return false;
		}

		const VkPresentInfoKHR present_info = {
			.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &signal_semaphores[0].semaphore,
			.swapchainCount = 1,
			.pSwapchains = &swapchain.handle,
			.pImageIndices = &active_image_index
		};

		if (!direct_queue.present(&present_info)) {
			active_frame->is_recording = false;
			active_frame = nullptr;
			return false;
		}

		active_frame->is_recording = false;
		active_frame = nullptr;

		frame_number++;

		return true;
	}

	void Renderer::image_update_begin(VkDeviceSize size, ImageUpdateInfo& update_info) noexcept {
		update_info.buffer_view = active_frame->try_allocate_staging_memory(alloc, size, 1);
	}

	void Renderer::image_update_end(ImageUpdateInfo& update_info) noexcept {
		const VkCopyBufferToImageInfo2KHR copy_image_info = {
			.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2_KHR,
			.srcBuffer = update_info.buffer_view.buffer.handle,
			.dstImage = update_info.dst_image.handle,
			.dstImageLayout = update_info.dst_image.layout,
			.regionCount = (u32)update_info.copy_regions.size(),
			.pRegions = update_info.copy_regions.data()
		};
		
		vkCmdCopyBufferToImage2KHR(active_frame->cmd.handle, &copy_image_info);
		update_info.copy_regions.destroy(alloc);
	}

	void Renderer::buffer_update_begin(VkDeviceSize size, BufferUpdateInfo& update_info) noexcept {
		update_info.buffer_view = active_frame->try_allocate_staging_memory(alloc, size, 1);
	}

	void Renderer::buffer_update_end(BufferUpdateInfo& update_info) {
		const VkCopyBufferInfo2KHR copy_buffer_info = {
			.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2_KHR,
			.srcBuffer = update_info.buffer_view.buffer.handle,
			.dstBuffer = update_info.dst_buffer.handle,
			.regionCount = (u32)update_info.copy_regions.size(),
			.pRegions = update_info.copy_regions.data()
		};

		vkCmdCopyBuffer2KHR(active_frame->cmd.handle, &copy_buffer_info);
		update_info.copy_regions.destroy(alloc);
	}
}