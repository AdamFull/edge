#include "gfx_renderer.h"
#include "gfx_context.h"

#include "../resources/texture_source.h"

#include <array.hpp>
#include <math.hpp>
#include <logger.hpp>

#include <atomic>
#include <utility>

#include <vulkan/vulkan.h>
#include <volk.h>

namespace edge::gfx {
	[[maybe_unused]] static bool is_depth_format(VkFormat format) {
		return format == VK_FORMAT_D16_UNORM || format == VK_FORMAT_D32_SFLOAT;
	}

	[[maybe_unused]] static bool is_depth_stencil_format(VkFormat format) {
		return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D16_UNORM_S8_UINT ||
			format == VK_FORMAT_D24_UNORM_S8_UINT;
	}

	bool RendererFrame::create(NotNull<const Allocator*> alloc, CmdPool cmd_pool) {
		BufferCreateInfo buffer_create_info = {
				.size = RENDERER_UPDATE_STAGING_ARENA_SIZE,
				.alignment = 1,
				.flags = BUFFER_FLAG_STAGING
		};

		if (!staging_memory.create(buffer_create_info)) {
			return false;
		}

		temp_staging_memory.reserve(alloc, 128);

		if (!image_available.create(VK_SEMAPHORE_TYPE_BINARY, 0)) {
			return false;
		}

		if (!rendering_finished.create(VK_SEMAPHORE_TYPE_BINARY, 0)) {
			return false;
		}

		if (!fence.create(VK_FENCE_CREATE_SIGNALED_BIT)) {
			return false;
		}

		if (!cmd.create(cmd_pool)) {
			return false;
		}

		if (!free_resources.reserve(alloc, 256)) {
			return false;
		}

		return true;
	}

	void RendererFrame::destroy(NotNull<const Allocator*> alloc, NotNull<Renderer*> renderer) {
		staging_memory.destroy();

		for (auto& buffer : temp_staging_memory) {
			buffer.destroy();
		}
		temp_staging_memory.destroy(alloc);

		release_resources(renderer);
		free_resources.destroy(alloc);

		cmd.destroy();
		fence.destroy();
		rendering_finished.destroy();
		image_available.destroy();
	}

