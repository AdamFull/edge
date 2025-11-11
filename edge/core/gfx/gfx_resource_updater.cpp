#include "gfx_resource_updater.h"

#define EDGE_LOGGER_SCOPE "gfx::ResourceUpdater"

namespace edge::gfx {
	BufferUpdater::BufferUpdater(ResourceSet& resource_set, Buffer& dst_buffer, ResourceStateFlags initial_state, ResourceStateFlags final_state, BufferRange&& range)
		: resource_set_{ &resource_set }
		, dst_buffer_{ &dst_buffer }
		, initial_state_{ initial_state }
		, final_state_{ final_state }
		, staging_range_{ std::move(range) } {

	}

	BufferUpdater::BufferUpdater(BufferUpdater&& other) noexcept {
		resource_set_ = std::exchange(other.resource_set_, nullptr);
		dst_buffer_ = std::exchange(other.dst_buffer_, nullptr);
		initial_state_ = std::move(other.initial_state_);
		final_state_ = std::move(other.final_state_);
		staging_range_ = std::move(other.staging_range_);
		staging_offset_ = std::exchange(other.staging_offset_, 0ull);
		copy_regions_ = std::move(other.copy_regions_);
		submitted_ = std::move(other.submitted_);
	}

	auto BufferUpdater::operator=(BufferUpdater&& other) noexcept -> BufferUpdater& {
		if (this != &other) {
			resource_set_ = std::exchange(other.resource_set_, nullptr);
			dst_buffer_ = std::exchange(other.dst_buffer_, nullptr);
			initial_state_ = std::move(other.initial_state_);
			final_state_ = std::move(other.final_state_);
			staging_range_ = std::move(other.staging_range_);
			staging_offset_ = std::exchange(other.staging_offset_, 0ull);
			copy_regions_ = std::move(other.copy_regions_);
			submitted_ = std::move(other.submitted_);
		}
		return *this;
	}

	auto BufferUpdater::write(Span<const uint8_t> data, vk::DeviceSize dst_offset, vk::DeviceSize size) -> vk::Result {
		GFX_ASSERT_MSG(!submitted_, "Cannot write after submit");
		GFX_ASSERT_MSG(staging_range_.get_buffer() != nullptr, "Invalid staging buffer");

		auto copy_size = size > 0 ? size : data.size();
		GFX_ASSERT_MSG(copy_size <= data.size(), "Copy size exceeds data size");

		auto available_size = staging_range_.get_size() - staging_offset_;
		if (copy_size > available_size) {
			EDGE_LOGE("Insufficient staging memory: need {}, have {}", copy_size, available_size);
			return vk::Result::eErrorOutOfDeviceMemory;
		}

		staging_range_.write(data.data(), copy_size, staging_offset_);
		copy_regions_.push_back(staging_range_.make_buffer_region_update(staging_offset_, dst_offset, copy_size));
		staging_offset_ += copy_size;

		return vk::Result::eSuccess;
	}

