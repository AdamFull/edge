#pragma once

#include <chrono>
#include <memory>
#include <expected>

#include "gfx_structures.h"

namespace edge::gfx {
	class IGFXSemaphore {
	public:
		virtual ~IGFXSemaphore() = default;

		virtual auto signal(uint64_t value) -> SyncResult = 0;
		virtual auto wait(uint64_t value, std::chrono::nanoseconds timeout = std::chrono::nanoseconds::max()) -> SyncResult = 0;

		virtual auto is_completed(uint64_t value) const -> bool = 0;
		virtual auto get_value() const -> uint64_t = 0;
	};

	class IGFXQueue {
	public:
		virtual ~IGFXQueue() = default;

		virtual auto create_command_allocator() const -> std::shared_ptr<IGFXCommandAllocator> = 0;

		virtual auto submit(const SignalQueueInfo& submit_info) -> void = 0;
		virtual auto wait_idle() -> SyncResult = 0;
	};

	class IGFXCommandAllocator {
	public:
		virtual ~IGFXCommandAllocator() = default;

		virtual auto allocate_command_list() const -> std::shared_ptr<IGFXCommandList> = 0;
		virtual auto reset() -> void = 0;
	};

	class IGFXCommandList {
	public:
		virtual ~IGFXCommandList() = default;

		virtual auto reset() -> void = 0;

		virtual auto begin() -> void = 0;
		virtual auto end() -> void = 0;

		virtual auto set_viewport(float x, float y, float width, float height, float min_depth, float max_depth) const -> void = 0;
		virtual auto set_scissor(uint32_t x, uint32_t y, uint32_t width, uint32_t height) const -> void = 0;

		virtual auto draw(uint32_t vertex_count, uint32_t first_vertex, uint32_t first_instance, uint32_t instance_count) const -> void = 0;
		virtual auto draw_indexed(uint32_t index_count, uint32_t first_index, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance) const -> void = 0;

		virtual auto dispatch(uint32_t group_x, uint32_t group_y, uint32_t group_z) const -> void = 0;

		virtual auto begin_marker(std::string_view name, uint32_t color) const -> void = 0;
		virtual auto insert_marker(std::string_view name, uint32_t color) const -> void = 0;
		virtual auto end_marker() const -> void = 0;
	};

	class IGFXPresentationEngine {
	public:
		virtual ~IGFXPresentationEngine() = default;

		virtual auto get_queue() const -> std::shared_ptr<IGFXQueue> = 0;
	};

	class IGFXContext {
	public:
		virtual ~IGFXContext() = default;

		virtual auto create(const GraphicsContextCreateInfo& create_info) -> bool = 0;

		virtual auto create_queue(QueueType queue_type) -> std::shared_ptr<IGFXQueue> = 0;
		virtual auto create_semaphore(uint64_t value) const -> std::shared_ptr<IGFXSemaphore> = 0;
	};
}