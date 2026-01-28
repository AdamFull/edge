#include "gfx_uploader.h"

#include "gfx_context.h"

#include <math.hpp>
#include <scheduler.hpp>
#include <logger.hpp>
#include <image.hpp>

#include <algorithm>

#include <volk.h>

namespace edge::gfx {
	bool ResourceSet::create(NotNull<const Allocator*> alloc, NotNull<Uploader*> uploader) {
		BufferCreateInfo buffer_create_info = {
				.size = 32 * 1024 * 1024,
				.alignment = 1,
				.flags = BUFFER_FLAG_STAGING
		};

		if (!staging_memory.create(buffer_create_info)) {
			return false;
		}

		temp_staging_memory.reserve(alloc, 128);

		if (!semaphore.create(VK_SEMAPHORE_TYPE_TIMELINE_KHR, 0ull)) {
			return false;
		}

		if (!cmd.create(uploader->cmd_pool)) {
			return false;
		}

		return true;
	}

	void ResourceSet::destroy(NotNull<const Allocator*> alloc, NotNull<Uploader*> uploader) {
		cmd.destroy();
		semaphore.destroy();
		staging_memory.destroy();

		for (auto& buffer : temp_staging_memory) {
			buffer.destroy();
		}
		temp_staging_memory.destroy(alloc);
	}

	bool ResourceSet::begin() {
		if (!recording) {
			staging_offset.store(0, std::memory_order_release);

			for (auto& buffer : temp_staging_memory) {
				buffer.destroy();
			}
			temp_staging_memory.clear();

			if (!cmd.begin()) {
				return false;
			}

			cmd.begin_marker("update", 0xFFFFFFFF);
			recording = true;
		}
		return true;
	}

	bool ResourceSet::end() {
		if (recording) {
			cmd.end_marker();
			cmd.end();
			recording = false;
			return true;
		}
		return false;
	}

	BufferView ResourceSet::try_allocate_staging_memory(NotNull<const Allocator*> alloc, VkDeviceSize required_memory, VkDeviceSize required_alignment) {
		if (!begin()) {
			return {};
		}

		VkDeviceSize aligned_requested_size = align_up(required_memory, required_alignment);
		VkDeviceSize available_size = staging_memory.memory.size - staging_offset.load(std::memory_order_acquire);

		if (staging_memory.memory.size < aligned_requested_size || available_size < aligned_requested_size) {
			BufferCreateInfo create_info = {
				.size = required_memory,
				.alignment = required_alignment,
				.flags = BUFFER_FLAG_STAGING
			};

			Buffer new_buffer;
			if (!new_buffer.create(create_info) || !temp_staging_memory.push_back(alloc, new_buffer)) {
				return {};
			}

			return BufferView{ .buffer = new_buffer, .local_offset = 0, .size = aligned_requested_size };
		}

		return BufferView{
			.buffer = staging_memory,
			.local_offset = staging_offset.fetch_add(aligned_requested_size, std::memory_order_acq_rel),
			.size = aligned_requested_size
		};
	}

	bool Uploader::create(NotNull<const Allocator*> alloc, UploaderCreateInfo create_info) {
		allocator = alloc.m_ptr;
		sched = create_info.sched;
		queue = create_info.queue;

		if (!cmd_pool.create(queue)) {
			destroy(alloc);
			return false;
		}

		for (usize i = 0; i < FRAME_OVERLAP; ++i) {
			if (!resource_sets[i].create(alloc, this)) {
				destroy(alloc);
				return false;
			}
		}

		if (!upload_commands.create(alloc, 64)) {
			destroy(alloc);
			return false;
		}

		should_exit.store(false, std::memory_order_release);

		if (thread_create(&thread_handle, Uploader::thread_entry, this) != ThreadResult::Success) {
			destroy(alloc);
			return false;
		}

		return true;
	}