	auto BufferUpdater::submit() -> void {
		GFX_ASSERT_MSG(!submitted_, "Already submitted");
		GFX_ASSERT_MSG(resource_set_ != nullptr, "Invalid resource set");
		GFX_ASSERT_MSG(!copy_regions_.empty(), "No data to copy");

		auto& cmd = resource_set_->command_buffer;
		submitted_ = true;

		auto src_state = util::get_resource_state(initial_state_);
		auto dst_state = util::get_resource_state(EDGE_GFX_RESOURCE_STATE_COPY_DST);

		vk::BufferMemoryBarrier2KHR pre_barrier{};
		pre_barrier.srcStageMask = src_state.stage_flags;
		pre_barrier.srcAccessMask = src_state.access_flags;
		pre_barrier.dstStageMask = dst_state.stage_flags;
		pre_barrier.dstAccessMask = dst_state.access_flags;
		pre_barrier.buffer = dst_buffer_->get_handle();
		pre_barrier.offset = 0ull;
		pre_barrier.size = VK_WHOLE_SIZE;

		vk::DependencyInfoKHR pre_dependency{};
		pre_dependency.bufferMemoryBarrierCount = 1u;
		pre_dependency.pBufferMemoryBarriers = &pre_barrier;

		cmd->pipelineBarrier2KHR(&pre_dependency);

		vk::CopyBufferInfo2KHR copy_info{};
		copy_info.srcBuffer = staging_range_.get_buffer();
		copy_info.dstBuffer = dst_buffer_->get_handle();
		copy_info.regionCount = static_cast<uint32_t>(copy_regions_.size());
		copy_info.pRegions = copy_regions_.data();

		cmd->copyBuffer2KHR(&copy_info);

		src_state = util::get_resource_state(EDGE_GFX_RESOURCE_STATE_COPY_DST);
		dst_state = util::get_resource_state(final_state_);

		vk::BufferMemoryBarrier2KHR post_barrier{};
		post_barrier.srcStageMask = src_state.stage_flags;
		post_barrier.srcAccessMask = src_state.access_flags;
		post_barrier.dstStageMask = dst_state.stage_flags;
		post_barrier.dstAccessMask = dst_state.access_flags;
		post_barrier.buffer = dst_buffer_->get_handle();
		post_barrier.offset = 0ull;
		post_barrier.size = VK_WHOLE_SIZE;

		vk::DependencyInfoKHR post_dependency{};
		post_dependency.bufferMemoryBarrierCount = 1u;
		post_dependency.pBufferMemoryBarriers = &post_barrier;

		cmd->pipelineBarrier2KHR(&post_dependency);
	}


	ImageUpdater::ImageUpdater(ResourceSet& resource_set, Image& dst_image, ResourceStateFlags initial_state, ResourceStateFlags final_state, BufferRange&& range) 
		: resource_set_{ &resource_set }
		, dst_image_{ &dst_image }
		, initial_state_{ initial_state }
		, final_state_{ final_state }
		, staging_range_{ std::move(range) } {
	}

	ImageUpdater::ImageUpdater(ImageUpdater&& other) noexcept {
		resource_set_ = std::exchange(other.resource_set_, nullptr);
		dst_image_ = std::exchange(other.dst_image_, nullptr);
		initial_state_ = std::move(other.initial_state_);
		final_state_ = std::move(other.final_state_);
		staging_range_ = std::move(other.staging_range_);
		staging_offset_ = std::exchange(other.staging_offset_, 0ull);
		copy_regions_ = std::move(other.copy_regions_);
		submitted_ = std::move(other.submitted_);
	}

	auto ImageUpdater::operator=(ImageUpdater&& other) noexcept -> ImageUpdater& {
		if (this != &other) {
			resource_set_ = std::exchange(other.resource_set_, nullptr);
			dst_image_ = std::exchange(other.dst_image_, nullptr);
			initial_state_ = std::move(other.initial_state_);
			final_state_ = std::move(other.final_state_);
			staging_range_ = std::move(other.staging_range_);
			staging_offset_ = std::exchange(other.staging_offset_, 0ull);
			copy_regions_ = std::move(other.copy_regions_);
			submitted_ = std::move(other.submitted_);
		}
		return *this;
	}

	auto ImageUpdater::write(ImageSubresourceData const& subresource_data) -> vk::Result {
		GFX_ASSERT_MSG(!submitted_, "Cannot write after submit");
		GFX_ASSERT_MSG(staging_range_.get_buffer() != nullptr, "Invalid staging buffer");
		GFX_ASSERT_MSG(!subresource_data.data.empty(), "Data cannot be empty");

		auto data_size = subresource_data.data.size();
		auto available_size = staging_range_.get_size() - staging_offset_;
		if (data_size > available_size) {
			EDGE_LOGE("Insufficient staging memory: need {}, have {}", data_size, available_size);
			return vk::Result::eErrorOutOfDeviceMemory;
		}

		staging_range_.write(subresource_data.data.data(), data_size, staging_offset_);

		auto extent = subresource_data.extent;
		if (extent.width == 0 || extent.height == 0) {
			auto image_extent = dst_image_->get_extent();
			auto mip = subresource_data.mip_level;
			extent.width = std::max(1u, image_extent.width >> mip);
			extent.height = std::max(1u, image_extent.height >> mip);
			extent.depth = std::max(1u, image_extent.depth >> mip);
		}

		vk::ImageSubresourceLayers subresource_layers{};
		subresource_layers.aspectMask = vk::ImageAspectFlagBits::eColor;
		subresource_layers.mipLevel = subresource_data.mip_level;
		subresource_layers.baseArrayLayer = subresource_data.array_layer;
		subresource_layers.layerCount = 1u;

		copy_regions_.push_back(staging_range_.make_image_region_update(staging_offset_, subresource_layers, subresource_data.offset, extent));

		staging_offset_ += data_size;

		return vk::Result::eSuccess;
	}

