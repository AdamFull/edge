#include "gfx_resource_updater.h"

#define EDGE_LOGGER_SCOPE "gfx::ResourceUpdater"

namespace edge::gfx {
	ResourceUpdater::~ResourceUpdater() {
		if (queue_) {
#ifdef RESOURCE_UPDATER_USE_INDIVIDUAL_QUEUE
			auto wait_result = queue_->waitIdle();
#else
			auto wait_result = (*queue_)->waitIdle();
#endif
			GFX_ASSERT_MSG(wait_result == vk::Result::eSuccess, "Failed to wait queue submission finished.");
		}
	}

	ResourceUpdater::ResourceUpdater(ResourceUpdater&& other) noexcept {
		queue_ = std::move(other.queue_);
		command_pool_ = std::move(other.command_pool_);
		resource_sets_ = std::exchange(other.resource_sets_, {});
		current_resource_set_ = std::move(other.current_resource_set_);
	}
	
	auto ResourceUpdater::operator=(ResourceUpdater&& other) noexcept -> ResourceUpdater& {
		if (this != &other) {
			queue_ = std::move(other.queue_);
			command_pool_ = std::move(other.command_pool_);
			resource_sets_ = std::exchange(other.resource_sets_, {});
			current_resource_set_ = std::move(other.current_resource_set_);
		}
		return *this;
	}

	auto ResourceUpdater::create(Queue const& queue, vk::DeviceSize arena_size, uint32_t uploader_count) -> Result<ResourceUpdater> {
		ResourceUpdater self{};
#ifndef RESOURCE_UPDATER_USE_INDIVIDUAL_QUEUE
		self.queue_ = &queue;
#endif
		if (auto result = self._construct(arena_size, uploader_count); result != vk::Result::eSuccess) {
			return std::unexpected(result);
		}
		return self;
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

#ifdef RESOURCE_UPDATER_USE_INDIVIDUAL_QUEUE
		auto submit_result = queue_->submit2KHR(1u, &submit_info, VK_NULL_HANDLE);
#else
		auto submit_result = (*queue_)->submit2KHR(1u, &submit_info, VK_NULL_HANDLE);
#endif
		GFX_ASSERT_MSG(submit_result == vk::Result::eSuccess, "Failed to submit uploader queue.");

		if (resource_set.first_submission) {
			resource_set.first_submission = false;
		}

		previously_signalled_semaphore_ = signal_info;

		current_resource_set_ = (current_resource_set_ + 1u) % static_cast<uint32_t>(resource_sets_.size());

		return signal_info;
	}

	auto ResourceUpdater::_construct(vk::DeviceSize arena_size, uint32_t uploader_count) -> vk::Result {
#ifdef RESOURCE_UPDATER_USE_INDIVIDUAL_QUEUE
		auto queue_result = device_.get_queue({
				.required_caps = QueuePresets::kGraphics,
				.strategy = QueueSelectionStrategy::ePreferDedicated
			});
		if (!queue_result) {
			return queue_result.error();
		}
		queue_ = std::move(queue_result.value());
#endif
		
#ifdef RESOURCE_UPDATER_USE_INDIVIDUAL_QUEUE
		auto command_pool_result = queue_.create_command_pool();
#else
		auto command_pool_result = queue_->create_command_pool();
#endif
		if (!command_pool_result) {
			return command_pool_result.error();
		}
		command_pool_ = std::move(command_pool_result.value());

		BufferCreateInfo buffer_create_info{};
		buffer_create_info.flags = BufferFlag::eStaging;
		buffer_create_info.size = std::max(4096ull, arena_size);
		buffer_create_info.count = 1u;
		buffer_create_info.minimal_alignment = 16u;

		resource_sets_.resize(uploader_count);
		for (auto& set : resource_sets_) {
			auto buffer_result = Buffer::create(buffer_create_info);
			if (buffer_result) {
				set.arena = std::move(buffer_result.value());
			}
			GFX_ASSERT_MSG(buffer_result.has_value(), "Failed to create staging memory.");

			set.temporary_buffers.reserve(128);

			auto semaphore_result = Semaphore::create(vk::SemaphoreType::eTimeline);
			if (!semaphore_result) {
				GFX_ASSERT_MSG(false, "Failed to create resource set's semaphore, but it's required.");
				return semaphore_result.error();
			}
			set.semaphore = std::move(semaphore_result.value());

			auto command_buffer_result = command_pool_.allocate_command_buffer();
			if (!command_buffer_result) {
				GFX_ASSERT_MSG(false, "Failed to create resource set command buffer, but it's required.");
				return command_buffer_result.error();
			}
			set.command_buffer = std::move(command_buffer_result.value());
		}

		return vk::Result::eSuccess;
	}

	auto ResourceUpdater::get_or_allocate_staging_memory(ResourceSet& resource_set, vk::DeviceSize required_memory, vk::DeviceSize required_alignment) -> Result<BufferRange> {
		auto aligned_requested_size = aligned_size(required_memory, required_alignment);
		auto available_size = resource_set.arena.get_size() - resource_set.offset;

		if (resource_set.arena.get_size() < aligned_requested_size || available_size < aligned_requested_size) {
			BufferCreateInfo create_info{};
			create_info.size = aligned_requested_size;
			create_info.count = 1u;
			create_info.minimal_alignment = required_alignment;
			create_info.flags = kStagingBuffer;

			auto buffer_result = Buffer::create(create_info);
			if (!buffer_result) {
				return std::unexpected(buffer_result.error());
			}

			auto& new_buffer = resource_set.temporary_buffers.emplace_back(std::move(buffer_result.value()));
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