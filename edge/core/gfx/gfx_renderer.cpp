#include "gfx_renderer.h"

#include <numeric>

#include <zstd.h>

#include "gfx_shader_effect.h"
#include "../../assets/shaders/interop.h"

namespace edge::gfx {
	void TechniqueStage::deserialize(BinaryReader& reader) {
		stage = reader.read<vk::ShaderStageFlagBits>();
		entry_point_name = reader.read_string();

		auto compressed_code = reader.read_vector<uint8_t>();

		auto const required_size = ZSTD_getFrameContentSize(compressed_code.data(), compressed_code.size());
		code.resize(required_size);

		size_t const decompressed_size = ZSTD_decompress(code.data(), code.size(), compressed_code.data(), compressed_code.size());
		code.resize(decompressed_size);
	}

#define EDGE_LOGGER_SCOPE "gfx::RenderResource"

	mi::FreeList<uint32_t> RenderResource::srv_free_list_{};
	mi::FreeList<uint32_t> RenderResource::uav_free_list_{};

	RenderResource::RenderResource(Renderer* renderer)
		: renderer_{ renderer } {

	}

	RenderResource::~RenderResource() {
		if (has_handle()) {
			reset();
		}
	}

	RenderResource::RenderResource(RenderResource&& other) noexcept {
		renderer_ = std::exchange(other.renderer_, nullptr);
		resource_handle_ = std::exchange(other.resource_handle_, std::monostate{});
		srv_view_ = std::exchange(other.srv_view_, std::monostate{});
		srv_resource_index_ = std::exchange(other.srv_resource_index_, ~0u);
		uav_views_ = std::exchange(other.uav_views_, {});
		uav_resource_indices_ = std::exchange(other.uav_resource_indices_, {});
		state_ = std::exchange(other.state_, {});
	}

	auto RenderResource::operator=(RenderResource&& other) noexcept -> RenderResource& {
		if (this != &other) {
			renderer_ = std::exchange(other.renderer_, nullptr);
			resource_handle_ = std::exchange(other.resource_handle_, std::monostate{});
			srv_view_ = std::exchange(other.srv_view_, std::monostate{});
			srv_resource_index_ = std::exchange(other.srv_resource_index_, ~0u);
			uav_views_ = std::exchange(other.uav_views_, {});
			uav_resource_indices_ = std::exchange(other.uav_resource_indices_, {});
			state_ = std::exchange(other.state_, {});
		}

		return *this;
	}

	auto RenderResource::setup(Image&& image, ResourceStateFlags initial_flags) -> void {
		resource_handle_ = std::move(image);
		state_ = initial_flags;

		auto& handle = std::get<Image>(resource_handle_);
		auto usage = handle.get_usage();
		auto extent = handle.get_extent(); 

		vk::ImageViewType view_type{};
		if (extent.depth > 1u) {
			view_type = vk::ImageViewType::e3D;
		}
		else if (extent.height > 1u) {
			if (handle.get_face_count() == 6u) {
				view_type = handle.get_layer_count() > 1u ? vk::ImageViewType::eCubeArray : vk::ImageViewType::eCube;
			}
			else {
				view_type = handle.get_layer_count() > 1u ? vk::ImageViewType::e2DArray : vk::ImageViewType::e2D;
			}
		}
		else {
			view_type = handle.get_layer_count() > 1u ? vk::ImageViewType::e1DArray : vk::ImageViewType::e1D;
		}

		vk::ImageSubresourceRange srv_subresource_range{};
		srv_subresource_range.aspectMask = vk::ImageAspectFlagBits::eColor;
		srv_subresource_range.baseMipLevel = 0u;
		srv_subresource_range.levelCount = handle.get_level_count();
		srv_subresource_range.baseArrayLayer = 0u;
		srv_subresource_range.layerCount = handle.get_layer_count() * handle.get_face_count();

		auto srv_view_result = handle.create_view(srv_subresource_range, view_type);
		GFX_ASSERT_MSG(srv_view_result.has_value(), "Failed to create srv image view.");
		srv_view_ = std::move(srv_view_result.value());

		srv_resource_index_ = srv_free_list_.allocate();

		// UAV descriptors not needed for non storage targets (read only)
		if (usage & vk::ImageUsageFlagBits::eStorage) {
			// Slang is not support cube arrays
			if (view_type == vk::ImageViewType::eCubeArray) {
				view_type = vk::ImageViewType::e2DArray;
			}

			// TODO: UAV descriptors needed only for render targets
			for (int32_t mip = 0; mip < static_cast<int32_t>(handle.get_level_count()); ++mip) {
				vk::ImageSubresourceRange uav_subresource_range{};
				uav_subresource_range.aspectMask = vk::ImageAspectFlagBits::eColor;
				uav_subresource_range.baseMipLevel = mip;
				uav_subresource_range.levelCount = 1u;
				uav_subresource_range.baseArrayLayer = 0u;
				uav_subresource_range.layerCount = handle.get_layer_count() * handle.get_face_count();

				auto uav_view_result = handle.create_view(uav_subresource_range, view_type);
				GFX_ASSERT_MSG(uav_view_result.has_value(), "Failed to create uav image view for mip {}.", mip);
				uav_views_.push_back(std::move(uav_view_result.value()));

				uav_resource_indices_.push_back(uav_free_list_.allocate());
			}
		}
	}