	auto ImageUpdater::submit() -> void {
		GFX_ASSERT_MSG(!submitted_, "Already submitted");
		GFX_ASSERT_MSG(resource_set_ != nullptr, "Invalid resource set");
		GFX_ASSERT_MSG(!copy_regions_.empty(), "No data to copy");

		auto& cmd = resource_set_->command_buffer;
		submitted_ = true;

		auto src_state = util::get_resource_state(initial_state_);
		auto dst_state = util::get_resource_state(EDGE_GFX_RESOURCE_STATE_COPY_DST);

		vk::ImageMemoryBarrier2KHR pre_barrier{};
		pre_barrier.srcStageMask = src_state.stage_flags;
		pre_barrier.srcAccessMask = src_state.access_flags;
		pre_barrier.dstStageMask = dst_state.stage_flags;
		pre_barrier.dstAccessMask = dst_state.access_flags;
		pre_barrier.oldLayout = util::get_image_layout(initial_state_);
		pre_barrier.newLayout = util::get_image_layout(EDGE_GFX_RESOURCE_STATE_COPY_DST);
		pre_barrier.image = dst_image_->get_handle();
		pre_barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		pre_barrier.subresourceRange.baseMipLevel = 0u;
		pre_barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
		pre_barrier.subresourceRange.baseArrayLayer = 0u;
		pre_barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

		vk::DependencyInfoKHR pre_dependency{};
		pre_dependency.imageMemoryBarrierCount = 1u;
		pre_dependency.pImageMemoryBarriers = &pre_barrier;

		cmd->pipelineBarrier2KHR(&pre_dependency);

		vk::CopyBufferToImageInfo2KHR copy_info{};
		copy_info.srcBuffer = staging_range_.get_buffer();
		copy_info.dstImage = dst_image_->get_handle();
		copy_info.dstImageLayout = pre_barrier.newLayout;
		copy_info.regionCount = static_cast<uint32_t>(copy_regions_.size());
		copy_info.pRegions = copy_regions_.data();

		cmd->copyBufferToImage2KHR(&copy_info);

		src_state = util::get_resource_state(EDGE_GFX_RESOURCE_STATE_COPY_DST);
		dst_state = util::get_resource_state(final_state_);

		vk::ImageMemoryBarrier2KHR post_barrier{};
		post_barrier.srcStageMask = src_state.stage_flags;
		post_barrier.srcAccessMask = src_state.access_flags;
		post_barrier.dstStageMask = dst_state.stage_flags;
		post_barrier.dstAccessMask = dst_state.access_flags;
		post_barrier.oldLayout = util::get_image_layout(EDGE_GFX_RESOURCE_STATE_COPY_DST);
		post_barrier.newLayout = util::get_image_layout(final_state_);
		post_barrier.image = dst_image_->get_handle();
		post_barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		post_barrier.subresourceRange.baseMipLevel = 0u;
		post_barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
		post_barrier.subresourceRange.baseArrayLayer = 0u;
		post_barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

		vk::DependencyInfoKHR post_dependency{};
		post_dependency.imageMemoryBarrierCount = 1u;
		post_dependency.pImageMemoryBarriers = &post_barrier;

		cmd->pipelineBarrier2KHR(&post_dependency);
	}


	ResourceUpdater::~ResourceUpdater() {
		if (queue_) {
			auto wait_result = (*queue_)->waitIdle();
			GFX_ASSERT_MSG(wait_result == vk::Result::eSuccess, "Failed to wait queue submission finished.");
		}
	}

