#include "gfx_uploader.h"

#include "gfx_context.h"

#include <math.hpp>
#include <scheduler.hpp>

namespace edge::gfx {
	bool ResourceSet::create(NotNull<const Allocator*> alloc, NotNull<Uploader*> uploader) noexcept {
		BufferCreateInfo buffer_create_info = {
				.size = 32 * 1024 * 1024,
				.alignment = 1,
				.flags = BUFFER_FLAG_STAGING
		};

		if (!buffer_create(buffer_create_info, staging_memory)) {
			return false;
		}

		temp_staging_memory.reserve(alloc, 128);

		if (!semaphore_create(VK_SEMAPHORE_TYPE_TIMELINE_KHR, 0ull, semaphore)) {
			return false;
		}

		if (!cmd_buf_create(uploader->cmd_pool, cmd)) {
			return false;
		}

		return true;
	}

	void ResourceSet::destroy(NotNull<const Allocator*> alloc, NotNull<Uploader*> uploader) noexcept {
		cmd_buf_destroy(cmd);
		semaphore_destroy(semaphore);
		buffer_destroy(staging_memory);

		for (auto& buffer : temp_staging_memory) {
			buffer_destroy(buffer);
		}
		temp_staging_memory.destroy(alloc);
	}

	bool ResourceSet::begin() noexcept {
		if (!recording) {
			staging_offset = 0;

			for (auto& buffer : temp_staging_memory) {
				buffer_destroy(buffer);
			}
			temp_staging_memory.clear();

			if (!cmd_begin(cmd)) {
				return false;
			}

			cmd_begin_marker(cmd, "update", 0xFFFFFFFF);
			recording = true;
		}
	}

	bool ResourceSet::end() noexcept {
		if (recording) {
			cmd_end_marker(cmd);
			cmd_end(cmd);
			recording = false;
			return true;
		}
		return false;
	}

	BufferView ResourceSet::try_allocate_staging_memory(NotNull<const Allocator*> alloc, VkDeviceSize required_memory, VkDeviceSize required_alignment) noexcept {
		if (!begin()) {
			return {};
		}

		VkDeviceSize aligned_requested_size = align_up(required_memory, required_alignment);
		VkDeviceSize available_size = staging_memory.memory.size - staging_offset;

		if (staging_memory.memory.size < aligned_requested_size || available_size < aligned_requested_size) {
			BufferCreateInfo create_info = {
				.size = required_memory,
				.alignment = required_alignment,
				.flags = BUFFER_FLAG_STAGING
			};

			Buffer new_buffer;
			if (!buffer_create(create_info, new_buffer) || !temp_staging_memory.push_back(alloc, new_buffer)) {
				return {};
			}

			return BufferView{ .buffer = new_buffer, .offset = 0, .size = aligned_requested_size };
		}

		return BufferView{
			.buffer = staging_memory,
			.offset = std::exchange(staging_offset, staging_offset + aligned_requested_size),
			.size = aligned_requested_size
		};
	}

	Uploader* uploader_create(UploaderCreateInfo create_info) {
		Uploader* uploader = create_info.alloc->allocate<Uploader>();
		if (!uploader) {
			return nullptr;
		}

		uploader->sched = create_info.sched;
		uploader->queue = create_info.queue;

		if (!uploader->cmd_pool.create(uploader->queue)) {
			uploader_destroy(create_info.alloc, uploader);
			return nullptr;
		}

		for (isize i = 0; i < 3; ++i) {
			if (!uploader->resource_sets[i].create(create_info.alloc, uploader)) {
				uploader_destroy(create_info.alloc, uploader);
				return nullptr;
			}
		}

		// TODO: create uploader thread
		//Thread thrd;
		//thread_create(&thrd, )

		return uploader;
	}

	void uploader_destroy(NotNull<const Allocator*> alloc, Uploader* uploader) {
		if (!uploader) {
			return;
		}

		for (isize i = 0; i < 3; ++i) {
			uploader->resource_sets[i].destroy(alloc, uploader);
		}

		uploader->cmd_pool.destroy();
		alloc->deallocate(uploader);
	}
}