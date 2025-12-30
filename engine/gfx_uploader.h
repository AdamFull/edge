#ifndef GFX_UPLOADER_H
#define GFX_UPLOADER_H

#include "gfx_context.h"

#include <array.hpp>
#include <threads.hpp>

namespace edge {
	struct Scheduler;
}

namespace edge::gfx {
	struct Uploader;

	struct ResourceSet {
		Buffer staging_memory = {};
		u64 staging_offset = 0;

		Array<Buffer> temp_staging_memory = {};

		Semaphore semaphore = {};
		std::atomic_uint64_t semaphore_counter = 0;
		bool first_submition = true;

		CmdBuf cmd = {};
		bool recording = false;

		bool create(NotNull<const Allocator*> alloc, NotNull<Uploader*> uploader) noexcept;
		void destroy(NotNull<const Allocator*> alloc, NotNull<Uploader*> uploader) noexcept;

		bool begin() noexcept;
		bool end() noexcept;

		BufferView try_allocate_staging_memory(NotNull<const Allocator*> alloc, VkDeviceSize required_memory, VkDeviceSize required_alignment) noexcept;
	};

	struct Uploader {
		Scheduler* sched = nullptr;

		Queue queue = {};
		CmdPool cmd_pool = {};

		ResourceSet resource_sets[3] = {};

		Thread uploader_thread = {};
	};

	struct UploaderCreateInfo {
		const Allocator* alloc = nullptr;
		Scheduler* sched = nullptr;
		Queue queue = {};
	};

	Uploader* uploader_create(UploaderCreateInfo create_info);
	void uploader_destroy(NotNull<const Allocator*> alloc, Uploader* uploader);
}

#endif 