	void RendererFrame::release_resources(NotNull<Renderer*> renderer) {
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

	bool RendererFrame::begin(NotNull<Renderer*> renderer) {
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

	BufferView RendererFrame::try_allocate_staging_memory(NotNull<const Allocator*> alloc, VkDeviceSize required_memory, VkDeviceSize required_alignment) {
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

	bool BufferUpdateInfo::write(NotNull<const Allocator*> alloc, Span<const u8> data, VkDeviceSize dst_offset) {
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

	bool ImageUpdateInfo::write(NotNull<const Allocator*> alloc, const ImageSubresourceData& subresource_info) {
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
				.layerCount = subresource_info.layer_count
				},
			.imageOffset = subresource_info.offset,
			.imageExtent = subresource_info.extent
			});

		return true;
	}

	bool Renderer::create(NotNull<const Allocator*> alloc, RendererCreateInfo create_info) {
		if (!create_info.main_queue) {
			return false;
		}

		direct_queue = create_info.main_queue;

		if (!cmd_pool.create(direct_queue)) {
			destroy(alloc);
			return false;
		}

		if (!frame_timestamp.create(VK_QUERY_TYPE_TIMESTAMP, 1)) {
			destroy(alloc);
			return false;
		}

		const VkPhysicalDeviceProperties* props = get_adapter_props();
		timestamp_freq = props->limits.timestampPeriod;

		DescriptorLayoutBuilder descriptor_layout_builder = {};

		VkDescriptorSetLayoutBinding samplers_binding = {
			.binding = RENDERER_SAMPLER_SLOT,
			.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
			.descriptorCount = RENDERER_HANDLE_MAX,
			.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT
		};

		VkDescriptorSetLayoutBinding srv_image_binding = {
			.binding = RENDERER_SRV_SLOT,
			.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
			.descriptorCount = RENDERER_HANDLE_MAX,
			.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT
		};

		VkDescriptorSetLayoutBinding uav_image_binding = {
			.binding = RENDERER_UAV_SLOT,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			.descriptorCount = RENDERER_HANDLE_MAX,
			.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT
		};

		VkDescriptorBindingFlags descriptor_binding_flags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

		descriptor_layout_builder.add_binding(samplers_binding, descriptor_binding_flags);
		descriptor_layout_builder.add_binding(srv_image_binding, descriptor_binding_flags);
		descriptor_layout_builder.add_binding(uav_image_binding, descriptor_binding_flags);

		if (!descriptor_layout.create(descriptor_layout_builder)) {
			destroy(alloc);
			return false;
		}

		if (!descriptor_pool.create(descriptor_layout.descriptor_sizes)) {
			destroy(alloc);
			return false;
		}

		if (!descriptor_set.create(descriptor_pool, &descriptor_layout)) {
			destroy(alloc);
			return false;
		}

		PipelineLayoutBuilder pipeline_layout_builder = {};
		pipeline_layout_builder.add_layout(descriptor_layout);
		pipeline_layout_builder.add_range(VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT, 0u, props->limits.maxPushConstantsSize);

		if (!pipeline_layout.create(pipeline_layout_builder)) {
			destroy(alloc);
			return false;
		}

		SwapchainCreateInfo swapchain_create_info = {};
		if (!swapchain.create(swapchain_create_info)) {
			destroy(alloc);
			return false;
		}

		if (!swapchain.get_images(swapchain_images)) {
			destroy(alloc);
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

			if (!swapchain_image_views[i].create(swapchain_images[i], VK_IMAGE_VIEW_TYPE_2D, subresource_range)) {
				destroy(alloc);
				return false;
			}
		}

		for (i32 i = 0; i < FRAME_OVERLAP; ++i) {
			if (!frames[i].create(alloc, cmd_pool)) {
				destroy(alloc);
				return false;
			}
		}

		if (!resource_handle_pool.create(alloc, RENDERER_HANDLE_MAX * 2)) {
			destroy(alloc);
			return false;
		}

		Resource backbuffer_resource = {
			.type = ResourceType::Image
		};
		backbuffer_handle = resource_handle_pool.allocate_with_data(backbuffer_resource);

		if (!sampler_indices_list.create(alloc, RENDERER_HANDLE_MAX)) {
			destroy(alloc);
			return false;
		}

		if (!srv_indices_list.create(alloc, RENDERER_HANDLE_MAX)) {
			destroy(alloc);
			return false;
		}

		if (!uav_indices_list.create(alloc, RENDERER_HANDLE_MAX)) {
			destroy(alloc);
			return false;
		}

		if (!write_descriptor_sets.reserve(alloc, 256)) {
			destroy(alloc);
			return false;
		}

		if (!image_descriptors.reserve(alloc, 256)) {
			destroy(alloc);
			return false;
		}

		if (!buffer_descriptors.reserve(alloc, 256)) {
			destroy(alloc);
			return false;
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

		if (!default_sampler.create(sampler_create_info)) {
			destroy(alloc);
			return false;
		}

		VkDescriptorImageInfo sampler_descriptor = {
			.sampler = default_sampler
		};
		image_descriptors.push_back(alloc, sampler_descriptor);

		VkWriteDescriptorSet sampler_write = {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = descriptor_set,
			.dstBinding = RENDERER_SAMPLER_SLOT,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
			.pImageInfo = image_descriptors.back()
		};
		write_descriptor_sets.push_back(alloc, sampler_write);

		return true;
	}

	void Renderer::destroy(NotNull<const Allocator*> alloc) {
		direct_queue.wait_idle();
		default_sampler.destroy();

		write_descriptor_sets.destroy(alloc);
		image_descriptors.destroy(alloc);
		buffer_descriptors.destroy(alloc);

		Resource* backbuffer_resource = get_resource(backbuffer_handle);
		if (backbuffer_resource) {
			backbuffer_resource->image = {};
			backbuffer_resource->srv = {};
		}

		for (auto entry : resource_handle_pool) {
			Resource* resource = entry.element;
			if (resource->type == ResourceType::Image) {
				resource->srv.destroy();
				srv_indices_list.free(resource->srv_index);

				for (i32 mip = 0; mip < resource->image.level_count; ++mip) {
					resource->uav[mip].destroy();
					uav_indices_list.free(resource->uav_index + mip);
				}

				resource->image.destroy();
			}
			else if (resource->type == ResourceType::Buffer) {
				resource->buffer.destroy();
			}
		}

		uav_indices_list.destroy(alloc);
		srv_indices_list.destroy(alloc);
		sampler_indices_list.destroy(alloc);

		resource_handle_pool.destroy(alloc);

		for (i32 i = 0; i < FRAME_OVERLAP; ++i) {
			frames[i].destroy(alloc, this);
		}

		for (i32 i = 0; i < swapchain.image_count; ++i) {
			swapchain_image_views[i].destroy();
		}

		swapchain.destroy();
		pipeline_layout.destroy();
		descriptor_set.destroy();
		descriptor_pool.destroy();
		descriptor_layout.destroy();
		frame_timestamp.destroy();
		cmd_pool.destroy();
	}

	Handle Renderer::add_resource() {
		if (resource_handle_pool.is_full()) {
			return HANDLE_INVALID;
		}
		return resource_handle_pool.allocate();
	}

	bool Renderer::setup_resource(NotNull<const Allocator*> alloc, Handle handle, Image image) {
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
			VkImageSubresourceRange subresource_range = {
				.aspectMask = image_aspect,
				.baseMipLevel = 0u,
				.levelCount = image_source.level_count,
				.baseArrayLayer = 0u,
				.layerCount = image_source.layer_count
			};

			// TODO: Make batching updates
			barrier_builder.add_image(image_source, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR, subresource_range);
			active_frame->cmd.pipeline_barrier(barrier_builder);
			barrier_builder.reset();

			image_source.layout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR;
		}

		VkWriteDescriptorSet descriptor_write = {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = descriptor_set,
			.descriptorCount = 1u
		};

		if (image_source.usage_flags & VK_IMAGE_USAGE_SAMPLED_BIT) {
			const VkDescriptorImageInfo image_descriptor = {
				.imageView = resource->srv,
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
					.imageView = resource->uav[mip],
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

	bool Renderer::setup_resource(Handle handle, Buffer buffer) {
		Resource* resource = resource_handle_pool.get(handle);
		resource->type = ResourceType::Buffer;
		resource->buffer = buffer;
		return true;
	}

	void Renderer::update_resource(NotNull<const Allocator*> alloc, Handle handle, Image image) {
		if (!resource_handle_pool.is_valid(handle)) {
			return;
		}

		if (active_frame) {
			Resource* resource = resource_handle_pool.get(handle);
			if (resource->type != ResourceType::Unknown) {
				active_frame->free_resources.push_back(alloc, *resource);
			}
		}

		setup_resource(alloc, handle, image);
	}

	void Renderer::update_resource(NotNull<const Allocator*> alloc, Handle handle, Buffer buffer) {
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

	Resource* Renderer::get_resource(Handle handle) {
		return resource_handle_pool.get(handle);
	}

	void Renderer::free_resource(NotNull<const Allocator*> alloc, Handle handle) {
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

	Handle Renderer::add_image_from_disk(NotNull<const Allocator*> alloc, const char* path) {
		FILE* stream = fopen(path, "rb");
		if (!stream) {
			return HANDLE_INVALID;
		}

		TextureSource tex_source = {};
		auto result = tex_source.from_stream(alloc, stream);
		if (!TextureSource::is_ok(result)) {
			tex_source.destroy(alloc);
			return HANDLE_INVALID;
		}

		Handle new_handle = add_resource();

		texture_uploads.push_back(alloc, {
			.handle = new_handle,
			.texture_source = tex_source
			});

		return new_handle;
	}

	bool Renderer::frame_begin() {
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
		}

		RendererFrame& current_frame = frames[frame_number % FRAME_OVERLAP];
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

				gpu_delta_time = (f64)elapsed_time * timestamp_freq / 1000000.0;
			}
		}

		current_frame.cmd.reset_query(frame_timestamp, 0u, 2u);
		current_frame.cmd.write_timestamp(frame_timestamp, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0u);

		current_frame.cmd.bind_descriptor(pipeline_layout, descriptor_set, VK_PIPELINE_BIND_POINT_GRAPHICS);
		current_frame.cmd.bind_descriptor(pipeline_layout, descriptor_set, VK_PIPELINE_BIND_POINT_COMPUTE);

		return true;
	}

	bool Renderer::frame_end(NotNull<const Allocator*> alloc) {
		if (!active_frame || !active_frame->is_recording) {
			return false;
		}

		CmdBuf cmd = active_frame->cmd;

		for (auto& texture_upload : texture_uploads) {
			TextureSource& tex_src = texture_upload.texture_source;

			const ImageCreateInfo image_create_info = {
				.extent = { tex_src.base_width, tex_src.base_height, tex_src.base_depth },
				.level_count = tex_src.mip_levels,
				.layer_count = tex_src.array_layers,
				.face_count = tex_src.face_count,
				.usage_flags = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
				.format = static_cast<VkFormat>(tex_src.format_info->vk_format)
			};

			Image image = {};
			if (!image.create(image_create_info)) {
				EDGE_LOG_ERROR("Failed to create image.");
				tex_src.destroy(alloc);
				continue;
			}

			barrier_builder.add_image(image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = tex_src.mip_levels,
				.baseArrayLayer = 0,
				.layerCount = tex_src.array_layers * tex_src.face_count
				});
			cmd.pipeline_barrier(barrier_builder);
			barrier_builder.reset();

			image.layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

			ImageUpdateInfo update_info = {
				.dst_image = image,
				.buffer_view = active_frame->try_allocate_staging_memory(alloc, tex_src.data_size, 1)
			};

			for (usize level = 0; level < tex_src.mip_levels; ++level) {
				u32 mip_width = max(tex_src.base_width >> level, 1u);
				u32 mip_height = max(tex_src.base_height >> level, 1u);
				u32 mip_depth = max(tex_src.base_depth >> level, 1u);

				auto subresource_info = tex_src.get_mip(level);

				update_info.write(alloc, {
						.data = { subresource_info.data, subresource_info.size },
						.extent = { mip_width, mip_height, mip_depth },
						.mip_level = (u32)level,
						.array_layer = 0,
						.layer_count = tex_src.array_layers * tex_src.face_count
					});
			}

			image_update_end(alloc, update_info);
			setup_resource(alloc, texture_upload.handle, image);
			tex_src.destroy(alloc);
		}
		texture_uploads.clear();

		Resource* backbuffer_resource = resource_handle_pool.get(backbuffer_handle);
		if (backbuffer_resource) {
			if (backbuffer_resource->image.layout != VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
				VkImageSubresourceRange subresource_range = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0u,
				.levelCount = 1u,
				.baseArrayLayer = 0u,
				.layerCount = 1u
				};

				barrier_builder.add_image(backbuffer_resource->image, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, subresource_range);
				cmd.pipeline_barrier(barrier_builder);
				barrier_builder.reset();

				backbuffer_resource->image.layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			}
		}

		if (!write_descriptor_sets.empty()) {
			updete_descriptors(write_descriptor_sets.data(), write_descriptor_sets.size());

			write_descriptor_sets.clear();
			image_descriptors.clear();
			buffer_descriptors.clear();
		}

		cmd.write_timestamp(frame_timestamp, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 1u);
		cmd.end();

		VkSemaphoreSubmitInfo wait_semaphores[2] = {};
		wait_semaphores[0].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
		wait_semaphores[0].semaphore = acquired_semaphore;
		wait_semaphores[0].stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

		VkSemaphoreSubmitInfo signal_semaphores[2] = {};
		signal_semaphores[0].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
		signal_semaphores[0].semaphore = active_frame->rendering_finished;
		signal_semaphores[0].stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

		i32 cmd_buffer_count = 0;
		VkCommandBufferSubmitInfo cmd_buffer_submit_infos[6];
		cmd_buffer_submit_infos[cmd_buffer_count++] = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
			.commandBuffer = cmd
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

	void Renderer::image_update_end(NotNull<const Allocator*> alloc, ImageUpdateInfo& update_info) {
		const VkCopyBufferToImageInfo2KHR copy_image_info = {
			.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2_KHR,
			.srcBuffer = update_info.buffer_view.buffer,
			.dstImage = update_info.dst_image,
			.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.regionCount = (u32)update_info.copy_regions.size(),
			.pRegions = update_info.copy_regions.data()
		};

		vkCmdCopyBufferToImage2KHR(active_frame->cmd, &copy_image_info);
		update_info.copy_regions.destroy(alloc);
	}

	void Renderer::buffer_update_end(NotNull<const Allocator*> alloc, BufferUpdateInfo& update_info) {
		const VkCopyBufferInfo2KHR copy_buffer_info = {
			.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2_KHR,
			.srcBuffer = update_info.buffer_view.buffer,
			.dstBuffer = update_info.dst_buffer,
			.regionCount = (u32)update_info.copy_regions.size(),
			.pRegions = update_info.copy_regions.data()
		};

		vkCmdCopyBuffer2KHR(active_frame->cmd, &copy_buffer_info);
		update_info.copy_regions.destroy(alloc);
	}
}