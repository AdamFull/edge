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

		virtual auto submit(const SignalQueueInfo& submit_info) -> SyncResult = 0;
		virtual auto wait_idle() -> void = 0;
	};

	class IGFXContext {
	public:
		virtual ~IGFXContext() = default;

		virtual auto create(const GraphicsContextCreateInfo& create_info) -> bool = 0;

		virtual auto get_queue_count(QueueType queue_type) -> uint32_t = 0;
		virtual auto get_queue(QueueType queue_type, uint32_t queue_index) -> std::expected<std::shared_ptr<IGFXQueue>, bool> = 0;

		virtual auto create_semaphore(uint64_t value) const -> std::shared_ptr<IGFXSemaphore> = 0;
	};
}