	ResourceUpdater::ResourceUpdater(ResourceUpdater&& other) noexcept {
		queue_ = std::move(other.queue_);
		owned_queue_ = std::move(other.owned_queue_);
		command_pool_ = std::move(other.command_pool_);
		resource_sets_ = std::exchange(other.resource_sets_, {});
		current_resource_set_ = std::move(other.current_resource_set_);
		previously_signalled_semaphore_ = std::move(other.previously_signalled_semaphore_);
	}
	
	auto ResourceUpdater::operator=(ResourceUpdater&& other) noexcept -> ResourceUpdater& {
		if (this != &other) {
			queue_ = std::move(other.queue_);
			owned_queue_ = std::move(other.owned_queue_);
			command_pool_ = std::move(other.command_pool_);
			resource_sets_ = std::exchange(other.resource_sets_, {});
			current_resource_set_ = std::move(other.current_resource_set_);
			previously_signalled_semaphore_ = std::move(other.previously_signalled_semaphore_);
		}
		return *this;
	}

	auto ResourceUpdater::create(Queue const& queue, vk::DeviceSize arena_size, uint32_t uploader_count) -> ResourceUpdater {
		ResourceUpdater self{};
		self.queue_ = &queue;
		self._construct(arena_size, uploader_count);
		return self;
	}

	auto ResourceUpdater::update_buffer(Buffer& buffer, ResourceStateFlags current_state, ResourceStateFlags final_state, vk::DeviceSize required_size) -> BufferUpdater {
		auto staging_size = required_size > 0 ? required_size : buffer.get_size();
		auto& resource_set = acquire_resource_set();
		return BufferUpdater(resource_set, buffer, current_state, final_state, get_or_allocate_staging_memory(resource_set, staging_size, 16ull));
	}

	auto ResourceUpdater::update_image(Image& image, ResourceStateFlags current_state, ResourceStateFlags final_state, vk::DeviceSize required_size) -> ImageUpdater {
		
		vk::DeviceSize staging_size{};
		if (required_size > 0) {
			staging_size = required_size;
		}
		else {
			auto extent = image.get_extent();
			auto layer_count = image.get_face_count() * image.get_layer_count();
			auto level_count = image.get_level_count();
			auto format = image.get_format();

			staging_size = util::calculate_image_size(format, extent.width, extent.height, extent.depth, level_count, layer_count);
		}

		auto& resource_set = acquire_resource_set();
		return ImageUpdater(resource_set, image, current_state, final_state, get_or_allocate_staging_memory(resource_set, staging_size, 16ull));
	}

	auto ResourceUpdater::flush(Span<vk::SemaphoreSubmitInfoKHR> wait_semaphores) -> vk::SemaphoreSubmitInfoKHR {
		auto& resource_set = resource_sets_[current_resource_set_];
		if (!resource_set.recording) {
			return {};
		}

		end_commands(resource_set);

		uint64_t wait_value = resource_set.counter.fetch_add(1, std::memory_order_relaxed);
		uint64_t signal_value = wait_value + 1;

		vk::SemaphoreSubmitInfoKHR signal_info{};
		signal_info.semaphore = *resource_set.semaphore;
		signal_info.value = signal_value;
		signal_info.stageMask = vk::PipelineStageFlagBits2::eAllCommands;
		signal_info.deviceIndex = 0;

		mi::Vector<vk::SemaphoreSubmitInfoKHR> semaphores_to_wait{};
		if (!resource_set.first_submission) {
			semaphores_to_wait.push_back(previously_signalled_semaphore_);
		}
		for (auto& semaphore : wait_semaphores) {
			if (!semaphore.semaphore) {
				continue;
			}
			semaphores_to_wait.push_back(semaphore);
		}

		vk::CommandBufferSubmitInfoKHR command_buffer_info{};
		command_buffer_info.commandBuffer = *resource_set.command_buffer;

		vk::SubmitInfo2KHR submit_info{};
		submit_info.waitSemaphoreInfoCount = static_cast<uint32_t>(semaphores_to_wait.size());
		submit_info.pWaitSemaphoreInfos = semaphores_to_wait.data();
		submit_info.signalSemaphoreInfoCount = 1u;
		submit_info.pSignalSemaphoreInfos = &signal_info;
		submit_info.commandBufferInfoCount = 1u;
		submit_info.pCommandBufferInfos = &command_buffer_info;
		auto submit_result = (*queue_)->submit2KHR(1u, &submit_info, VK_NULL_HANDLE);
		GFX_ASSERT_MSG(submit_result == vk::Result::eSuccess, "Failed to submit uploader queue.");

		if (resource_set.first_submission) {
			resource_set.first_submission = false;
		}

		previously_signalled_semaphore_ = signal_info;

		current_resource_set_ = (current_resource_set_ + 1u) % static_cast<uint32_t>(resource_sets_.size());

		return signal_info;
	}