	auto RenderResource::setup(Buffer&& buffer, ResourceStateFlags initial_flags) -> void {
		resource_handle_ = std::move(buffer);
		state_ = initial_flags;
	}

	auto RenderResource::update(Image&& image, ResourceStateFlags initial_flags) -> void {
		reset();
		setup(std::move(image), initial_flags);
	}

	auto RenderResource::update(Buffer&& buffer, ResourceStateFlags initial_flags) -> void {
		reset();
		setup(std::move(buffer), initial_flags);
	}

	auto RenderResource::reset() -> void {
		if (std::holds_alternative<BufferView>(srv_view_)) {
			renderer_->deletion_queue_.push_back(std::move(std::get<BufferView>(srv_view_)));
		}
		else if (std::holds_alternative<ImageView>(srv_view_)) {
			renderer_->deletion_queue_.push_back(std::move(std::get<ImageView>(srv_view_)));
		}

		for (auto&& uav_view : uav_views_) {
			if (std::holds_alternative<BufferView>(uav_view)) {
				renderer_->deletion_queue_.push_back(std::move(std::get<BufferView>(uav_view)));
			}
			else if (std::holds_alternative<ImageView>(uav_view)) {
				renderer_->deletion_queue_.push_back(std::move(std::get<ImageView>(uav_view)));
			}
		}

		if (std::holds_alternative<Buffer>(resource_handle_)) {
			renderer_->deletion_queue_.push_back(std::move(std::get<Buffer>(resource_handle_)));
		}
		else if (std::holds_alternative<Image>(resource_handle_)) {
			renderer_->deletion_queue_.push_back(std::move(std::get<Image>(resource_handle_)));
		}

		state_ = ResourceStateFlag::eUndefined;

		if (srv_resource_index_ != ~0u) {
			srv_free_list_.deallocate(srv_resource_index_);
			srv_resource_index_ = ~0u;
		}

		for (auto& index : uav_resource_indices_) {
			uav_free_list_.deallocate(index);
		}
		uav_resource_indices_.clear();
	}

	// TODO: Select descriptor type
	auto RenderResource::get_descriptor(uint32_t mip) const -> DescriptorType {
		if (std::holds_alternative<Buffer>(resource_handle_)) {
			auto& buffer = std::get<Buffer>(resource_handle_);
			return vk::DescriptorBufferInfo{ buffer.get_handle(), 0ull, buffer.get_size() };
		}
		else if (std::holds_alternative<Image>(resource_handle_)) {
			if (mip == ~0u) {
				auto& image_view = std::get<ImageView>(srv_view_);
				return vk::DescriptorImageInfo{ VK_NULL_HANDLE, image_view.get_handle(), vk::ImageLayout::eShaderReadOnlyOptimal };
			}

			auto& image_view = std::get<ImageView>(uav_views_[mip]);
			return vk::DescriptorImageInfo{ VK_NULL_HANDLE, image_view.get_handle(), vk::ImageLayout::eGeneral };
		}
		return std::monostate{};
	}

	auto RenderResource::has_handle() const -> bool {
		return !std::holds_alternative<std::monostate>(resource_handle_);
	}

	auto RenderResource::transfer_state(CommandBuffer const& cmdbuf, ResourceStateFlags new_state) -> void {
		if (new_state == state_) {
			// Already in this state
			return;
		}

		if (std::holds_alternative<Buffer>(resource_handle_)) {
			auto& handle = std::get<Buffer>(resource_handle_);

			BufferBarrier barrier{};
			barrier.buffer = &handle;
			barrier.src_state = state_;
			barrier.dst_state = new_state;
			cmdbuf.push_barrier(barrier);

			state_ = new_state;
		}
		else if (std::holds_alternative<Image>(resource_handle_)) {
			auto& handle = std::get<Image>(resource_handle_);
			auto usage = handle.get_usage();

			ImageBarrier barrier{};
			barrier.src_state = state_;
			barrier.dst_state = new_state;
			barrier.image = &handle;
			barrier.subresource_range.aspectMask = (usage & vk::ImageUsageFlagBits::eDepthStencilAttachment) ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor;
			barrier.subresource_range.baseMipLevel = 0u;
			barrier.subresource_range.levelCount = handle.get_level_count();
			barrier.subresource_range.baseArrayLayer = 0u;
			barrier.subresource_range.layerCount = handle.get_layer_count() * handle.get_face_count();
			cmdbuf.push_barrier(barrier);

			state_ = new_state;
		}
		else {
			GFX_ASSERT_MSG(false, "RenderResource is not initialized yet.");
		}
	}

#undef EDGE_LOGGER_SCOPE // RenderResource


#define EDGE_LOGGER_SCOPE "gfx::Frame"

