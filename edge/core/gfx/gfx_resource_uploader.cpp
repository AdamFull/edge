#include "gfx_resource_uploader.h"

#include "../filesystem/filesystem.h"

#include <stb_image.h>
#include <ktx.h>
#include <ktxvulkan.h>

#define EDGE_LOGGER_SCOPE "gfx::ResourceUploader"

namespace {
	constexpr uint8_t PNG_MAGIC[] = { 0x89, 0x50, 0x4E, 0x47 };
	constexpr uint8_t JPEG_MAGIC[] = { 0xFF, 0xD8, 0xFF };
	constexpr uint8_t KTX_MAGIC[] = { 0xAB, 0x4B, 0x54, 0x58 };

	template<size_t N>
	bool matches_magic(const edge::mi::Vector<uint8_t>& data, const uint8_t(&magic)[N]) {
		if (data.size() < N) return false;
		return std::memcmp(data.data(), magic, N) == 0;
	}
}

namespace edge::gfx {
	ResourceUploader::~ResourceUploader() {
		stop_streamer();
	}

	ResourceUploader::ResourceUploader(ResourceUploader&& other) noexcept {
		other.stop_streamer();

		queue_ = std::move(other.queue_);
		command_pool_ = std::move(other.command_pool_);
		resource_sets_ = std::exchange(other.resource_sets_, {});
		current_resource_set_.store(other.current_resource_set_.load(std::memory_order_relaxed), std::memory_order_relaxed);

		pending_tasks_ = std::exchange(other.pending_tasks_, {});
		finished_tasks_ = std::exchange(other.finished_tasks_, {});

		should_stop_.store(other.should_stop_.load(std::memory_order_relaxed), std::memory_order_relaxed);
		task_counter_.store(other.task_counter_.load(std::memory_order_relaxed), std::memory_order_relaxed);
		last_completed_task_.store(other.last_completed_task_.load(std::memory_order_relaxed), std::memory_order_relaxed);
	}

	auto ResourceUploader::operator=(ResourceUploader&& other) noexcept -> ResourceUploader& {
		if (this != &other) {
			stop_streamer();
			other.stop_streamer();

			queue_ = std::move(other.queue_);
			command_pool_ = std::move(other.command_pool_);
			resource_sets_ = std::exchange(other.resource_sets_, {});
			current_resource_set_.store(other.current_resource_set_.load(std::memory_order_relaxed), std::memory_order_relaxed);

			pending_tasks_ = std::exchange(other.pending_tasks_, {});
			finished_tasks_ = std::exchange(other.finished_tasks_, {});

			should_stop_.store(other.should_stop_.load(std::memory_order_relaxed), std::memory_order_relaxed);
			task_counter_.store(other.task_counter_.load(std::memory_order_relaxed), std::memory_order_relaxed);
			last_completed_task_.store(other.last_completed_task_.load(std::memory_order_relaxed), std::memory_order_relaxed);
		}

		return *this;
	}

	auto ResourceUploader::create(vk::DeviceSize arena_size, uint32_t uploader_count) -> Result<ResourceUploader> {
		ResourceUploader self{};
		if (auto result = self._construct(arena_size, uploader_count); result != vk::Result::eSuccess) {
			return std::unexpected(result);
		}
		return self;
	}

	auto ResourceUploader::start_streamer() -> void {
		uploader_thread_ = std::thread([this]() { worker_loop(); });
	}

	auto ResourceUploader::stop_streamer() -> void {
		if (uploader_thread_.joinable()) {
			{
				std::lock_guard lock(pending_tasks_mutex_);
				should_stop_.store(true, std::memory_order_release);
			}
			pending_tasks_cv_.notify_all();
			uploader_thread_.join();
		}

		if (queue_) {
			auto wait_result = queue_->waitIdle();
			GFX_ASSERT_MSG(wait_result == vk::Result::eSuccess, "Failed to wait queue submission finished.");
		}
	}

	auto ResourceUploader::load_image(ImageImportInfo&& import_info) -> uint64_t {
		auto next_token = task_counter_.fetch_add(1ull, std::memory_order_relaxed); 

		{
			std::unique_lock lock(pending_tasks_mutex_);
			pending_tasks_.emplace_back(UploadTask{
				.type = UploadType::eImage,
				.sync_token = next_token,
				.import_info = std::move(import_info)
				});
		}
		pending_tasks_cv_.notify_one();

		return next_token;
	}

	auto ResourceUploader::is_task_done(uint64_t task_id) const -> bool {
		return last_completed_task_.load(std::memory_order_acquire) >= task_id;
	}

