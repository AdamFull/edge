#pragma once 

#include <variant>

#include "gfx_context.h"

namespace edge::gfx {
	enum class UploadingStatus {
		eDone,
		eNotFound,
		eNotSupported,
		eFailed
	};

	enum class UploadType {
		eImage,
		eBuffer
	};

	struct UploadTask {
		UploadType type;
		uint64_t sync_token;
		mi::U8String path;
	};

	struct UploadResult {
		uint64_t sync_token;
		UploadType type;
		UploadingStatus status;
		std::variant<Buffer, Image> data;
	};

	struct ResourceSet {
		Buffer arena{};
		uint64_t offset{ 0ull };

		mi::Vector<Buffer> temporary_buffers{};

		Semaphore semaphore{};
		std::atomic_uint64_t counter{ 0ull };
		bool first_submission{ true };

		ResourceSet() = default;
		ResourceSet(const ResourceSet&) = delete;
		ResourceSet& operator=(const ResourceSet&) = delete;

		ResourceSet(ResourceSet&& other) noexcept {
			arena = std::move(other.arena);
			offset = std::exchange(other.offset, {});
			temporary_buffers = std::exchange(other.temporary_buffers, {});
			semaphore = std::move(other.semaphore);

			counter.store(other.counter.load(std::memory_order_relaxed), std::memory_order_relaxed);
			first_submission = true;
		}

		auto operator=(ResourceSet&& other) noexcept -> ResourceSet& {
			if (this != &other) {
				arena = std::move(other.arena);
				offset = std::exchange(other.offset, {});
				temporary_buffers = std::exchange(other.temporary_buffers, {});
				semaphore = std::move(other.semaphore);

				counter.store(other.counter.load(std::memory_order_relaxed), std::memory_order_relaxed);
				first_submission = true;
			}
			return *this;
		}

		CommandBuffer command_buffer{};
		bool recording{ false };
	};

	class ResourceUploader : public NonCopyable {
	public:
		ResourceUploader() = default;
		~ResourceUploader();

		ResourceUploader(ResourceUploader&& other) noexcept;
		auto operator=(ResourceUploader&& other) noexcept -> ResourceUploader&;

		static auto create(vk::DeviceSize arena_size, uint32_t uploader_count = 1u) -> Result<ResourceUploader>;

		auto start_streamer() -> void;
		auto stop_streamer() -> void;
		[[nodiscard]] auto load_image(std::u8string_view path) -> uint64_t;

		auto is_task_done(uint64_t task_id) const -> bool;
		auto wait_for_task(uint64_t task_id) -> void;
		auto get_last_complete_task_id() const -> uint64_t;
		auto get_task_result(uint64_t task_id) -> std::optional<UploadResult>;
		auto is_all_work_complete() const -> bool;
		auto wait_all_work_complete() -> void;
	private:
		auto _construct(vk::DeviceSize arena_size, uint32_t uploader_count) -> vk::Result;

		auto worker_loop() -> void;
		auto process_task(ResourceSet& resource_set, UploadTask const& task) -> UploadResult;
		auto process_image(ResourceSet& resource_set, UploadTask const& task) -> UploadResult;
		auto _load_image_stb(ResourceSet& resource_set, UploadTask const& task, Span<uint8_t> image_raw_data) -> UploadResult;
		auto _load_image_ktx(ResourceSet& resource_set, UploadTask const& task, Span<uint8_t> image_raw_data) -> UploadResult;
		auto get_or_allocate_staging_memory(ResourceSet& resource_set, vk::DeviceSize required_memory, vk::DeviceSize required_alignment) -> Result<BufferRange>;
		auto begin_commands(ResourceSet& resource_set) -> void;
		auto end_commands(ResourceSet& resource_set) -> void;

		Queue queue_{};
		CommandPool command_pool_{};

		mi::Vector<ResourceSet> resource_sets_{};
		std::atomic_uint32_t current_resource_set_{ 0u };

		std::thread uploader_thread_{};
		std::atomic_bool should_stop_{ false };

		std::atomic_uint64_t task_counter_{ 1ull };
		std::atomic_uint64_t last_completed_task_{ 0ull };

		Semaphore* last_signalled_semaphore_{ nullptr };
		uint64_t last_signalled_value_{ 0ull };

		std::mutex pending_tasks_mutex_{};
		std::mutex semaphore_mutex_{};
		std::mutex finished_tasks_mutex_{};
		std::mutex token_wait_mutex_{};

		std::condition_variable pending_tasks_cv_{};
		std::condition_variable token_completion_cv_{};

		mi::Vector<UploadTask> pending_tasks_{};
		mi::Vector<UploadResult> finished_tasks_{};
	};
}