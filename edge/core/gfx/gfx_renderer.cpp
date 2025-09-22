#include "gfx_renderer.h"

namespace edge::gfx {

#define EDGE_LOGGER_SCOPE "gfx::Frame"

	Frame::~Frame() {

	}

	auto Frame::construct(const Context& ctx, CommandBuffer&& command_buffer) -> Result<Frame> {
		Frame self{};
		self.command_buffer_ = std::move(command_buffer);
		if (auto result = self._construct(ctx); result != vk::Result::eSuccess) {
			return std::unexpected(result);
		}
		return self;
	}

	auto Frame::_construct(const Context& ctx) -> vk::Result {
		if (auto result = ctx.create_semaphore(); !result.has_value()) {
			EDGE_SLOGE("Failed to create image available semaphore handle. Reason: {}.", vk::to_string(result.error()));
			return result.error();
			
		}
		else {
			image_available_ = std::move(result.value());
		}

		if (auto result = ctx.create_semaphore(); !result.has_value()) {
			EDGE_SLOGE("Failed to create rendering finished semaphore handle. Reason: {}.", vk::to_string(result.error()));
			return result.error();
		}
		else {
			rendering_finished_ = std::move(result.value());
		}

		if (auto result = ctx.create_fence(vk::FenceCreateFlagBits::eSignaled); !result.has_value()) {
			EDGE_SLOGE("Failed to create frame fence handle. Reason: {}.", vk::to_string(result.error()));
			return result.error();
		}
		else {
			fence_ = std::move(result.value());
		}

		return vk::Result::eSuccess;
	}

#undef EDGE_LOGGER_SCOPE // Frames

#define EDGE_LOGGER_SCOPE "gfx::Renderer"

	auto Renderer::construct(Context&& context, const RendererCreateInfo& create_info) -> Result<std::unique_ptr<Renderer>> {
		auto self = std::make_unique<Renderer>();
		self->context_ = std::move(context);
		self->swapchain_images_ = Vector<Image>(self->context_.get_allocator());
		self->swapchain_image_views_ = Vector<ImageView>(self->context_.get_allocator());
		self->frames_ = Vector<Frame>(self->context_.get_allocator());
		if (auto result = self->_construct(create_info); result != vk::Result::eSuccess) {
			context = std::move(self->context_);
			return std::unexpected(result);
		}
		return self;
	}