	auto ResourceUploader::wait_for_task(uint64_t task_id) -> void {
		std::unique_lock lock(token_wait_mutex_);
		token_completion_cv_.wait(lock, [this, task_id] {
			return last_completed_task_.load(std::memory_order_acquire) >= task_id;
			});
	}

	auto ResourceUploader::get_last_complete_task_id() const -> uint64_t {
		return last_completed_task_.load(std::memory_order_acquire);
	}

	auto ResourceUploader::get_task_result(uint64_t task_id) -> std::optional<UploadResult> {
		std::lock_guard lock(finished_tasks_mutex_);

		auto found = std::find_if(finished_tasks_.begin(), finished_tasks_.end(),
			[task_id](const UploadResult& task) { return task.sync_token == task_id; });

		if (found != finished_tasks_.end()) {
			UploadResult result = std::move(*found);
			finished_tasks_.erase(found);
			return result;
		}

		return std::nullopt;
	}

	auto ResourceUploader::is_all_work_complete() const -> bool {
		auto token = task_counter_.load(std::memory_order_relaxed) - 1ull;
		return token <= last_completed_task_.load(std::memory_order_acquire);
	}

	auto ResourceUploader::wait_all_work_complete() -> void {
		auto token = task_counter_.load(std::memory_order_relaxed) - 1ull;
		wait_for_task(token);
	}

	auto ResourceUploader::get_last_submitted_semaphore() const -> vk::SemaphoreSubmitInfoKHR {
		std::lock_guard lock(semaphore_mutex_);
		return last_submitted_semaphore_;
	}

	auto ResourceUploader::_construct(vk::DeviceSize arena_size, uint32_t uploader_count) -> vk::Result {
		auto queue_result = device_.get_queue(QueueType::eDirect);
		if (!queue_result) {
			return queue_result.error();
		}
		queue_ = std::move(queue_result.value());

		auto command_pool_result = queue_.create_command_pool();
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

		pending_tasks_.reserve(100);
		finished_tasks_.reserve(100);

		return vk::Result::eSuccess;
	}

	auto ResourceUploader::worker_loop() -> void {
		while (!should_stop_.load(std::memory_order_acquire)) {

			// Grab current tasks
			mi::Vector<UploadTask> tasks_to_process;
			{
				std::unique_lock lock(pending_tasks_mutex_);
				pending_tasks_cv_.wait(lock, [&] { return !pending_tasks_.empty() || should_stop_.load(std::memory_order_acquire); });

				if (should_stop_.load(std::memory_order_acquire)) {
					break;
				}

				std::swap(tasks_to_process, pending_tasks_);
			}

			if (!tasks_to_process.empty()) {
				auto& resource_set = resource_sets_[current_resource_set_.load(std::memory_order_relaxed)];

				begin_commands(resource_set);

				// Process resources
				for (auto const& task : tasks_to_process) {
					auto result = process_task(resource_set, task);
					{
						std::lock_guard lock(finished_tasks_mutex_);
						finished_tasks_.push_back(std::move(result));
					}

					{
						std::lock_guard lock(token_wait_mutex_);
						last_completed_task_.store(task.sync_token, std::memory_order_release);
					}
					token_completion_cv_.notify_all();
				}

				end_commands(resource_set);

				// Submit commands
				{
					std::lock_guard lock(semaphore_mutex_);

					uint64_t wait_value = resource_set.counter.fetch_add(1, std::memory_order_relaxed);
					uint64_t signal_value = wait_value + 1;

					vk::SemaphoreSubmitInfoKHR wait_info{};
					wait_info.semaphore = *resource_set.semaphore;
					wait_info.value = wait_value;
					wait_info.stageMask = vk::PipelineStageFlagBits2::eCopy;
					wait_info.deviceIndex = 0;

					vk::SemaphoreSubmitInfoKHR signal_info{};
					signal_info.semaphore = *resource_set.semaphore;
					signal_info.value = signal_value;
					signal_info.stageMask = vk::PipelineStageFlagBits2::eCopy;
					signal_info.deviceIndex = 0;

					vk::CommandBufferSubmitInfoKHR command_buffer_info{};
					command_buffer_info.commandBuffer = *resource_set.command_buffer;

					vk::SubmitInfo2KHR submit_info{};
					submit_info.waitSemaphoreInfoCount = resource_set.first_submission ? 0u : 1u;
					submit_info.pWaitSemaphoreInfos = resource_set.first_submission ? nullptr : &wait_info;
					submit_info.signalSemaphoreInfoCount = 1u;
					submit_info.pSignalSemaphoreInfos = &signal_info;
					submit_info.commandBufferInfoCount = 1u;
					submit_info.pCommandBufferInfos = &command_buffer_info;

					auto submit_result = queue_->submit2KHR(1u, &submit_info, VK_NULL_HANDLE);
					GFX_ASSERT_MSG(submit_result == vk::Result::eSuccess, "Failed to submit uploader queue.");

					if (resource_set.first_submission) {
						resource_set.first_submission = false;
					}

					last_submitted_semaphore_ = signal_info;

					uint32_t current = current_resource_set_.load(std::memory_order_relaxed);
					uint32_t next = (current + 1u) % static_cast<uint32_t>(resource_sets_.size());
					current_resource_set_.store(next, std::memory_order_relaxed);
				}
			}
		}
	}