	Frame::~Frame() {
		
	}

	auto Frame::construct(CommandBuffer&& command_buffer, DescriptorSetLayout const& descriptor_layout) -> Result<Frame> {
		Frame self{};
		self.command_buffer_ = std::move(command_buffer);
		if (auto result = self._construct(descriptor_layout); result != vk::Result::eSuccess) {
			return std::unexpected(result);
		}
		return self;
	}

	auto Frame::begin() -> void {
		GFX_ASSERT_MSG(!is_recording_, "Already recording rendering command buffer.");
		if (!is_recording_) {
			// Wait rendering finished
			fence_.wait(1000000000ull);
			fence_.reset();

			is_recording_ = command_buffer_.begin() == vk::Result::eSuccess;
			GFX_ASSERT_MSG(is_recording_, "Failed to begin command buffer.");
		}
	}

	auto Frame::end() -> void {
		GFX_ASSERT_MSG(is_recording_, "Rendering command buffer is not recording.");
		if (is_recording_) {
			command_buffer_.end();
			is_recording_ = false;
		}
	}

	auto Frame::_construct(DescriptorSetLayout const& descriptor_layout) -> vk::Result {
		assert(device_ && "Device handle is null.");

		auto image_sem_result = Semaphore::create();
		if (!image_sem_result) {
			GFX_ASSERT_MSG(false, "Failed to create image available semaphore: {}", vk::to_string(image_sem_result.error()));
			return image_sem_result.error();
		}
		image_available_ = std::move(image_sem_result.value());

		auto render_sem_result = Semaphore::create();
		if (!render_sem_result) {
			GFX_ASSERT_MSG(false, "Failed to create rendering finished semaphore: {}", vk::to_string(render_sem_result.error()));
			return render_sem_result.error();
		}
		rendering_finished_ = std::move(render_sem_result.value());

		auto fence_result = Fence::create(vk::FenceCreateFlagBits::eSignaled);
		if (!fence_result) {
			GFX_ASSERT_MSG(false, "Failed to create frame fence: {}", vk::to_string(fence_result.error()));
			return fence_result.error();
		}
		fence_ = std::move(fence_result.value());

		return vk::Result::eSuccess;
	}

#undef EDGE_LOGGER_SCOPE // Frames

#define EDGE_LOGGER_SCOPE "gfx::Renderer"

	Renderer::~Renderer() {
		auto wait_result = queue_->waitIdle();
		GFX_ASSERT_MSG(wait_result == vk::Result::eSuccess, "Failed waiting for queue finish all work before destruction.");

		descriptor_pool_.free_descriptor_set(descriptor_set_);
	}

	auto Renderer::construct(const RendererCreateInfo& create_info) -> Result<std::unique_ptr<Renderer>> {
		auto self = std::make_unique<Renderer>();
		if (auto result = self->_construct(create_info); result != vk::Result::eSuccess) {
			return std::unexpected(result);
		}
		return self;
	}

	auto Renderer::create_render_resource() -> uint32_t {
		render_resources_.emplace_back(this);
		return render_resource_free_list_.allocate();
	}

	auto Renderer::setup_render_resource(uint32_t resource_id, Image&& image, ResourceStateFlags initial_state) -> void {
		GFX_ASSERT_MSG(resource_id < render_resources_.size(), "Invalid render resource index: {}", resource_id);
		auto& render_resource = render_resources_[resource_id];
		render_resource.setup(std::move(image), initial_state);

		// Check do we need to update this resource for final state (like for loaded from disk resources)
		auto& image_resource = render_resource.get_handle<Image>();
		auto image_usage = image_resource.get_usage();

		auto is_color_attachment = (image_usage & vk::ImageUsageFlagBits::eColorAttachment) == vk::ImageUsageFlagBits::eColorAttachment;
		auto is_depth_attachment = (image_usage & vk::ImageUsageFlagBits::eDepthStencilAttachment) == vk::ImageUsageFlagBits::eDepthStencilAttachment;
		auto is_attachment = is_color_attachment || is_depth_attachment;
		if (!is_attachment && initial_state != ResourceStateFlag::eShaderResource) {
			GFX_ASSERT_MSG(active_frame_, "This call should be between begin_frame and end_frame.");
			auto& cmdbuf = active_frame_->get_command_buffer();
			render_resource.transfer_state(cmdbuf, ResourceStateFlag::eShaderResource);
		}

		// Write resource descriptors to descriptor table
		vk::WriteDescriptorSet descriptor_write{};
		descriptor_write.dstSet = descriptor_set_.get_handle();
		descriptor_write.descriptorCount = 1u;
		descriptor_write.pBufferInfo = nullptr;
		descriptor_write.pTexelBufferView = nullptr;

		// At first write SRV descriptor
		if (image_usage & vk::ImageUsageFlagBits::eSampled) {
			image_descriptors_.push_back(std::move(std::get<vk::DescriptorImageInfo>(render_resource.get_descriptor())));
			descriptor_write.dstBinding = SRV_TEXTURE_SLOT;
			descriptor_write.dstArrayElement = render_resource.get_srv_index();
			descriptor_write.descriptorType = vk::DescriptorType::eSampledImage;
			descriptor_write.pImageInfo = &image_descriptors_.back();
			write_descriptor_sets_.push_back(descriptor_write);
		}

		// Not needed to write UAV descriptors for non storage images
		// Write uav descriptors
		if (image_usage & vk::ImageUsageFlagBits::eStorage) {
			descriptor_write.dstBinding = UAV_TEXTURE_SLOT;
			descriptor_write.descriptorType = vk::DescriptorType::eStorageImage;

			for (int32_t mip = 0; mip < image.get_level_count(); ++mip) {
				image_descriptors_.push_back(std::move(std::get<vk::DescriptorImageInfo>(render_resource.get_descriptor(mip))));

				descriptor_write.dstArrayElement = render_resource.get_uav_index(mip);
				descriptor_write.pImageInfo = &image_descriptors_.back();
				write_descriptor_sets_.push_back(descriptor_write);
			}
		}
	}