	auto ResourceUpdater::_construct(vk::DeviceSize arena_size, uint32_t uploader_count) -> void {
		auto queue_result = device_.get_queue({
				.required_caps = QueuePresets::kGraphics,
				.strategy = EDGE_GFX_QUEUE_SELECTION_STRATEGY_PREFER_DEDICATED
			});
		if (queue_result) {
			EDGE_SLOGD("Found dedicated graphics queue for resource uploader.");
			owned_queue_ = std::make_unique<Queue>(std::move(queue_result.value()));
			queue_ = owned_queue_.get();
		}
		
		command_pool_ = queue_->create_command_pool();

		BufferCreateInfo buffer_create_info{};
		buffer_create_info.flags = EDGE_GFX_BUFFER_FLAG_STAGING;
		buffer_create_info.size = std::max(vk::DeviceSize{4096ull}, arena_size);
		buffer_create_info.count = 1u;
		buffer_create_info.minimal_alignment = 16u;

		resource_sets_.resize(uploader_count);
		for (auto& set : resource_sets_) {
			set.arena = Buffer::create(buffer_create_info);
			set.temporary_buffers.reserve(128);
			set.semaphore = Semaphore::create(vk::SemaphoreType::eTimeline);
			set.command_buffer = command_pool_.allocate_command_buffer();
		}
	}

	auto ResourceUpdater::get_or_allocate_staging_memory(ResourceSet& resource_set, vk::DeviceSize required_memory, vk::DeviceSize required_alignment) -> BufferRange {
		auto aligned_requested_size = aligned_size(required_memory, required_alignment);
		auto available_size = resource_set.arena.get_size() - resource_set.offset;

		if (resource_set.arena.get_size() < aligned_requested_size || available_size < aligned_requested_size) {
			BufferCreateInfo create_info{};
			create_info.size = aligned_requested_size;
			create_info.count = 1u;
			create_info.minimal_alignment = required_alignment;
			create_info.flags = kStagingBuffer;
			auto& new_buffer = resource_set.temporary_buffers.emplace_back(Buffer::create(create_info));
			GFX_ASSERT_MSG(resource_set.temporary_buffers.size() < 128, "Warning, all temporary buffer links now is invalid.");
			return BufferRange::create(&new_buffer, 0ull, new_buffer.get_size());
		}

		auto current_offset = resource_set.offset;
		resource_set.offset += aligned_requested_size;
		return BufferRange::create(&resource_set.arena, current_offset, aligned_requested_size);
	}

	auto ResourceUpdater::acquire_resource_set() -> ResourceSet& {
		auto& resource_set = resource_sets_[current_resource_set_];
		if (!resource_set.recording) {
			begin_commands(resource_set);
		}
		return resource_set;
	}

	auto ResourceUpdater::begin_commands(ResourceSet& resource_set) -> void {
		GFX_ASSERT_MSG(!resource_set.recording, "Commands is already recording.");

		resource_set.offset = 0ull;
		resource_set.temporary_buffers.clear();

		auto begin_result = resource_set.command_buffer.begin();
		GFX_ASSERT_MSG(begin_result == vk::Result::eSuccess, "Failed to begin commands.");
		resource_set.command_buffer.begin_marker("Updater", 0xFFFFFFFF);

		resource_set.recording = true;
	}

	auto ResourceUpdater::end_commands(ResourceSet& resource_set) -> void {
		GFX_ASSERT_MSG(resource_set.recording, "Commands was not started to recording.");

		resource_set.command_buffer.end_marker();

		auto end_result = resource_set.command_buffer.end();
		GFX_ASSERT_MSG(end_result == vk::Result::eSuccess, "Failed to end commands.");

		resource_set.recording = false;
	}
}

#undef EDGE_LOGGER_SCOPE