	auto ResourceUploader::process_task(ResourceSet& resource_set, UploadTask const& task) -> UploadResult {
		switch (task.type)
		{
		case UploadType::eBuffer: break;
		case UploadType::eImage: 
			return process_image(resource_set, task);
		}

		return UploadResult{ task.sync_token, task.type, UploadingStatus::eFailed, {} };
	}

	auto ResourceUploader::process_image(ResourceSet& resource_set, UploadTask const& task) -> UploadResult {
		auto& import_info = std::get<ImageImportInfo>(task.import_info);

		ImageUploadInfo upload_info{};

		if (!import_info.path.empty()) {
			fs::InputFileStream file(import_info.path, std::ios_base::binary);
			if (!file.is_open()) {
				EDGE_SLOGW("Failed to open file: {}", reinterpret_cast<const char*>(import_info.path.c_str()));
				return UploadResult{ task.sync_token, UploadType::eImage, UploadingStatus::eNotFound, {} };
			}

			file.seekg(0, std::ios::end);
			auto file_size = file.tellg();
			file.seekg(0, std::ios::beg);

			mi::Vector<uint8_t> file_data;
			file_data.resize(file_size);

			file.read(reinterpret_cast<char*>(file_data.data()), file_size);

			if (file_data.empty()) {
				return UploadResult{ task.sync_token, UploadType::eImage, UploadingStatus::eFailed, {} };
			}

			if (matches_magic(file_data, PNG_MAGIC) || matches_magic(file_data, JPEG_MAGIC)) {
				vk::Format format{};
				switch (import_info.import_type)
				{
				case ImageImportType::eDefault: format = vk::Format::eR8G8B8A8Srgb; break;
				case ImageImportType::eNormalMap: format = vk::Format::eR8G8B8A8Unorm; break;
				case ImageImportType::eSingleChannel: format = vk::Format::eR8Unorm; break;
				}

				auto result = _load_image_stb(resource_set, file_data, format);
				if (!result) {
					// TODO: Handle error
				}
				upload_info = std::move(result.value());
			}
			else if (matches_magic(file_data, KTX_MAGIC)) {
				auto result = _load_image_ktx(resource_set, file_data);
				if (!result) {
					// TODO: Handle error
				}
				upload_info = std::move(result.value());
			}
			else {
				// Unsupported format
				return UploadResult{ task.sync_token, UploadType::eImage, UploadingStatus::eNotSupported, {} };
			}
		}
		else {
			vk::Format format{};
			switch (import_info.import_type)
			{
			case ImageImportType::eDefault: format = vk::Format::eR8G8B8A8Srgb; break;
			case ImageImportType::eNormalMap: format = vk::Format::eR8G8B8A8Unorm; break;
			case ImageImportType::eSingleChannel: format = vk::Format::eR8Unorm; break;
			}

			auto result = _load_image_raw(resource_set, import_info.raw.data, import_info.raw.width, import_info.raw.height, format, false);
			if (!result) {
				// TODO: Handle error
			}
			upload_info = std::move(result.value());
		}

		mi::U8String zone_name = !import_info.path.empty() ? import_info.path : u8"image";

		resource_set.command_buffer.begin_marker(reinterpret_cast<const char*>(zone_name.c_str()));

		ImageCreateInfo create_info{};
		create_info.extent.width = upload_info.width;
		create_info.extent.height = upload_info.height;
		create_info.extent.depth = upload_info.depth;
		create_info.layer_count = upload_info.face_count * upload_info.layer_count;
		create_info.level_count = upload_info.level_count;
		create_info.format = upload_info.format;
		create_info.flags = ImageFlag::eSample | ImageFlag::eCopyTarget;

		auto image_result = Image::create(create_info);
		if (!image_result) {
			GFX_ASSERT_MSG(false, "Failed to load texture. Can't create image handle. Reason: {}", vk::to_string(image_result.error()));
			resource_set.command_buffer.end_marker();
			return UploadResult{ task.sync_token, task.type, UploadingStatus::eFailed, {} };
		}
		auto image = std::move(image_result.value());

		ImageBarrier image_barrier{};
		image_barrier.image = &image;
		image_barrier.src_state = ResourceStateFlag::eUndefined;
		image_barrier.dst_state = ResourceStateFlag::eCopyDst;
		image_barrier.subresource_range.aspectMask = vk::ImageAspectFlagBits::eColor;
		image_barrier.subresource_range.baseArrayLayer = 0u;
		image_barrier.subresource_range.baseMipLevel = 0u;
		image_barrier.subresource_range.layerCount = create_info.layer_count;
		image_barrier.subresource_range.levelCount = create_info.level_count;
		resource_set.command_buffer.push_barrier(image_barrier);

		mi::Vector<vk::BufferImageCopy2KHR> copy_regions{};
		copy_regions.reserve(create_info.layer_count * create_info.level_count);

		for (int32_t layer = 0; layer < static_cast<int32_t>(create_info.layer_count); layer++) {
			for (int32_t level = 0; level < static_cast<int32_t>(create_info.level_count); level++) {
				vk::BufferImageCopy2KHR image_copy_region{};
				image_copy_region.bufferOffset = upload_info.memory_range.get_offset();
				image_copy_region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
				image_copy_region.imageSubresource.mipLevel = level;
				image_copy_region.imageSubresource.baseArrayLayer = layer;
				image_copy_region.imageSubresource.layerCount = 1u;
				image_copy_region.imageOffset = vk::Offset3D{ 0, 0, 0 };
				image_copy_region.imageExtent = vk::Extent3D{ upload_info.width >> level, upload_info.height >> level, 1u };

				ktx_size_t source_offset = upload_info.src_copy_offsets[layer * create_info.level_count + level];
				image_copy_region.bufferOffset += source_offset;
				copy_regions.push_back(image_copy_region);
			}
		}

		vk::CopyBufferToImageInfo2KHR copy_info{};
		copy_info.srcBuffer = upload_info.memory_range.get_buffer();
		copy_info.dstImage = image.get_handle();
		copy_info.dstImageLayout = vk::ImageLayout::eTransferDstOptimal;
		copy_info.regionCount = static_cast<uint32_t>(copy_regions.size());
		copy_info.pRegions = copy_regions.data();

		resource_set.command_buffer->copyBufferToImage2KHR(&copy_info);

		image_barrier.src_state = image_barrier.dst_state;
		image_barrier.dst_state = ResourceStateFlag::eShaderResource;
		resource_set.command_buffer.push_barrier(image_barrier);

		resource_set.command_buffer.end_marker();

		return UploadResult{ task.sync_token, UploadType::eImage, UploadingStatus::eDone, std::move(image) };
	}