	void Uploader::destroy(NotNull<const Allocator*> alloc) {
		queue.wait_idle();

		should_exit.store(true, std::memory_order_release);
		futex_counter.fetch_add(1, std::memory_order_release);
		futex_wake_all(&futex_counter);
		thread_join(thread_handle, nullptr);

		// TODO: Free commands
		upload_commands.destroy(alloc);

		for (usize i = 0; i < FRAME_OVERLAP; ++i) {
			resource_sets[i].destroy(alloc, this);
		}

		cmd_pool.destroy();
	}

	ImagePromise* Uploader::load_image(NotNull<const Allocator*> alloc, const char* path) {
		ImagePromise* promise = alloc->allocate<ImagePromise>();

		upload_commands.enqueue({ 
			.type = UploadingCommandType::Image, 
			.path = path,
			.image_promise = promise
			});

		if (sleeping.load(std::memory_order_acquire)) {
			futex_counter.fetch_add(1, std::memory_order_release);
			futex_wake(&futex_counter);
		}

		return promise;
	}

	ResourceSet& Uploader::get_resource_set() {
		return resource_sets[resource_set_index.load(std::memory_order_relaxed) % FRAME_OVERLAP];
	}

	void Uploader::load_image_job(NotNull<const Allocator*> alloc, const char* path) {
		// TODO: Write error descriptions and converters
		auto reader_open_result = open_image_reader(alloc, path);
		if (!reader_open_result) {
			job_failed(ImageLoadingError::OpenImageError);
			return;
		}

		IImageReader* reader = reader_open_result.value();
		auto reader_result = reader->create(alloc);
		if (reader_result != IImageReader::Result::Success) {
			job_failed(ImageLoadingError::HeaderReadingError);
			return;
		}

		const ImageInfo& image_info = reader->get_info();

		const ImageCreateInfo create_info = {
				.extent = { image_info.base_width, image_info.base_height, image_info.base_depth},
				.level_count = image_info.mip_levels,
				.layer_count = image_info.array_layers,
				.face_count = 1,
				.usage_flags = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
				.format = static_cast<VkFormat>(image_info.format_desc->vk_format)
		};

		Image image = {};
		if (!image.create(create_info)) {
			EDGE_LOG_ERROR("Image loading failed. Can't create image handle.");
			job_failed(ImageLoadingError::FailedToCreateImage);
			return;
		}

		ResourceSet& set = get_resource_set();
		if (!set.recording) {
			set.begin();
		}

		PipelineBarrierBuilder barrier_builder = {};
		barrier_builder.add_image(image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = image_info.mip_levels,
				.baseArrayLayer = 0,
				.layerCount = image_info.array_layers
			});
		set.cmd.pipeline_barrier(barrier_builder);
		barrier_builder.reset();

		image.layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

		// Copy image data
		BufferView buffer_view = set.try_allocate_staging_memory(alloc, image_info.whole_size, 16);
		if (!buffer_view) {
			EDGE_LOG_ERROR("Image loading failed. Failed to allocate uploading memory.");
			job_failed(ImageLoadingError::FailedToAllocateStagingMemory);
			return;
		}

		Buffer staging_buffer = buffer_view.buffer;
		usize copy_offset = buffer_view.local_offset;
		Array<VkBufferImageCopy2KHR> copy_regions = {};

		void* buffer_dst = staging_buffer.memory.map();

