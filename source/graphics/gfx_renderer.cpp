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
	[[maybe_unused]] static bool is_depth_format(VkFormat format) {
		return format == VK_FORMAT_D16_UNORM || format == VK_FORMAT_D32_SFLOAT;
	}

	[[maybe_unused]] static bool is_depth_stencil_format(VkFormat format) {
		return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D16_UNORM_S8_UINT ||
			format == VK_FORMAT_D24_UNORM_S8_UINT;
	}

	void ImageResource::destroy() {
		for (usize i = 0; i < handle.level_count; ++i) {
			uavs[i].destroy();
		}
		srv.destroy();
		handle.destroy();
	}

	void BufferResource::destroy() {
		handle.destroy();
	}

	void SamplerResource::destroy() {
		handle.destroy();
	}

	void RenderResource::destroy() {
		std::visit([](auto&& arg) {
			using T = std::decay_t<decltype(arg)>;
			if constexpr (!std::is_same_v<T, std::monostate>) { arg.destroy(); }
			}, resource);
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

		if (!pending_destroys.reserve(alloc, 256)) {
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

		pending_destroys.destroy(alloc);

		cmd.destroy();
		fence.destroy();
		rendering_finished.destroy();
		image_available.destroy();
	}

	bool RendererFrame::begin(NotNull<Renderer*> renderer) {
		if (is_recording) {
			return false;
		}

		fence.wait(1000000000ull);
		fence.reset();
		cmd.reset();

		is_recording = cmd.begin();

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

		if (!resource_pool.create(alloc, RENDERER_HANDLE_MAX * 2)) {
			destroy(alloc);
			return false;
		}

		if (!smp_index_allocator.create(alloc, RENDERER_HANDLE_MAX)) {
			destroy(alloc);
			return false;
		}

		if (!srv_index_allocator.create(alloc, RENDERER_HANDLE_MAX)) {
			destroy(alloc);
			return false;
		}

		if (!uav_index_allocator.create(alloc, RENDERER_HANDLE_MAX)) {
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

		backbuffer_handle = create_empty();
		RenderResource* res = resource_pool.get(backbuffer_handle);
		res->resource = ImageResource{};
		if (!srv_index_allocator.allocate(&res->srv_index)) {
			destroy(alloc);
			return false;
		}

		return true;
	}

	void Renderer::destroy(NotNull<const Allocator*> alloc) {
		direct_queue.wait_idle();

		write_descriptor_sets.destroy(alloc);
		image_descriptors.destroy(alloc);
		buffer_descriptors.destroy(alloc);

		for (usize i = 0; i < FRAME_OVERLAP; ++i) {
			flush_resource_destruction(frames[i]);
			frames[i].destroy(alloc, this);
		}

		for (usize i = 0; i < swapchain.image_count; ++i) {
			swapchain_image_views[i].destroy();
		}

		for (auto entry : resource_pool) {
			if (entry.element) {
				entry.element->destroy();
			}
		}
		resource_pool.destroy(alloc);

		smp_index_allocator.destroy(alloc);
		srv_index_allocator.destroy(alloc);
		uav_index_allocator.destroy(alloc);

		swapchain.destroy();
		pipeline_layout.destroy();
		descriptor_set.destroy();
		descriptor_pool.destroy();
		descriptor_layout.destroy();
		frame_timestamp.destroy();
		cmd_pool.destroy();
	}

	Handle Renderer::create_empty() {
		if (resource_pool.is_full()) {
			return HANDLE_INVALID;
		}
		return resource_pool.allocate();
	}

	Handle Renderer::create_image(const ImageCreateInfo& create_info) {
		Image image;
		if (!image.create(create_info)) {
			return HANDLE_INVALID;
		}

		Handle h = resource_pool.allocate();

		if (!attach_image(h, image)) {
			image.destroy();
			return HANDLE_INVALID;
		}

		return h;
	}

	bool Renderer::attach_image(Handle h, Image img) {
		RenderResource* res = resource_pool.get(h);
		if (!res) {
			return false;
		}

		ImageResource img_res = {};
		img_res.handle = std::move(img);

		VkImageAspectFlags image_aspect = (img.usage_flags & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

		VkImageViewType view_type = {};
		if (img.extent.depth > 1) {
			view_type = VK_IMAGE_VIEW_TYPE_3D;
		}
		else if (img.extent.height > 1) {
			if (img.layer_count > 1) {
				view_type = (img.face_count > 1) ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
			}
			else {
				view_type = (img.face_count > 1) ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;
			}
		}
		else if (img.extent.width > 1) {
			view_type = img.layer_count > 1 ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D;
		}

		if (img.usage_flags & VK_IMAGE_USAGE_SAMPLED_BIT) {
			const VkImageSubresourceRange srv_subresource_range = {
				.aspectMask = image_aspect,
				.baseMipLevel = 0u,
				.levelCount = img.level_count,
				.baseArrayLayer = 0u,
				.layerCount = img.layer_count * img.face_count
			};

			if (!img_res.srv.create(img, view_type, srv_subresource_range)) {
				img_res.destroy();
				return false;
			}

			if (!srv_index_allocator.allocate(&res->srv_index)) {
				img_res.destroy();
				return false;
			}

			update_srv_descriptor(res->srv_index, img_res.srv);
		}

		if (img.usage_flags & VK_IMAGE_USAGE_STORAGE_BIT) {
			VkImageSubresourceRange uav_subresource_range = {
				.aspectMask = image_aspect,
				.baseMipLevel = 0u,
				.levelCount = 1u,
				.baseArrayLayer = 0u,
				.layerCount = img.layer_count * img.face_count
			};

			for (usize i = 0; i < img.level_count; ++i) {
				uav_subresource_range.baseMipLevel = i;

				if (!img_res.uavs[i].create(img_res.handle, view_type, uav_subresource_range)) {
					img_res.destroy();
					return false;
				}

				if (!uav_index_allocator.allocate(&res->uav_indices[i])) {
					img_res.destroy();
					return false;
				}

				update_uav_descriptor(res->uav_indices[i], img_res.uavs[i]);
			}
		}

		res->resource = img_res;
		return true;
	}

	bool Renderer::update_image(NotNull<const Allocator*> alloc, Handle h, Image img) {
		if (!resource_pool.is_valid(h)) {
			return false;
		}

		if (active_frame) {
			RenderResource* resource = resource_pool.get(h);
			assert(resource && "Resource should not be null.");
			active_frame->pending_destroys.push_back(alloc, *resource);
		}

		attach_image(h, img);
		return true;
	}

	Handle Renderer::create_buffer(const BufferCreateInfo& create_info) {
		Buffer buffer;
		if (!buffer.create(create_info)) {
			return HANDLE_INVALID;
		}

		Handle h = resource_pool.allocate();
		if (!attach_buffer(h, buffer)) {
			buffer.destroy();
			return HANDLE_INVALID;
		}

		return h;
	}

	bool Renderer::attach_buffer(Handle h, Buffer buffer) {
		RenderResource* res = resource_pool.get(h);
		if (!res) {
			return false;
		}

		BufferResource buf_res = {};
		buf_res.handle = std::move(buffer);
		res->resource = buf_res;

		return true;
	}

	bool Renderer::update_buffer(NotNull<const Allocator*> alloc, Handle h, Buffer buf) {
		if (!resource_pool.is_valid(h)) {
			return false;
		}

		if (active_frame) {
			RenderResource* resource = resource_pool.get(h);
			assert(resource && "Resource should not be null.");
			active_frame->pending_destroys.push_back(alloc, *resource);
		}

		attach_buffer(h, buf);
		return true;
	}

	Handle Renderer::create_sampler(const VkSamplerCreateInfo& create_info) {
		Sampler sampler;
		if (!sampler.create(create_info)) {
			return HANDLE_INVALID;
		}

		Handle h = resource_pool.allocate();
		if (!attach_sampler(h, sampler)) {
			sampler.destroy();
			return HANDLE_INVALID;
		}

		return h;
	}

	bool Renderer::attach_sampler(Handle h, Sampler sampler) {
		RenderResource* res = resource_pool.get(h);
		if (!res) {
			return false;
		}

		if (!smp_index_allocator.allocate(&res->srv_index)) {
			return false;
		}

		SamplerResource smp_res = {};
		smp_res.handle = std::move(sampler);
		res->resource = smp_res;

		update_srv_descriptor(res->srv_index, smp_res.handle);

		return true;
	}

	bool Renderer::update_sampler(NotNull<const Allocator*> alloc, Handle h, Sampler smp) {
		if (!resource_pool.is_valid(h)) {
			return false;
		}

		if (active_frame) {
			RenderResource* resource = resource_pool.get(h);
			assert(resource && "Resource should not be null.");
			active_frame->pending_destroys.push_back(alloc, *resource);
		}

		attach_sampler(h, smp);
		return true;
	}

	RenderResource* Renderer::get_resource(Handle handle) {
		return resource_pool.get(handle);
	}

	void Renderer::free_resource(NotNull<const Allocator*> alloc, Handle handle) {
		if (resource_pool.is_valid(handle)) {
			if (active_frame) {
				RenderResource* resource = resource_pool.get(handle);
				assert(resource && "Resource should not be null.");
				active_frame->pending_destroys.push_back(alloc, *resource);
			}
			resource_pool.free(handle);
		}
	}

	void Renderer::add_state_translation(Handle h, ResourceState new_state) {
		RenderResource* res = resource_pool.get(h);
		if (!res || res->state == new_state) {
			return;
		}

		state_translations[state_translation_count++] = {
			.handle = h,
			.new_state = new_state
		};
	}

	void Renderer::translate_states(CmdBuf cmd) {
		PipelineBarrierBuilder builder = {};

		for (usize i = 0; i < state_translation_count; ++i) {
			const StateTranslation& translation = state_translations[i];

			RenderResource* res = resource_pool.get(translation.handle);
			[[unlikely]] if (!res) {
				continue;
			}

			std::visit([&](auto&& data) {
				using T = std::decay_t<decltype(data)>;

				if constexpr (std::is_same_v<T, ImageResource>) {
					VkImageAspectFlags aspect_mask = (is_depth_format(data.handle.format) || is_depth_stencil_format(data.handle.format)) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

					VkImageSubresourceRange subresource_range = {
						.aspectMask = aspect_mask,
						.baseMipLevel = 0u,
						.levelCount = data.handle.level_count,
						.baseArrayLayer = 0u,
						.layerCount = data.handle.layer_count
					};

					builder.add_image(data.handle, res->state, translation.new_state, subresource_range);
				}
				else if constexpr (std::is_same_v<T, BufferResource>) {
					builder.add_buffer(data.handle, res->state, translation.new_state, 0, VK_WHOLE_SIZE);
				}
				res->state = translation.new_state;
				}, res->resource);
		}

		cmd.pipeline_barrier(builder);
		state_translation_count = 0;
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

			for (usize i = 0; i < swapchain.image_count; ++i) {
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

		// Free old resources
		flush_resource_destruction(current_frame);

		acquired_semaphore = current_frame.image_available;

		if (!swapchain.acquire_next_image(1000000000ull, acquired_semaphore, &active_image_index)) {
			return false;
		}

		active_frame = &current_frame;

		// Update backbuffer resource
		if (RenderResource* backbuffer_resource = resource_pool.get(backbuffer_handle)) {
			auto* img_res = backbuffer_resource->as<ImageResource>();
			img_res->handle = swapchain_images[active_image_index];
			img_res->srv = swapchain_image_views[active_image_index];
		}

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

	bool Renderer::frame_end(NotNull<const Allocator*> alloc, VkSemaphoreSubmitInfoKHR uploader_semaphore) {
		if (!active_frame || !active_frame->is_recording) {
			return false;
		}

		CmdBuf cmd = active_frame->cmd;

		add_state_translation(backbuffer_handle, ResourceState::Present);
		translate_states(cmd);

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
		wait_semaphores[1] = uploader_semaphore;

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
			.waitSemaphoreInfoCount = uploader_semaphore.semaphore ? 2u : 1u,
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

	void Renderer::flush_resource_destruction(RendererFrame& frame) {
		for (auto& resource : frame.pending_destroys) {
			std::visit([this, &resource](auto&& res) {
				using T = std::decay_t<decltype(res)>;
				if constexpr (std::is_same_v<T, ImageResource>) {
					if (resource.srv_index != ~0u) {
						srv_index_allocator.free(resource.srv_index);
					}

					for (usize i = 0; i < res.handle.layer_count; ++i) {
						uav_index_allocator.free(resource.uav_indices[i]);
					}

					res.destroy();
				}
				else if constexpr (std::is_same_v<T, BufferResource>) {
					res.destroy();
				}
				else if constexpr (std::is_same_v<T, SamplerResource>) {
					if (resource.srv_index != ~0u) {
						smp_index_allocator.free(resource.srv_index);
					}

					res.destroy();
				}
				}, resource.resource);
		}
		frame.pending_destroys.clear();
	}

	void Renderer::update_srv_descriptor(u32 index, Sampler sampler) {
		image_descriptors.push_back({ .sampler = sampler });

		VkWriteDescriptorSet sampler_write = {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = descriptor_set,
			.dstBinding = RENDERER_SAMPLER_SLOT,
			.dstArrayElement = index,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
			.pImageInfo = image_descriptors.back()
		};
		write_descriptor_sets.push_back(sampler_write);
	}

	void Renderer::update_srv_descriptor(u32 index, ImageView view) {
		const VkDescriptorImageInfo image_descriptor = {
				.imageView = view,
				.imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR
		};
		image_descriptors.push_back(image_descriptor);

		VkWriteDescriptorSet descriptor_write = {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = descriptor_set,
			.dstBinding = RENDERER_SRV_SLOT,
			.dstArrayElement = index,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
			.pImageInfo = image_descriptors.back()
		};
		write_descriptor_sets.push_back(descriptor_write);
	}

	void Renderer::update_uav_descriptor(u32 index, ImageView view) {
		const VkDescriptorImageInfo image_descriptor = {
				.imageView = view,
				.imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR
		};
		image_descriptors.push_back(image_descriptor);

		VkWriteDescriptorSet descriptor_write = {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = descriptor_set,
			.dstBinding = RENDERER_UAV_SLOT,
			.dstArrayElement = index,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			.pImageInfo = image_descriptors.back()
		};
		write_descriptor_sets.push_back(descriptor_write);
	}
}