	auto ResourceUploader::_load_image_raw(ResourceSet& resource_set, Span<const uint8_t> image_raw_data, uint32_t width, uint32_t height, vk::Format format, bool generate_mipmap) -> ImageLoadResult {
		ImageUploadInfo upload_info{};
		upload_info.width = width;
		upload_info.height = height;
		upload_info.depth = 1u;
		upload_info.layer_count = 1u;
		upload_info.level_count = 1u;
		upload_info.face_count = 1u;
		upload_info.format = format;
		upload_info.src_copy_offsets.push_back(0ull);

		// TODO: generate mips

		auto staging_result = get_or_allocate_staging_memory(resource_set, image_raw_data.size(), 4ull);
		if (!staging_result) {
			GFX_ASSERT_MSG(false, "Failed to request staging memory. Reason: {}", vk::to_string(staging_result.error()));
			return std::unexpected(ImageLoadStatus::eOutOfMemory);
		}
		upload_info.memory_range = std::move(staging_result.value());

		auto byte_range = upload_info.memory_range.get_range();
		std::memcpy(byte_range.data(), image_raw_data.data(), image_raw_data.size());

		return upload_info;
	}

	auto ResourceUploader::_load_image_stb(ResourceSet& resource_set, Span<const uint8_t> image_raw_data, vk::Format format) -> ImageLoadResult {
		int32_t width{ 1 }, height{ 1 }, channel_count{ 1 };

		auto* data = stbi_load_from_memory(image_raw_data.data(), static_cast<int32_t>(image_raw_data.size()), &width, &height, &channel_count, STBI_rgb_alpha);
		if (!data) {
			return std::unexpected(ImageLoadStatus::eInvalidHeader);
		}

		auto result = _load_image_raw(resource_set, { data, static_cast<size_t>(width * height * 4) }, width, height, format, false);
		stbi_image_free(data);
		return result;
	}

