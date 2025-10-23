#include "gfx_renderer.h"

#include <numeric>

#include <stb_image.h>

#include <ktx.h>
#include <zstd.h>

#include "gfx_shader_effect.h"

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

//#define EDGE_LOGGER_SCOPE "gfx::ResourceLoader"
//
//	auto ResourceLoader::construct(Context const* ctx, vk::DeviceSize arena_size, uint32_t frames_in_flight) -> Result<ResourceLoader> {
//		ResourceLoader self{ ctx };
//		if (auto result = self._construct(arena_size, frames_in_flight); result != vk::Result::eSuccess) {
//			return std::unexpected(result);
//		}
//		return self;
//	}
//
//	auto ResourceLoader::load_image_from_memory(Span<uint8_t> image_data) -> Result<Image> {
//
//		if(image_data.size() < 4) {
//			return std::unexpected(vk::Result::eErrorUnknown);
//		}
//
//		auto magic = image_data.subspan(0, 4);
//
//		// PNG: 89 50 4E 47
//		if (image_data.size() >= 8 && magic[0] == 0x89 && magic[1] == 0x50 && magic[2] == 0x4E && magic[3] == 0x47) {
//			return _load_image_from_stbi(image_data);
//		}
//		// JPEG: FF D8 FF
//		else if (magic[0] == 0xFF && magic[1] == 0xD8 && magic[2] == 0xFF) {
//			return _load_image_from_stbi(image_data);
//		}
//		// KTX: AB 4B 54 58
//		else if (magic[0] == 0xAB && magic[1] == 0x4B && magic[2] == 0x54 && magic[3] == 0x58) {
//			return _load_image_from_ktx(image_data);
//		}
//
//		return std::unexpected(vk::Result::eErrorUnknown);
//	}
//
//	auto ResourceLoader::_construct(vk::DeviceSize arena_size, uint32_t frames_in_flight) -> vk::Result {
//		for (int32_t i = 0; i < static_cast<int32_t>(frames_in_flight); ++i) {
//			auto result = BufferArena::construct(context_, arena_size, kStagingBuffer);
//			if (!result) {
//				return result.error();
//			}
//
//			staging_arenas_.push_back(std::move(result.value()));
//		}
//
//		return vk::Result::eSuccess;
//	}
//
//	auto ResourceLoader::_load_image_from_stbi(Span<uint8_t> image_data) -> Result<Image> {
//		int32_t width, height, channel_count;
//
//		ImageCreateInfo create_info{};
//		create_info.extent.width = static_cast<uint32_t>(width);
//		create_info.extent.height = static_cast<uint32_t>(height);
//		create_info.extent.depth = 1u;
//		create_info.flags = ImageFlag::eSample | ImageFlag::eCopyTarget;
//
//		uint8_t* data = stbi_load_from_memory(image_data.data(), static_cast<int32_t>(image_data.size()), &width, &height, &channel_count, 0);
//		if (!data) {
//			return std::unexpected(vk::Result::eErrorUnknown);
//		}
//
//		auto texture_size = create_info.extent.width * create_info.extent.height * static_cast<uint32_t>(channel_count);
//
//		stbi_image_free(data);
//
//		return std::unexpected(vk::Result::eErrorUnknown);
//	}
//
//	auto ResourceLoader::_load_image_from_ktx(Span<uint8_t> image_data) -> Result<Image> {
//		return std::unexpected(vk::Result::eErrorUnknown);
//	}
//
//	auto ResourceLoader::_allocate_staging_memory(vk::DeviceSize image_size) -> Result<BufferRange> {
//		return std::unexpected(vk::Result::eErrorUnknown);
//	}
//
//#undef EDGE_LOGGER_SCOPE // ResourceLoader

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
		// Wait rendering finished
		fence_.wait(1000000000ull);
		fence_.reset();
	}

	auto Frame::end() -> void {
	}

	auto Frame::_construct(DescriptorSetLayout const& descriptor_layout) -> vk::Result {
		if (auto result = Semaphore::create(); !result.has_value()) {
			EDGE_SLOGE("Failed to create image available semaphore handle. Reason: {}.", vk::to_string(result.error()));
			return result.error();
			
		}
		else {
			image_available_ = std::move(result.value());
		}

		if (auto result = Semaphore::create(); !result.has_value()) {
			EDGE_SLOGE("Failed to create rendering finished semaphore handle. Reason: {}.", vk::to_string(result.error()));
			return result.error();
		}
		else {
			rendering_finished_ = std::move(result.value());
		}

		if (auto result = Fence::create(vk::FenceCreateFlagBits::eSignaled); !result.has_value()) {
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

	Renderer::~Renderer() {
		queue_->waitIdle();

		descriptor_pool_.free_descriptor_set(descriptor_set_);
	}

	auto Renderer::construct(const RendererCreateInfo& create_info) -> Result<std::unique_ptr<Renderer>> {
		auto self = std::make_unique<Renderer>();
		if (auto result = self->_construct(create_info); result != vk::Result::eSuccess) {
			return std::unexpected(result);
		}
		return self;
	}

	auto Renderer::create_shader(const std::string& shader_path) -> void {
		std::ifstream shader_file(shader_path, std::ios_base::binary);
		if (!shader_file.is_open()) {
			return;
		}

		BinaryReader reader{ shader_file };

		ShaderEffect shader_effect;
		reader.read(shader_effect);

		// TODO: validate shader effect

		mi::Vector<ShaderModule> shader_modules{};
		mi::Vector<vk::PipelineShaderStageCreateInfo> shader_stages{};

		for (auto const& stage : shader_effect.stages) {
			auto result = ShaderModule::create(stage.code);
			if (!result) {
				continue;
			}

			shader_modules.push_back(std::move(result.value()));

			vk::PipelineShaderStageCreateInfo shader_stage_create_info{};
			shader_stage_create_info.stage = stage.stage;
			shader_stage_create_info.pName = stage.entry_point_name.c_str();
			shader_stage_create_info.module = shader_modules.back().get_handle();
			shader_stages.push_back(shader_stage_create_info);
		}
		
		if (shader_effect.bind_point == vk::PipelineBindPoint::eGraphics) {
			mi::Vector<vk::VertexInputBindingDescription> vertex_input_binding_descriptions{};
			for (auto const& binding_desc : shader_effect.vertex_input_bindings) {
				vertex_input_binding_descriptions.push_back(binding_desc.to_vulkan());
			}

			mi::Vector<vk::VertexInputAttributeDescription> vertex_input_attribute_descriptions{};
			for (auto const& attribute_desc : shader_effect.vertex_input_attributes) {
				vertex_input_attribute_descriptions.push_back(attribute_desc.to_vulkan());
			}

			vk::PipelineVertexInputStateCreateInfo input_state_create_info{};
			input_state_create_info.vertexBindingDescriptionCount = static_cast<uint32_t>(vertex_input_binding_descriptions.size());
			input_state_create_info.pVertexBindingDescriptions = vertex_input_binding_descriptions.data();
			input_state_create_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertex_input_attribute_descriptions.size());
			input_state_create_info.pVertexAttributeDescriptions = vertex_input_attribute_descriptions.data();

			vk::PipelineInputAssemblyStateCreateInfo input_assembly_state_create_info{};
			input_assembly_state_create_info.topology = static_cast<vk::PrimitiveTopology>(shader_effect.pipeline_state.input_assembly_state_primitive_topology);
			input_assembly_state_create_info.primitiveRestartEnable = static_cast<vk::Bool32>(shader_effect.pipeline_state.input_assembly_state_primitive_restart_enable);

			vk::PipelineTessellationStateCreateInfo tessellation_state_create_info{};
			tessellation_state_create_info.patchControlPoints = static_cast<uint32_t>(shader_effect.pipeline_state.tessellation_state_control_points);

			auto& image = swapchain_images_[swapchain_image_index_];
			auto extent = image.get_extent();

			vk::PipelineViewportStateCreateInfo viewport_state_create_info{};
			vk::Viewport base_viewport{};
			vk::Rect2D base_scissor{};

			vk::GraphicsPipelineCreateInfo create_info{};
			create_info.stageCount = static_cast<uint32_t>(shader_stages.size());
			create_info.pStages = shader_stages.data();
			create_info.pVertexInputState = &input_state_create_info;
			create_info.pInputAssemblyState = &input_assembly_state_create_info;
			create_info.pTessellationState = &tessellation_state_create_info;

		}
		else if (shader_effect.bind_point == vk::PipelineBindPoint::eCompute) {
			vk::ComputePipelineCreateInfo create_info{};
			create_info.stage = shader_stages.front();
		}
	}

	auto Renderer::begin_frame(float delta_time) -> void {
		if (active_frame_) {
			EDGE_SLOGW("Attempting to start a new frame when the old one is not finished.");
			return;
		}

		auto swapchain_recreated = handle_surface_change();

		auto* current_frame = get_current_frame();
		current_frame->begin();

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

		active_frame_ = current_frame;

		auto const& cmdbuf = active_frame_->get_command_buffer();
		if (auto result = cmdbuf.begin(); result != vk::Result::eSuccess) {
			EDGE_SLOGE("Failed to begin command buffer. Reason: {}.", vk::to_string(result));
			active_frame_ = nullptr;
			return;
		}

		// Get previous frame time 
		uint64_t timestamps[2] = {};
		timestamp_query_.get_data(0u, &timestamps[0]);

		uint64_t elapsed_time = timestamps[1] - timestamps[0];
		if (timestamps[1] <= timestamps[0]) {
			elapsed_time = 0ull;
		}

		gpu_delta_time_ = static_cast<double>(elapsed_time) * timestamp_frequency_ / 1000000.0;

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

	auto Renderer::end_frame() -> void {
		if (!active_frame_) {
			EDGE_SLOGW("Attempting to end a frame when the new one is not started yet.");
			return;
		}

		auto const& cmdbuf = active_frame_->get_command_buffer();
		auto& image = swapchain_images_[swapchain_image_index_];
		auto& image_view = swapchain_image_views_[swapchain_image_index_];

		cmdbuf.push_barrier(ImageBarrier{
			.image = &image,
			.src_state = ResourceStateFlag::eUndefined,
			.dst_state = ResourceStateFlag::eRenderTarget,
			.subresource_range = { vk::ImageAspectFlagBits::eColor, 0u, 1u, 0u, 1u }
		});

		vk::RenderingAttachmentInfoKHR attachment{};
		attachment.imageView = *image_view;
		attachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
		attachment.loadOp = vk::AttachmentLoadOp::eClear;
		attachment.storeOp = vk::AttachmentStoreOp::eStore;

		float flash_r = std::abs(std::cos(frame_number_ / 120.f));
		float flash_g = std::abs(std::sin(frame_number_ / 120.f));
		float flash_b = std::abs(std::sinh(frame_number_ / 120.f));
		attachment.clearValue.color = vk::ClearColorValue{ flash_r, flash_g, flash_b, 1.0f };

		vk::RenderingInfoKHR rendering_info{};
		rendering_info.renderArea.extent.width = image.get_extent().width;
		rendering_info.renderArea.extent.height = image.get_extent().height;
		rendering_info.layerCount = 1u;
		rendering_info.colorAttachmentCount = 1u;
		rendering_info.pColorAttachments = &attachment;

		cmdbuf->beginRenderingKHR(&rendering_info);
		cmdbuf->endRenderingKHR();

		cmdbuf.push_barrier(ImageBarrier{
			.image = &image,
			.src_state = ResourceStateFlag::eRenderTarget,
			.dst_state = ResourceStateFlag::ePresent,
			.subresource_range = { vk::ImageAspectFlagBits::eColor, 0u, 1u, 0u, 1u }
			});

		cmdbuf->writeTimestamp2KHR(vk::PipelineStageFlagBits2::eTopOfPipe, *timestamp_query_, 1u);

		cmdbuf.end();

		mi::Vector<vk::SemaphoreSubmitInfo> wait_semaphores{};
		wait_semaphores.push_back(vk::SemaphoreSubmitInfo{ acquired_semaphore_, 0ull, vk::PipelineStageFlagBits2::eColorAttachmentOutput });

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
			EDGE_SLOGE("Failed to submit queue. Reason: {}", vk::to_string(result));
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
				EDGE_SLOGE("Failed to present images. Reason: {}", vk::to_string(result));
				return;
			}
		}

		active_frame_->end();
		active_frame_ = nullptr;
		frame_number_++;
	}

	auto Renderer::_construct(const RendererCreateInfo& create_info) -> vk::Result {
		if (auto result = get_queue(QueueType::eDirect); !result.has_value()) {
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

		vk::QueryPoolCreateInfo query_pool_create_info{};
		query_pool_create_info.queryCount = 1u;
		query_pool_create_info.queryType = vk::QueryType::eTimestamp;

		if (auto result = QueryPool::create(vk::QueryType::eTimestamp, 1u); !result.has_value()) {
			EDGE_SLOGE("Failed to create timestamp query. Reason: {}.", vk::to_string(result.error()));
			return result.error();
		}
		else {
			timestamp_query_ = std::move(result.value());
			timestamp_query_.reset(0u);
		}

		vk::PhysicalDeviceProperties adapter_properties;
		adapter_->getProperties(&adapter_properties);

		timestamp_frequency_ = adapter_properties.limits.timestampPeriod;

		// Create descriptor layout
		DescriptorSetLayoutBuilder set_layout_builder{};
		set_layout_builder.add_binding(0, vk::DescriptorType::eSampler, UINT16_MAX,
			vk::ShaderStageFlagBits::eAllGraphics | vk::ShaderStageFlagBits::eCompute,
			vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eUpdateAfterBind);
		set_layout_builder.add_binding(1, vk::DescriptorType::eSampledImage, UINT16_MAX,
			vk::ShaderStageFlagBits::eAllGraphics | vk::ShaderStageFlagBits::eCompute,
			vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eUpdateAfterBind);
		set_layout_builder.add_binding(2, vk::DescriptorType::eStorageImage, UINT16_MAX,
			vk::ShaderStageFlagBits::eAllGraphics | vk::ShaderStageFlagBits::eCompute,
			vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eUpdateAfterBind);

		if (auto result = set_layout_builder.build(vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool); !result) {
			return result.error();
		}
		else {
			descriptor_layout_ = std::move(result.value());
		}

		auto const& requested_sizes = descriptor_layout_.get_pool_sizes();
		if (auto result = DescriptorPool::create(requested_sizes, 1u, vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet | vk::DescriptorPoolCreateFlagBits::eUpdateAfterBindEXT); !result) {
			EDGE_SLOGE("Failed to create frame descriptor pool handle. Reason: {}.", vk::to_string(result.error()));
			return result.error();
		}
		else {
			descriptor_pool_ = std::move(result.value());
		}

		if (auto result = descriptor_pool_.allocate_descriptor_set(descriptor_layout_); !result) {
			EDGE_SLOGE("Failed to create frame descriptor set handle. Reason: {}.", vk::to_string(result.error()));
			return result.error();
		}
		else {
			descriptor_set_ = std::move(result.value());
		}

		PipelineLayoutBuilder pipeline_layout_builder{};
		// TODO: add ability to set specific set point
		pipeline_layout_builder.add_set_layout(descriptor_layout_);
		pipeline_layout_builder.add_constant_range(vk::ShaderStageFlagBits::eAllGraphics | vk::ShaderStageFlagBits::eCompute, 0u, adapter_properties.limits.maxPushConstantsSize);

		if (auto result = pipeline_layout_builder.build(); !result) {
			return result.error();
		}
		else {
			pipeline_layout_ = std::move(result.value());
		}

		push_constant_buffer_.resize(adapter_properties.limits.maxPushConstantsSize);

		if (auto result = ShaderLibrary::construct(pipeline_layout_, u8"/shader_cache.cache", u8"/assets/shaders"); !result) {
			EDGE_SLOGE("Failed to create shader library. Reason: {}.", vk::to_string(result.error()));
			return result.error();
		}
		else {
			shader_library_ = std::move(result.value());
		}

		if (auto result = create_swapchain({
			.format = {create_info.preferred_format, create_info.preferred_color_space},
			.extent = create_info.extent,
			.vsync = create_info.enable_vsync,
			.hdr = create_info.enable_hdr
			}); result != vk::Result::eSuccess) {
			return result;
		}

		for (int32_t i = 0; i < k_frame_overlap_; ++i) {
			auto command_list_result = command_pool_.allocate_command_buffer();
			if (!command_list_result) {
				EDGE_SLOGE("Failed to allocate command list for frame at index {}. Reason: {}.", i, vk::to_string(command_list_result.error()));
				return command_list_result.error();
			}

			if (auto frame_result = Frame::construct(std::move(command_list_result.value()), descriptor_layout_); !frame_result.has_value()) {
				EDGE_SLOGE("Failed to create frame at index {}. Reason: {}.", i, vk::to_string(frame_result.error()));
				return frame_result.error();
			}
			else {
				frames_.push_back(std::move(frame_result.value()));
			}
		}

		//create_shader("assets/shaders/imgui.shfx");

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
			EDGE_SLOGE("Failed to recreate swapchain with reason: {}", vk::to_string(result.error()));
			return result.error();
		}
		else {
			swapchain_.reset();
			swapchain_ = std::move(result.value());

			if (auto result = swapchain_.get_images(); !result.has_value()) {
				EDGE_SLOGE("Failed to request swapchain images. Reason: {}.", vk::to_string(result.error()));
				return result.error();
			}
			else {
				swapchain_images_ = std::move(result.value());
			}

			swapchain_image_views_.clear();

			for (int32_t i = 0; i < static_cast<int32_t>(swapchain_images_.size()); ++i) {
				vk::ImageSubresourceRange subresource_range{};
				subresource_range.aspectMask = vk::ImageAspectFlagBits::eColor;
				subresource_range.baseArrayLayer = 0u;
				subresource_range.layerCount = 1u;
				subresource_range.baseMipLevel = 0u;
				subresource_range.levelCount = 1u;

				if (auto result = swapchain_images_[i].create_view(subresource_range, vk::ImageViewType::e2D); !result.has_value()) {
					return result.error();
				}
				else {
					swapchain_image_views_.push_back(std::move(result.value()));
				}
			}
		}

		return vk::Result::eSuccess;
	}

#undef EDGE_LOGGER_SCOPE // Renderer

}