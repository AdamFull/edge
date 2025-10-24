#pragma once

#include "gfx_context.h"

namespace edge::gfx {
	struct ResourceUpdaterInfo {
		vk::DeviceSize update_arena_size{ 4ull * 1024ull * 1024ull };
		uint32_t swap_buffer_count{ 1u };
		Queue const* queue_{ nullptr };
	};

	

	class ResourceUpdater {
	public:
	private:
		struct Node {
			CommandBuffer cmdbuf_;
			Semaphore semaphore_;
			uint64_t counter_{ 0ull };

			Buffer arena_;
			vk::DeviceSize offset_{ 0ull };
			mi::Vector<Buffer> temporary_buffers_;
		};

		Queue const* queue_{ nullptr };
		CommandPool command_pool_;

		mi::Vector<Node> updater_nodes_;
		uint32_t current_node_index_{ 0u };
	};
}