	auto ResourceUploader::_load_image_ktx(ResourceSet& resource_set, Span<const uint8_t> image_raw_data) -> ImageLoadResult {
		ktxTexture* ktxtexture{ nullptr };
		auto ktxresult = ktxTexture_CreateFromMemory(image_raw_data.data(), image_raw_data.size(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxtexture);
		GFX_ASSERT_MSG(ktxresult == KTX_SUCCESS, "Failed to parse ktx data.");
		if (ktxresult != KTX_SUCCESS) {
			return std::unexpected(ImageLoadStatus::eInvalidHeader);
		}

		ImageUploadInfo upload_info{};
		upload_info.width = ktxtexture->baseWidth;
		upload_info.height = ktxtexture->baseHeight;
		upload_info.depth = ktxtexture->baseDepth;
		upload_info.layer_count = ktxtexture->numLayers;
		upload_info.level_count = ktxtexture->numLevels;
		upload_info.face_count = ktxtexture->numFaces;

		switch (ktxtexture->classId) {
		case ktxTexture1_c: {
			ktxTexture1* ktxtexture1 = (ktxTexture1*)ktxtexture;
			upload_info.format = static_cast<vk::Format>(ktxTexture1_GetVkFormat(ktxtexture1));
		} break;
		case ktxTexture2_c: {
			ktxTexture2* ktxtexture2 = (ktxTexture2*)ktxtexture;

			if (ktxTexture2_NeedsTranscoding(ktxtexture2)) {
				ktxresult = ktxTexture2_TranscodeBasis(ktxtexture2, KTX_TTF_BC7_RGBA, 0);
			}
			upload_info.format = static_cast<vk::Format>(ktxtexture2->vkFormat);
		} break;
		}

		// Collect per layer and per mip offsets
		auto layer_count = upload_info.face_count * upload_info.level_count;
		for (int32_t layer = 0; layer < static_cast<int32_t>(layer_count); layer++) {
			for (int32_t level = 0; level < static_cast<int32_t>(ktxtexture->numLevels); level++) {
				ktx_size_t source_offset;
				ktxTexture_GetImageOffset(ktxtexture, level, 0, layer, &source_offset);
				upload_info.src_copy_offsets.push_back(source_offset);
			}
		}

		// TODO: add mip generation for basic formats

		auto staging_result = get_or_allocate_staging_memory(resource_set, ktxtexture->dataSize, 16ull);
		if (!staging_result) {
			GFX_ASSERT_MSG(false, "Failed to request staging memory. Reason: {}", vk::to_string(staging_result.error()));
			ktxTexture_Destroy(ktxtexture);
			return std::unexpected(ImageLoadStatus::eOutOfMemory);
		}
		upload_info.memory_range = std::move(staging_result.value());

		auto byte_range = upload_info.memory_range.get_range();
		std::memcpy(byte_range.data(), ktxtexture->pData, ktxtexture->dataSize);

		ktxTexture_Destroy(ktxtexture);

		return upload_info;
	}

	auto ResourceUploader::get_or_allocate_staging_memory(ResourceSet& resource_set, vk::DeviceSize required_memory, vk::DeviceSize required_alignment) -> Result<BufferRange> {
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

		// TODO: add flush uploads when arena is full

		auto current_offset = resource_set.offset;
		resource_set.offset += aligned_requested_size;
		return BufferRange::create(&resource_set.arena, current_offset, aligned_requested_size);
	}

	auto ResourceUploader::begin_commands(ResourceSet& resource_set) -> void {
		GFX_ASSERT_MSG(!resource_set.recording, "Commands is already recording.");

		resource_set.offset = 0ull;
		resource_set.temporary_buffers.clear();
		
		auto begin_result = resource_set.command_buffer.begin();
		GFX_ASSERT_MSG(begin_result == vk::Result::eSuccess, "Failed to begin commands.");
		resource_set.command_buffer.begin_marker("Uploader", 0xFFFFFFFF);

		resource_set.recording = true;
	}

	auto ResourceUploader::end_commands(ResourceSet& resource_set) -> void {
		GFX_ASSERT_MSG(resource_set.recording, "Commands was not started to recording.");

		resource_set.command_buffer.end_marker();

		auto end_result = resource_set.command_buffer.end();
		GFX_ASSERT_MSG(end_result == vk::Result::eSuccess, "Failed to end commands.");

		resource_set.recording = false;
	}
}

#undef EDGE_LOGGER_SCOPE // ResourceUploader