	auto Renderer::setup_render_resource(uint32_t resource_id, Buffer&& buffer, ResourceStateFlags initial_state) -> void {
		auto& render_resource = render_resources_[resource_id];
		render_resource.setup(std::move(buffer), initial_state);
		// NOTE: Not needed to update descriptors, because using pointers for buffers
	}

	auto Renderer::get_render_resource(uint32_t resource_id) -> RenderResource& {
		return render_resources_[resource_id];
	}

	auto Renderer::set_render_area(vk::Rect2D render_area) -> void {
		render_area_ = render_area;
	}

	auto Renderer::set_layer_count(uint32_t layer_count) -> void {
		layer_count_ = layer_count;
	}

	auto Renderer::add_color_attachment(uint32_t resource_id, vk::AttachmentLoadOp load_op, vk::ClearColorValue clear_color) -> void {
		auto& render_resource = render_resources_[resource_id];
		auto& image = render_resource.get_handle<Image>();

		auto resource_state = render_resource.get_state();
		if (resource_state != ResourceStateFlag::eRenderTarget) {
			ImageBarrier barrier{};
			barrier.image = &image;
			barrier.src_state = resource_state;
			barrier.dst_state = ResourceStateFlag::eRenderTarget;
			barrier.subresource_range.aspectMask = vk::ImageAspectFlagBits::eColor;
			barrier.subresource_range.baseMipLevel = 0u;
			barrier.subresource_range.levelCount = image.get_level_count();
			barrier.subresource_range.baseArrayLayer = 0u;
			barrier.subresource_range.layerCount = image.get_layer_count() * image.get_face_count();
			image_barriers_.push_back(barrier);
			render_resource.set_state(barrier.dst_state);
		}

		auto& image_view = render_resource.get_srv_view<ImageView>();

		vk::RenderingAttachmentInfo attachment_info{};
		attachment_info.imageView = image_view.get_handle();
		attachment_info.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
		attachment_info.loadOp = load_op;
		attachment_info.storeOp = vk::AttachmentStoreOp::eNone;
		attachment_info.clearValue = clear_color;
		color_attachments_.push_back(attachment_info);
	}

	auto Renderer::add_shader_pass(ShaderPassInfo&& shader_pass_info) -> void {
		shader_passes_.push_back(std::move(shader_pass_info));
	}

