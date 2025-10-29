#pragma once

#include "gfx_resource_uploader.h"

namespace edge::gfx {
	class ResourceUpdater : public NonCopyable {
	public:
		ResourceUpdater() = default;
		~ResourceUpdater();

		ResourceUpdater(ResourceUpdater&& other) noexcept;
		auto operator=(ResourceUpdater&& other) noexcept -> ResourceUpdater&;

		static auto create(Queue const& queue, vk::DeviceSize arena_size, uint32_t uploader_count = 1u) -> Result<ResourceUpdater>;

		auto acquire_resource_set() -> ResourceSet&;
		auto get_or_allocate_staging_memory(ResourceSet& resource_set, vk::DeviceSize required_memory, vk::DeviceSize required_alignment) -> Result<BufferRange>;

		auto flush(Span<vk::SemaphoreSubmitInfoKHR> wait_semaphores) -> vk::SemaphoreSubmitInfoKHR;
	private:
		auto _construct(vk::DeviceSize arena_size, uint32_t uploader_count) -> vk::Result;
		
		auto begin_commands(ResourceSet& resource_set) -> void;
		auto end_commands(ResourceSet& resource_set) -> void;

		Queue const* queue_{ nullptr };
		CommandPool command_pool_{};

		mi::Vector<ResourceSet> resource_sets_{};
		uint32_t current_resource_set_{ 0u };

		vk::SemaphoreSubmitInfoKHR previously_signalled_semaphore_{};
	};
}