		ImageBlockInfo read_block_info = {};
		while (reader->read_next_block(buffer_dst, copy_offset, read_block_info) != IImageReader::Result::EndOfStream) {
			copy_regions.push_back(alloc, {
				.sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2_KHR,
				.bufferOffset = read_block_info.write_offset,
				.imageSubresource = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.mipLevel = read_block_info.mip_level,
					.baseArrayLayer = read_block_info.array_layer,
					.layerCount = read_block_info.layer_count
					},
				.imageOffset = {},
				.imageExtent = {
					.width = read_block_info.block_width,
					.height = read_block_info.block_height,
					.depth = read_block_info.block_depth
				}
			});
		}

		reader->destroy(alloc);
		alloc->deallocate(reader);

		const VkCopyBufferToImageInfo2KHR copy_image_info = {
			.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2_KHR,
			.srcBuffer = staging_buffer,
			.dstImage = image,
			.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.regionCount = static_cast<u32>(copy_regions.size()),
			.pRegions = copy_regions.data()
		};

		vkCmdCopyBufferToImage2KHR(set.cmd, &copy_image_info);
		
		copy_regions.destroy(alloc);

		job_return(image);
	}

	i32 Uploader::thread_entry(void* data) {
		return static_cast<Uploader*>(data)->thread_loop();
	}

	i32 Uploader::thread_loop() {
		Array<Job*> uploading_jobs = {};
		Array<ImagePromise*> image_promises = {};
		usize promise_count = 0;

		if (!image_promises.reserve(allocator, 64)) {
			return -1;
		}

		while (!should_exit.load(std::memory_order_acquire)) {
			UploadingCommand command = {};
			while (upload_commands.dequeue(&command)) {
				// TODO: Make job depends on job type
				Job* job = Job::from_lambda(allocator, sched,
					[this, path = command.path]() {
						load_image_job(allocator, path);
					});

				if (!job) {
					// TODO: LOG ERROR
					continue;
				}

				job->promise = command.image_promise;

				image_promises.push_back(allocator, command.image_promise);
				uploading_jobs.push_back(allocator, job);
			}

			if (uploading_jobs.empty()) {
				sleeping.store(true, std::memory_order_release);
				u32 futex_val = futex_counter.load(std::memory_order_acquire);
				futex_wait(&futex_counter, futex_val, std::chrono::nanoseconds::max());
				sleeping.store(false, std::memory_order_release);
				continue;
			}

			sched->schedule(uploading_jobs, Scheduler::Workgroup::IO);

			// Wait for all scheduled work.
			while (std::any_of(image_promises.begin(), image_promises.end(),
				[](const auto& p) { return !p->is_done(); })) {
				thread_yield();
			}

			usize set_idx = resource_set_index.fetch_add(1, std::memory_order_acq_rel) % FRAME_OVERLAP;
			ResourceSet& set = resource_sets[set_idx];

			if (set.recording) {
				set.cmd.end();

				u64 wait_value = set.counter.fetch_add(1, std::memory_order_relaxed);
				u64 signal_value = wait_value + 1;

				VkSemaphoreSubmitInfoKHR wait_info = {
					.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR,
					.semaphore = set.semaphore,
					.value = wait_value,
					.stageMask = VK_PIPELINE_STAGE_2_COPY_BIT_KHR
				};

				VkSemaphoreSubmitInfoKHR signal_info = {
					.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR,
					.semaphore = set.semaphore,
					.value = signal_value,
					.stageMask = VK_PIPELINE_STAGE_2_COPY_BIT_KHR
				};

				VkCommandBufferSubmitInfoKHR command_buffer_info = {
					.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO_KHR,
					.commandBuffer = set.cmd
				};

				VkSubmitInfo2KHR submit_info = {
					.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR,
					.waitSemaphoreInfoCount = set.first_submition ? 0u : 1u,
					.pWaitSemaphoreInfos = set.first_submition ? nullptr : &wait_info,
					.commandBufferInfoCount = 1,
					.pCommandBufferInfos = &command_buffer_info,
					.signalSemaphoreInfoCount = 1,
					.pSignalSemaphoreInfos = &signal_info
				};

				// TODO: Check result
				queue.submit(VK_NULL_HANDLE, &submit_info);

				if (set.first_submition) {
					set.first_submition = false;
				}

				last_submitted_semaphore.store(signal_info, std::memory_order_release);

				EDGE_LOG_INFO("Submitted %d uploads.", uploading_jobs.size());
			}

			uploading_jobs.clear();
			promise_count = 0;
		}

		// Cleanup
		uploading_jobs.destroy(allocator);
		image_promises.destroy(allocator);

		return 0;
	}
}