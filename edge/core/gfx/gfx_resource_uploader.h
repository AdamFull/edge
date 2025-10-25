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

	enum class ImageImportType {
		eDefault,
		eNormalMap,
		eSingleChannel
	};

	struct BufferImportInfo {
		bool boba;
	};

	struct ImageImportInfo {
		mi::U8String path{};
		ImageImportType import_type{ ImageImportType::eDefault };
		bool generate_mipmaps{ false };
		struct {
			mi::Vector<uint8_t> data{};
			uint32_t width{ 1u };
			uint32_t height{ 1u };
		} raw{};
	};

	struct UploadTask {
		UploadType type;
		uint64_t sync_token;
		std::variant<BufferImportInfo, ImageImportInfo> import_info;
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
		[[nodiscard]] auto load_image(ImageImportInfo&& import_info) -> uint64_t;

		auto is_task_done(uint64_t task_id) const -> bool;
		auto wait_for_task(uint64_t task_id) -> void;
		auto get_last_complete_task_id() const -> uint64_t;
		auto get_task_result(uint64_t task_id) -> std::optional<UploadResult>;
		auto is_all_work_complete() const -> bool;
		auto wait_all_work_complete() -> void;

		auto get_last_submitted_semaphore() const -> vk::SemaphoreSubmitInfoKHR;
	private:
		auto _construct(vk::DeviceSize arena_size, uint32_t uploader_count) -> vk::Result;

		enum class ImageLoadStatus {
			eDone,
			eInvalidHeader,
			eOutOfMemory
		};

		struct ImageUploadInfo {
			uint32_t width{ 1u }, height{ 1u }, depth{ 1u };
			uint32_t layer_count{ 1u }, level_count{ 1u }, face_count{ 1u };
			bool generate_mipmap{ false };
			vk::Format format;
			mi::Vector<uint64_t> src_copy_offsets;
			BufferRange memory_range;
		};

		using ImageLoadResult = std::expected<ImageUploadInfo, ImageLoadStatus>;

		auto worker_loop() -> void;
		auto process_task(ResourceSet& resource_set, UploadTask const& task) -> UploadResult;
		auto process_image(ResourceSet& resource_set, UploadTask const& task) -> UploadResult;
		auto get_or_allocate_staging_memory(ResourceSet& resource_set, vk::DeviceSize required_memory, vk::DeviceSize required_alignment) -> Result<BufferRange>;
		auto _load_image_raw(ResourceSet& resource_set, Span<const uint8_t> image_raw_data, uint32_t width, uint32_t height, vk::Format format, bool generate_mipmap) -> ImageLoadResult;
		auto _load_image_stb(ResourceSet& resource_set, Span<const uint8_t> image_raw_data, vk::Format format) -> ImageLoadResult;
		auto _load_image_ktx(ResourceSet& resource_set, Span<const uint8_t> image_raw_data) -> ImageLoadResult;
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

		vk::SemaphoreSubmitInfoKHR last_submitted_semaphore_{};

		std::mutex pending_tasks_mutex_{};
		mutable std::mutex semaphore_mutex_{};
		std::mutex finished_tasks_mutex_{};
		std::mutex token_wait_mutex_{};

		std::condition_variable pending_tasks_cv_{};
		std::condition_variable token_completion_cv_{};

		mi::Vector<UploadTask> pending_tasks_{};
		mi::Vector<UploadResult> finished_tasks_{};
	};
}