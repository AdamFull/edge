#pragma once

#include "gfx_resource_uploader.h"

#define RESOURCE_UPDATER_USE_INDIVIDUAL_QUEUE

namespace edge::gfx {
	class ResourceUpdater;

	class BufferUpdater : public NonCopyable {
	public:
		BufferUpdater() = default;
		BufferUpdater(ResourceSet& resource_set, Buffer& dst_buffer, ResourceStateFlags initial_state, ResourceStateFlags final_state, BufferRange&& range);

		BufferUpdater(BufferUpdater&& other) noexcept;
		auto operator=(BufferUpdater&& other) noexcept -> BufferUpdater&;

		auto write(Span<const uint8_t> data, vk::DeviceSize dst_offset = 0ull, vk::DeviceSize size = 0ull) -> vk::Result;
		auto submit() -> void;
	private:
		ResourceSet* resource_set_{ nullptr };
		Buffer* dst_buffer_{ nullptr };
		ResourceStateFlags initial_state_{};
		ResourceStateFlags final_state_{};
		BufferRange staging_range_{};

		vk::DeviceSize staging_offset_{ 0ull };
		mi::Vector<vk::BufferCopy2KHR> copy_regions_{};
		bool submitted_{ false };
	};

	struct ImageSubresourceData {
		Span<const uint8_t> data{};
		uint32_t mip_level{ 0u };
		uint32_t array_layer{ 0u };
		vk::Offset3D offset{ 0, 0, 0 };
		vk::Extent3D extent{ 0u, 0u, 1u };
	};

	class ImageUpdater : public NonCopyable {
	public:
		ImageUpdater() = default;
		ImageUpdater(ResourceSet& resource_set, Image& dst_image, ResourceStateFlags initial_state, ResourceStateFlags final_state, BufferRange&& range);

		ImageUpdater(ImageUpdater&& other) noexcept;
		auto operator=(ImageUpdater&& other) noexcept -> ImageUpdater&;

		auto write(ImageSubresourceData const& subresource_data) -> vk::Result;
		auto submit() -> void;
	private:
		ResourceSet* resource_set_{ nullptr };
		Image* dst_image_{ nullptr };
		ResourceStateFlags initial_state_{};
		ResourceStateFlags final_state_{};
		BufferRange staging_range_{};

		vk::DeviceSize staging_offset_{ 0ull };
		mi::Vector<vk::BufferImageCopy2KHR> copy_regions_{};
		bool submitted_{ false };
	};

	class ResourceUpdater : public NonCopyable {
	public:
		ResourceUpdater() = default;
		~ResourceUpdater();

		ResourceUpdater(ResourceUpdater&& other) noexcept;
		auto operator=(ResourceUpdater&& other) noexcept -> ResourceUpdater&;

		static auto create(Queue const& queue, vk::DeviceSize arena_size, uint32_t uploader_count = 1u) -> ResourceUpdater;

		auto update_buffer(Buffer& buffer, ResourceStateFlags current_state, ResourceStateFlags final_state, vk::DeviceSize required_size = 0ull) -> BufferUpdater;
		auto update_image(Image& image, ResourceStateFlags current_state, ResourceStateFlags final_state, vk::DeviceSize required_size = 0ull) -> ImageUpdater;

		auto acquire_resource_set() -> ResourceSet&;
		auto get_or_allocate_staging_memory(ResourceSet& resource_set, vk::DeviceSize required_memory, vk::DeviceSize required_alignment) -> BufferRange;

		auto flush(Span<vk::SemaphoreSubmitInfoKHR> wait_semaphores) -> vk::SemaphoreSubmitInfoKHR;
	private:
		auto _construct(vk::DeviceSize arena_size, uint32_t uploader_count) -> void;
		
		auto begin_commands(ResourceSet& resource_set) -> void;
		auto end_commands(ResourceSet& resource_set) -> void;

#ifdef RESOURCE_UPDATER_USE_INDIVIDUAL_QUEUE
		Queue queue_{};
#else
		Queue const* queue_{ nullptr };
#endif
		CommandPool command_pool_{};

		mi::Vector<ResourceSet> resource_sets_{};
		uint32_t current_resource_set_{ 0u };

		vk::SemaphoreSubmitInfoKHR previously_signalled_semaphore_{};
	};
}