	auto Renderer::begin_frame(float delta_time) -> void {
		if (active_frame_) {
			EDGE_SLOGW("Attempting to start a new frame when the old one is not finished.");
			return;
		}

		auto swapchain_recreated = handle_surface_change();

		auto* current_frame = get_current_frame();
		current_frame->begin();

		deletion_queue_.clear();

		auto const& semaphore = current_frame->get_image_available_semaphore();
		acquired_semaphore_ = *semaphore;

		if (swapchain_) {
			auto result = device_->acquireNextImageKHR(swapchain_, 1000000000ull, acquired_semaphore_, VK_NULL_HANDLE, &swapchain_image_index_);
			if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
				swapchain_recreated = handle_surface_change(true);

				// Try to acquire next image again after recreation
				if (swapchain_recreated) {
					result = device_->acquireNextImageKHR(swapchain_, ~0ull, acquired_semaphore_, VK_NULL_HANDLE, &swapchain_image_index_);
				}

				if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
					return;
				}
			}
		}

		if (current_frame->is_recording()) {
			active_frame_ = current_frame;
		}

		// Get previous frame time 
		uint64_t timestamps[2] = {};
		timestamp_query_.get_data(0u, &timestamps[0]);

		uint64_t elapsed_time = timestamps[1] - timestamps[0];
		if (timestamps[1] <= timestamps[0]) {
			elapsed_time = 0ull;
		}

		gpu_delta_time_ = static_cast<double>(elapsed_time) * timestamp_frequency_ / 1000000.0;

		auto const& cmdbuf = active_frame_->get_command_buffer();
		cmdbuf->resetQueryPool(*timestamp_query_, 0u, 2u);
		cmdbuf->writeTimestamp2KHR(vk::PipelineStageFlagBits2::eTopOfPipe, *timestamp_query_, 0u);

		// Bind global descriptor and layout
		vk::BindDescriptorSetsInfo bind_descriptor_info{};
		bind_descriptor_info.stageFlags = vk::ShaderStageFlagBits::eAllGraphics | vk::ShaderStageFlagBits::eCompute;
		bind_descriptor_info.layout = pipeline_layout_.get_handle();
		bind_descriptor_info.firstSet = 0u;
		bind_descriptor_info.dynamicOffsetCount = 0u;
		bind_descriptor_info.pDynamicOffsets = nullptr;
		bind_descriptor_info.descriptorSetCount = 1u;
		auto const& descriptor_set_handle = descriptor_set_.get_handle();
		bind_descriptor_info.pDescriptorSets = &descriptor_set_handle;

		cmdbuf->bindDescriptorSets2KHR(&bind_descriptor_info);

		delta_time_ = delta_time;
	}

	auto Renderer::execute_graph(float delta_time) -> void {
		GFX_ASSERT_MSG(active_frame_, "Can't execute graph, because frame is not active.");
		if (!active_frame_) {
			return;
		}

		auto& cmd = active_frame_->get_command_buffer();
		for (auto& shader_pass : shader_passes_) {
			auto* pipeline = shader_library_.get_pipeline(shader_pass.pipeline_name);
			GFX_ASSERT_MSG(pipeline, "Pipeline \"{}\" was not found.", shader_pass.pipeline_name);
			if (!pipeline) {
				continue;
			}

			if (shader_pass.setup_cb) {
				shader_pass.setup_cb(*this, cmd);
			}

			Barrier barrier{};
			barrier.image_barriers = image_barriers_;
			cmd.push_barrier(barrier);
			image_barriers_.clear();

			auto bind_point = pipeline->get_bind_point();
			auto is_graphics_pipeline = bind_point == vk::PipelineBindPoint::eGraphics;
			cmd->bindPipeline(bind_point, pipeline->get_handle());

			if (is_graphics_pipeline) {
				vk::RenderingInfoKHR rendering_info{};
				rendering_info.renderArea = render_area_;
				rendering_info.layerCount = layer_count_;
				rendering_info.colorAttachmentCount = static_cast<uint32_t>(color_attachments_.size());
				rendering_info.pColorAttachments = rendering_info.colorAttachmentCount != 0u ? color_attachments_.data() : nullptr;
				rendering_info.pDepthAttachment = depth_attachment_.imageView ? &depth_attachment_ : nullptr;
				rendering_info.pStencilAttachment = stencil_attachment_.imageView ? &stencil_attachment_ : nullptr;

				cmd->beginRenderingKHR(&rendering_info);

				color_attachments_.clear();
				depth_attachment_ = vk::RenderingAttachmentInfo{};
				stencil_attachment_ = vk::RenderingAttachmentInfo{};
			}

			if (shader_pass.execute_cb) {
				shader_pass.execute_cb(*this, cmd, delta_time);
			}

			if (is_graphics_pipeline) {
				cmd->endRenderingKHR();
			}
		}

		shader_passes_.clear();
	}

	auto Renderer::end_frame(Span<vk::SemaphoreSubmitInfoKHR> wait_external_semaphores) -> void {
		if (!active_frame_) {
			EDGE_SLOGW("Attempting to end a frame when the new one is not started yet.");
			return;
		}

		auto const& cmdbuf = active_frame_->get_command_buffer();

		auto& backbuffer_resource = get_backbuffer_resource();
		if (backbuffer_resource.get_state() != ResourceStateFlag::ePresent) {
			backbuffer_resource.transfer_state(cmdbuf, ResourceStateFlag::ePresent);
		}

		if (!write_descriptor_sets_.empty()) {
			device_->updateDescriptorSets(write_descriptor_sets_.size(), write_descriptor_sets_.data(), 0u, nullptr);
			write_descriptor_sets_.clear();
			image_descriptors_.clear();
			buffer_descriptors_.clear();
		}

		cmdbuf->writeTimestamp2KHR(vk::PipelineStageFlagBits2::eTopOfPipe, *timestamp_query_, 1u);

		active_frame_->end();

		mi::Vector<vk::SemaphoreSubmitInfo> wait_semaphores{};
		wait_semaphores.push_back(vk::SemaphoreSubmitInfo{ acquired_semaphore_, 0ull, vk::PipelineStageFlagBits2::eColorAttachmentOutput });

		for (auto& external_semaphore : wait_external_semaphores) {
			if (external_semaphore.semaphore) {
				wait_semaphores.push_back(external_semaphore);
			}
		}

		mi::Vector<vk::SemaphoreSubmitInfo> signal_semaphores{};

		vk::Semaphore rendering_finished_sem = active_frame_->get_rendering_finished_semaphore();
		signal_semaphores.push_back(vk::SemaphoreSubmitInfo{ rendering_finished_sem, 0ull, vk::PipelineStageFlagBits2::eColorAttachmentOutput });

		vk::CommandBufferSubmitInfo cmd_buffer_submit_info{};
		cmd_buffer_submit_info.commandBuffer = *cmdbuf;

		vk::SubmitInfo2 submit_info{};
		submit_info.waitSemaphoreInfoCount = static_cast<uint32_t>(wait_semaphores.size());
		submit_info.pWaitSemaphoreInfos = wait_semaphores.data();
		submit_info.signalSemaphoreInfoCount = static_cast<uint32_t>(signal_semaphores.size());
		submit_info.pSignalSemaphoreInfos = signal_semaphores.data();
		submit_info.commandBufferInfoCount = 1u;
		submit_info.pCommandBufferInfos = &cmd_buffer_submit_info;

		if (auto result = queue_->submit2KHR(1u, &submit_info, active_frame_->get_fence()); result != vk::Result::eSuccess) {
			GFX_ASSERT_MSG(false, "Failed to submit queue. Reason: {}", vk::to_string(result));
			return;
		}
		
		if (swapchain_) {
			vk::PresentInfoKHR present_info{};
			present_info.waitSemaphoreCount = 1u;
			present_info.pWaitSemaphores = &rendering_finished_sem;
			present_info.swapchainCount = 1u;
			present_info.pSwapchains = &(*swapchain_);
			present_info.pImageIndices = &swapchain_image_index_;

			if (auto result = queue_->presentKHR(&present_info); result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
				GFX_ASSERT_MSG(false, "Failed to present images. Reason: {}", vk::to_string(result));
				return;
			}
		}

		active_frame_ = nullptr;
		frame_number_++;
	}
	auto Renderer::get_backbuffer_resource_id() -> uint32_t {
		return swapchain_targets_[swapchain_image_index_];
	}

	auto Renderer::get_backbuffer_resource() -> RenderResource& {
		return render_resources_[get_backbuffer_resource_id()];
	}

	auto Renderer::push_constant_range(CommandBuffer const& cmd, vk::ShaderStageFlags stage_flags, Span<const uint8_t> range) const -> void {
		cmd->pushConstants(pipeline_layout_.get_handle(), stage_flags, 0u, range.size(), range.data());
	}

	auto Renderer::_construct(const RendererCreateInfo& create_info) -> vk::Result {
		GFX_ASSERT_MSG(device_, "Device handle is null.");

		write_descriptor_sets_.reserve(256);
		image_descriptors_.reserve(256);
		buffer_descriptors_.reserve(256);

		auto queue_result = device_.get_queue({
				.required_caps = QueuePresets::kPresentGraphics,
				.strategy = QueueSelectionStrategy::ePreferDedicated
			});
		if (!queue_result) {
			GFX_ASSERT_MSG(false, "Failed to request queue. Reason: {}.", vk::to_string(queue_result.error()));
			return queue_result.error();
		}
		queue_ = std::move(queue_result.value());

		auto command_pool_result = queue_.create_command_pool();
		if (!command_pool_result) {
			GFX_ASSERT_MSG(false, "Failed to create command pool. Reason: {}.", vk::to_string(command_pool_result.error()));
			return command_pool_result.error();
		}
		command_pool_ = std::move(command_pool_result.value());

		vk::QueryPoolCreateInfo query_pool_create_info{};
		query_pool_create_info.queryCount = 1u;
		query_pool_create_info.queryType = vk::QueryType::eTimestamp;

		auto query_pool_result = QueryPool::create(vk::QueryType::eTimestamp, 1u);
		if (!query_pool_result) {
			GFX_ASSERT_MSG(false, "Failed to create timestamp query. Reason: {}.", vk::to_string(query_pool_result.error()));
			return query_pool_result.error();
		}
		timestamp_query_ = std::move(query_pool_result.value());
		timestamp_query_.reset(0u);

		vk::PhysicalDeviceProperties adapter_properties;
		adapter_->getProperties(&adapter_properties);

		timestamp_frequency_ = adapter_properties.limits.timestampPeriod;

		// Create descriptor layout
		DescriptorSetLayoutBuilder set_layout_builder{};
		set_layout_builder.add_binding(SAMPLER_SLOT, vk::DescriptorType::eSampler, MAX_SAMPLER_SLOTS,
			vk::ShaderStageFlagBits::eAllGraphics | vk::ShaderStageFlagBits::eCompute,
			vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eUpdateAfterBind);
		set_layout_builder.add_binding(SRV_TEXTURE_SLOT, vk::DescriptorType::eSampledImage, MAX_SRV_TEXTURE_SLOTS,
			vk::ShaderStageFlagBits::eAllGraphics | vk::ShaderStageFlagBits::eCompute,
			vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eUpdateAfterBind);
		set_layout_builder.add_binding(UAV_TEXTURE_SLOT, vk::DescriptorType::eStorageImage, MAX_UAV_TEXTURE_SLOTS,
			vk::ShaderStageFlagBits::eAllGraphics | vk::ShaderStageFlagBits::eCompute,
			vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eUpdateAfterBind);

		auto descriptor_set_layout_result = set_layout_builder.build(vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool);
		if (!descriptor_set_layout_result) {
			GFX_ASSERT_MSG(false, "Failed to create descriptor set layout. Reason: {}.", vk::to_string(descriptor_set_layout_result.error()));
			return descriptor_set_layout_result.error();
		}
		descriptor_layout_ = std::move(descriptor_set_layout_result.value());

		auto const& requested_sizes = descriptor_layout_.get_pool_sizes();
		auto descriptor_pool_result = DescriptorPool::create(requested_sizes, 1u, vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet | vk::DescriptorPoolCreateFlagBits::eUpdateAfterBindEXT);
		if (!descriptor_pool_result) {
			GFX_ASSERT_MSG(false, "Failed to create frame descriptor pool handle. Reason: {}.", vk::to_string(descriptor_pool_result.error()));
			return descriptor_pool_result.error();
		}
		descriptor_pool_ = std::move(descriptor_pool_result.value());

		auto descriptor_set_result = descriptor_pool_.allocate_descriptor_set(descriptor_layout_);
		if (!descriptor_set_result) {
			GFX_ASSERT_MSG(false, "Failed to create frame descriptor set handle. Reason: {}.", vk::to_string(descriptor_set_result.error()));
			return descriptor_set_result.error();
		}
		descriptor_set_ = std::move(descriptor_set_result.value());

		PipelineLayoutBuilder pipeline_layout_builder{};
		// TODO: add ability to set specific set point
		pipeline_layout_builder.add_set_layout(descriptor_layout_);
		pipeline_layout_builder.add_constant_range(vk::ShaderStageFlagBits::eAllGraphics | vk::ShaderStageFlagBits::eCompute, 0u, adapter_properties.limits.maxPushConstantsSize);

		auto pipeline_layout_result = pipeline_layout_builder.build();
		if (!pipeline_layout_result) {
			GFX_ASSERT_MSG(false, "Failed to create common pipeline layout. Reason: {}.", vk::to_string(pipeline_layout_result.error()));
			return pipeline_layout_result.error();
		}
		pipeline_layout_ = std::move(pipeline_layout_result.value());

		push_constant_buffer_.resize(adapter_properties.limits.maxPushConstantsSize);

		if (auto result = create_swapchain({
			.format = {create_info.preferred_format, create_info.preferred_color_space},
			.extent = create_info.extent,
			.vsync = create_info.enable_vsync,
			.hdr = create_info.enable_hdr
			}); result != vk::Result::eSuccess) {
			return result;
		}

		ShaderLibraryInfo shader_library_info{};
		shader_library_info.pipeline_layout = &pipeline_layout_;
		shader_library_info.pipeline_cache_path = u8"/shader_cache.cache";
		shader_library_info.library_path = u8"/assets/shaders";
		shader_library_info.backbuffer_format = swapchain_.get_format();

		auto shader_library_result = ShaderLibrary::construct(shader_library_info);
		if (!shader_library_result) {
			GFX_ASSERT_MSG(false, "Failed to create shader library. Reason: {}.", vk::to_string(shader_library_result.error()));
			return shader_library_result.error();
		}
		shader_library_ = std::move(shader_library_result.value());

		for (int32_t i = 0; i < k_frame_overlap_; ++i) {
			auto command_list_result = command_pool_.allocate_command_buffer();
			if (!command_list_result) {
				GFX_ASSERT_MSG(false, "Failed to allocate command list for frame at index {}. Reason: {}.", i, vk::to_string(command_list_result.error()));
				return command_list_result.error();
			}

			if (auto frame_result = Frame::construct(std::move(command_list_result.value()), descriptor_layout_); !frame_result.has_value()) {
				GFX_ASSERT_MSG(false, "Failed to create frame at index {}. Reason: {}.", i, vk::to_string(frame_result.error()));
				return frame_result.error();
			}
			else {
				frames_.push_back(std::move(frame_result.value()));
			}
		}

		vk::SamplerCreateInfo sampler_create_info{};
		sampler_create_info.magFilter = vk::Filter::eLinear;
		sampler_create_info.minFilter = vk::Filter::eLinear;
		sampler_create_info.mipmapMode = vk::SamplerMipmapMode::eLinear;
		sampler_create_info.addressModeU = vk::SamplerAddressMode::eClampToEdge;
		sampler_create_info.addressModeV = vk::SamplerAddressMode::eClampToEdge;
		sampler_create_info.addressModeW = vk::SamplerAddressMode::eClampToEdge;
		sampler_create_info.mipLodBias = 1.0f;
		sampler_create_info.anisotropyEnable = VK_TRUE;
		sampler_create_info.maxAnisotropy = 4.0f;

		auto sampler_result = Sampler::create(sampler_create_info);
		if (!sampler_result) {
			GFX_ASSERT_MSG(false, "Failed to create test sampler. Reason: {}.", vk::to_string(sampler_result.error()));
		}
		test_sampler_ = std::move(sampler_result.value());

		// Push test sampler
		vk::DescriptorImageInfo sampler_descriptor{};
		sampler_descriptor.sampler = test_sampler_.get_handle();
		sampler_descriptor.imageView = nullptr;
		sampler_descriptor.imageLayout = vk::ImageLayout::eUndefined;
		image_descriptors_.push_back(std::move(sampler_descriptor));

		vk::WriteDescriptorSet sampler_write{};
		sampler_write.dstSet = descriptor_set_.get_handle();
		sampler_write.dstBinding = SAMPLER_SLOT;
		sampler_write.dstArrayElement = 0u;
		sampler_write.descriptorCount = 1u;
		sampler_write.descriptorType = vk::DescriptorType::eSampler;
		sampler_write.pImageInfo = &image_descriptors_.back();
		sampler_write.pBufferInfo = nullptr;
		sampler_write.pTexelBufferView = nullptr;
		write_descriptor_sets_.push_back(std::move(sampler_write));

		return vk::Result::eSuccess;
	}

	auto Renderer::handle_surface_change(bool force) -> bool {
		if (!swapchain_) {
			EDGE_SLOGW("Can't handle surface changes in headless mode, skipping.");
			return false;
		}

		vk::SurfaceCapabilitiesKHR surface_capabilities;
		if (auto result = adapter_->getSurfaceCapabilitiesKHR(surface_, &surface_capabilities); result != vk::Result::eSuccess) {
			return false;
		}

		if (surface_capabilities.currentExtent.width == 0xFFFFFFFF || surface_capabilities.currentExtent.height == 0xFFFFFFFF) {
			return false;
		}

		auto current_extent = swapchain_.get_extent();
		if (current_extent != surface_capabilities.currentExtent) {
			if (auto result = queue_->waitIdle(); result != vk::Result::eSuccess) {
				return false;
			}

			auto swapchain_state = swapchain_.get_state();
			swapchain_state.extent = surface_capabilities.currentExtent;

			if (auto result = create_swapchain(swapchain_state); result != vk::Result::eSuccess) {
				return false;
			}

			active_frame_ = nullptr;
			swapchain_image_index_ = 0u;
			return true;
		}

		return false;
	}

	auto Renderer::create_swapchain(const Swapchain::State& state) -> vk::Result {
		if (auto result = SwapchainBuilder{}
			.set_old_swapchain(*swapchain_)
			.set_image_extent(state.extent)
			.set_image_format(state.format.format)
			.set_color_space(state.format.colorSpace)
			.set_image_count(state.image_count)
			.enable_hdr(state.hdr)
			.enable_vsync(state.vsync)
			.build(); !result.has_value()) {
			GFX_ASSERT_MSG(false, "Failed to recreate swapchain with reason: {}", vk::to_string(result.error()));
			return result.error();
		}
		else {
			swapchain_.reset();
			swapchain_ = std::move(result.value());

			auto images_result = swapchain_.get_images();
			if (!images_result) {
				GFX_ASSERT_MSG(false, "Failed to request swapchain images. Reason: {}.", vk::to_string(images_result.error()));
				return result.error();
			}
			auto swapchain_images = std::move(images_result.value());

			if (swapchain_targets_.empty()) {
				for (auto&& image : swapchain_images) {
					auto new_resource = create_render_resource();
					setup_render_resource(new_resource, std::move(image), ResourceStateFlag::eUndefined);
					swapchain_targets_.push_back(new_resource);
				}
			}
			else {
				for (int32_t i = 0; i < static_cast<int32_t>(swapchain_images.size()); ++i) {
					auto& render_resource = render_resources_[swapchain_targets_[i]];
					render_resource.update(std::move(swapchain_images[i]), ResourceStateFlag::eUndefined);
				}
			}
		}

		return vk::Result::eSuccess;
	}

#undef EDGE_LOGGER_SCOPE // Renderer

}