	auto Renderer::begin_frame(float delta_time) -> void {
		if (active_frame_) {
			EDGE_SLOGW("Attempting to start a new frame when the old one is not finished.");
			return;
		}

		auto swapchain_recreated = handle_surface_change();

		auto* current_frame = get_current_frame();

		auto const& frame_fence = current_frame->get_fence();
		frame_fence.wait(1000000000ull);
		frame_fence.reset();

		auto const& semaphore = current_frame->get_image_available_semaphore();
		acquired_senmaphore_ = semaphore.get_handle();

		vk::Device device = context_.get_device();
		if (swapchain_) {
			auto result = device.acquireNextImageKHR(swapchain_, ~0ull, acquired_senmaphore_, VK_NULL_HANDLE, &swapchain_image_index_);
			if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
				swapchain_recreated = handle_surface_change(true);

				// Try to acquire next image again after recreation
				if (swapchain_recreated) {
					result = device.acquireNextImageKHR(swapchain_, ~0ull, acquired_senmaphore_, VK_NULL_HANDLE, &swapchain_image_index_);
				}

				if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
					return;
				}
			}
		}

		active_frame_ = current_frame;

		auto const& cmdbuf = active_frame_->get_command_buffer();
		if (auto result = cmdbuf.begin(); result != vk::Result::eSuccess) {
			EDGE_SLOGE("Failed to begin command buffer. Reason: {}.", vk::to_string(result));
			active_frame_ = nullptr;
			return;
		}

		delta_time_ = delta_time;
		// TODO: add query pool for performance measures
	}

	auto Renderer::end_frame() -> void {
		if (!active_frame_) {
			EDGE_SLOGW("Attempting to end a frame when the new one is not started yet.");
			return;
		}

		auto const& cmdbuf = active_frame_->get_command_buffer();
		cmdbuf.push_barrier(ImageBarrier{
			.image = &swapchain_images_[swapchain_image_index_],
			.src_state = ResourceStateFlag::eUndefined,
			.dst_state = ResourceStateFlag::ePresent,
			.subresource_range = { vk::ImageAspectFlagBits::eColor, 0u, 1u, 0u, 1u }
			});

		cmdbuf.end();

		Vector<vk::SemaphoreSubmitInfo> wait_semaphores{ context_.get_allocator() };
		wait_semaphores.push_back(vk::SemaphoreSubmitInfo{ acquired_senmaphore_, 0ull, vk::PipelineStageFlagBits2::eColorAttachmentOutput });

		Vector<vk::SemaphoreSubmitInfo> signal_semaphores{ context_.get_allocator() };

		vk::Semaphore rendering_finished_sem = active_frame_->get_rendering_finished_semaphore();
		signal_semaphores.push_back(vk::SemaphoreSubmitInfo{ rendering_finished_sem, 0ull, vk::PipelineStageFlagBits2::eColorAttachmentOutput });

		vk::CommandBufferSubmitInfo cmd_buffer_submit_info{};
		cmd_buffer_submit_info.commandBuffer = cmdbuf.get_handle();

		vk::SubmitInfo2 submit_info{};
		submit_info.waitSemaphoreInfoCount = static_cast<uint32_t>(wait_semaphores.size());
		submit_info.pWaitSemaphoreInfos = wait_semaphores.data();
		submit_info.signalSemaphoreInfoCount = static_cast<uint32_t>(signal_semaphores.size());
		submit_info.pSignalSemaphoreInfos = signal_semaphores.data();
		submit_info.commandBufferInfoCount = 1u;
		submit_info.pCommandBufferInfos = &cmd_buffer_submit_info;

		vk::Queue queue = queue_;
		if (auto result = queue.submit2KHR(1u, &submit_info, active_frame_->get_fence()); result != vk::Result::eSuccess) {
			EDGE_SLOGE("Failed to submit queue. Reason: {}", vk::to_string(result));
			return;
		}
		
		if (swapchain_) {
			vk::SwapchainKHR swapchain = swapchain_;

			vk::PresentInfoKHR present_info{};
			present_info.waitSemaphoreCount = 1u;
			present_info.pWaitSemaphores = &rendering_finished_sem;
			present_info.swapchainCount = 1u;
			present_info.pSwapchains = &swapchain;
			present_info.pImageIndices = &swapchain_image_index_;

			if (auto result = queue.presentKHR(&present_info); result != vk::Result::eSuccess) {
				EDGE_SLOGE("Failed to present images. Reason: {}", vk::to_string(result));
				return;
			}
		}

		active_frame_ = nullptr;
		frame_number_++;
	}

	auto Renderer::_construct(const RendererCreateInfo& create_info) -> vk::Result {
		if (auto result = context_.get_queue(QueueType::eDirect); !result.has_value()) {
			EDGE_SLOGE("Failed to request queue. Reason: {}.", vk::to_string(result.error()));
			return result.error();
		}
		else {
			queue_ = std::move(result.value());
		}

		if (auto result = queue_.create_command_pool(); !result.has_value()) {
			EDGE_SLOGE("Failed to create command pool. Reason: {}.", vk::to_string(result.error()));
			return result.error();
		}
		else {
			command_pool_ = std::move(result.value());
		}

		if (auto result = SwapchainBuilder{ context_.get_adapter(), context_.get_device(), context_.get_surface() }
			.set_image_extent(create_info.extent)
			.set_image_format(create_info.preferred_format)
			.set_color_space(create_info.preferred_color_space)
			.enable_hdr(create_info.enable_hdr)
			.enable_vsync(create_info.enable_vsync)
			.build(); !result.has_value()) {
			EDGE_SLOGE("Failed to create swapchain. Reason: {}.", vk::to_string(result.error()));
			return result.error();
		}
		else {
			swapchain_ = std::move(result.value());
		}

		// Create frames
		if (auto result = swapchain_.get_images(); !result.has_value()) {
			EDGE_SLOGE("Failed to request swapchain images. Reason: {}.", vk::to_string(result.error()));
			return result.error();
		}
		else {
			swapchain_images_ = std::move(result.value());
		}

		for (int32_t i = 0; i < static_cast<int32_t>(swapchain_images_.size()); ++i) {
			vk::ImageSubresourceRange subresource_range{};
			subresource_range.aspectMask = vk::ImageAspectFlagBits::eColor;
			subresource_range.baseArrayLayer = 0u;
			subresource_range.layerCount = 1u;
			subresource_range.baseMipLevel = 0u;
			subresource_range.levelCount = 1u;

			if (auto result = context_.create_image_view(swapchain_images_[i], subresource_range, vk::ImageViewType::e2D); !result.has_value()) {
				return result.error();
			}
			else {
				swapchain_image_views_.push_back(std::move(result.value()));
			}

			auto command_list_result = command_pool_.allocate_command_buffer();
			if (!command_list_result) {
				EDGE_SLOGE("Failed to allocate command list for frame at index {}. Reason: {}.", i, vk::to_string(command_list_result.error()));
				return command_list_result.error();
			}

			if (auto frame_result = Frame::construct(context_, std::move(command_list_result.value())); !frame_result.has_value()) {
				EDGE_SLOGE("Failed to create frame at index {}. Reason: {}.", i, vk::to_string(frame_result.error()));
				return frame_result.error();
			}
			else {
				frames_.push_back(std::move(frame_result.value()));
			}
		}

		return vk::Result::eSuccess;
	}

	auto Renderer::handle_surface_change(bool force) -> bool {
		if (!swapchain_) {
			EDGE_SLOGW("Can't handle surface changes in headless mode, skipping.");
			return false;
		}

		vk::PhysicalDevice adapter = context_.get_adapter();
		vk::SurfaceKHR surface = context_.get_surface();

		vk::SurfaceCapabilitiesKHR surface_capabilities;
		if (auto result = adapter.getSurfaceCapabilitiesKHR(surface, &surface_capabilities); result != vk::Result::eSuccess) {
			return false;
		}

		if (surface_capabilities.currentExtent.width == 0xFFFFFFFF || surface_capabilities.currentExtent.height == 0xFFFFFFFF) {
			return false;
		}

		auto current_extent = swapchain_.get_extent();
		if (current_extent != surface_capabilities.currentExtent) {
			vk::Queue queue = queue_;
			if (auto result = queue.waitIdle(); result != vk::Result::eSuccess) {
				return false;
			}

			auto swapchain_state = swapchain_.get_state();

			if (auto swapchain_result = SwapchainBuilder{ context_.get_adapter(), context_.get_device(), context_.get_surface() }
				.set_old_swapchain(swapchain_.get_handle())
				.set_image_extent(surface_capabilities.currentExtent)
				.set_image_format(swapchain_state.format.format)
				.set_color_space(swapchain_state.format.colorSpace)
				.set_image_count(swapchain_state.image_count)
				.enable_hdr(swapchain_state.hdr)
				.enable_vsync(swapchain_state.vsync)
				.build(); !swapchain_result.has_value()) {
				EDGE_SLOGE("Failed to recreate swapchain with reason: {}", vk::to_string(swapchain_result.error()));
				return false;
			}
			else {
				swapchain_.reset();
				swapchain_ = std::move(swapchain_result.value());
			}

			active_frame_ = nullptr;
			swapchain_image_index_ = 0u;
			return true;
		}

		return false;
	}

#undef EDGE_LOGGER_SCOPE // Renderer

}