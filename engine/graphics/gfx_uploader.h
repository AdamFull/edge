#ifndef GFX_UPLOADER_H
#define GFX_UPLOADER_H

#include "gfx_context.h"

#include <string.hpp>
#include <scheduler.hpp>

namespace edge::gfx {
	struct Uploader;

	struct ResourceSet {
		Buffer staging_memory = {};
		std::atomic<u64> staging_offset = 0;

		Array<Buffer> temp_staging_memory = {};

		Semaphore semaphore = {};
		std::atomic<u64> counter = 0;
		bool first_submition = true;

		CmdBuf cmd = {};
		bool recording = false;

		bool create(NotNull<const Allocator*> alloc, NotNull<Uploader*> uploader);
		void destroy(NotNull<const Allocator*> alloc, NotNull<Uploader*> uploader);

		bool begin();
		bool end();

		BufferView try_allocate_staging_memory(NotNull<const Allocator*> alloc, VkDeviceSize required_memory, VkDeviceSize required_alignment);
	};

	enum class ImageLoadingError {
		HeaderReadingError,
		FailedToCreateImage,
		FailedToReadData
	};

	enum class UploadingCommandType {
		Image,
		Geometry
	};

	struct UploadingCommand {
		UploadingCommandType type;
		const char* path = nullptr;
	};

	struct UploaderCreateInfo {
		Scheduler* sched = nullptr;
		Queue queue = {};
	};

	struct Uploader {
		const Allocator* allocator = nullptr;
		Scheduler* sched = nullptr;

		Queue queue = {};
		CmdPool cmd_pool = {};
		std::atomic<VkSemaphoreSubmitInfoKHR> last_submitted_semaphore = {};

		ResourceSet resource_sets[FRAME_OVERLAP] = {};
		std::atomic<usize> resource_set_index = 0;

		MPMCQueue<UploadingCommand> upload_commands = {};

		Thread thread_handle = {};
		std::atomic<bool> should_exit = false;
		std::atomic<bool> sleeping = false;
		std::atomic<u32> futex_counter = 0;

		bool create(NotNull<const Allocator*> alloc, UploaderCreateInfo create_info);
		void destroy(NotNull<const Allocator*> alloc);

		void load_image(NotNull<const Allocator*> alloc, const char* path);

		ResourceSet& get_resource_set();
	private:
		void load_image_job(NotNull<const Allocator*> alloc, FILE* stream);

		static i32 thread_entry(void* data);
		i32 thread_loop();
	};
}

#endif 