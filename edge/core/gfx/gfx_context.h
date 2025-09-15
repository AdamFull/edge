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

		virtual auto is_completed(uint64_t value) const -> GFXResult<bool> = 0;
		virtual auto get_value() const -> GFXResult<uint64_t> = 0;
	};

	class IGFXQueue {
	public:
		virtual ~IGFXQueue() = default;

		[[nodiscard]] virtual auto create_command_allocator() const -> GFXResult<Shared<IGFXCommandAllocator>> = 0;

		virtual auto submit(const SubmitQueueInfo& submit_info) -> void = 0;
		virtual auto wait_idle() -> SyncResult = 0;
	};

	class IGFXBufferView {
	public:
		virtual ~IGFXBufferView() = default;

		[[nodiscard]] virtual auto get_offset() const noexcept -> uint64_t = 0;
		[[nodiscard]] virtual auto get_size() const noexcept -> uint64_t = 0;
		[[nodiscard]] virtual auto get_format() const noexcept -> TinyImageFormat = 0;
	};

	class IGFXBuffer {
	public:
		virtual ~IGFXBuffer() = default;

		[[nodiscard]] virtual auto create_view(const BufferViewCreateInfo& create_info) const -> GFXResult<Shared<IGFXBufferView>> = 0;

		[[nodiscard]] virtual auto map() -> GFXResult<std::span<uint8_t>> = 0;
		virtual auto unmap() -> void = 0;
		virtual auto flush(uint64_t size, uint64_t offset) -> Result = 0;

		[[nodiscard]] virtual auto update(const void* data, uint64_t size, uint64_t offset = 0ull) -> GFXResult<uint64_t> = 0;

		[[nodiscard]] virtual auto get_size() const noexcept -> uint64_t = 0;
		[[nodiscard]] virtual auto get_address() const -> uint64_t = 0;
	};

	class IGFXImageView {
	public:
		virtual ~IGFXImageView() = default;

		[[nodiscard]] virtual auto get_first_level() const noexcept -> uint32_t = 0;
		[[nodiscard]] virtual auto get_level_count() const noexcept -> uint32_t = 0;
		[[nodiscard]] virtual auto get_first_layer() const noexcept -> uint32_t = 0;
		[[nodiscard]] virtual auto get_layer_count() const noexcept -> uint32_t = 0;
	};

	class IGFXImage {
	public:
		virtual ~IGFXImage() = default;

		[[nodiscard]] virtual auto create_view(const ImageViewCreateInfo& create_info) const -> GFXResult<Shared<IGFXImageView>> = 0;

		[[nodiscard]] virtual auto get_extent() const noexcept -> Extent3D = 0;
		[[nodiscard]] virtual auto get_level_count() const noexcept -> uint32_t = 0;
		[[nodiscard]] virtual auto get_layer_count() const noexcept -> uint32_t = 0;
		[[nodiscard]] virtual auto get_size() const noexcept -> uint64_t = 0;
	};

	class IGFXCommandAllocator {
	public:
		virtual ~IGFXCommandAllocator() = default;

		[[nodiscard]] virtual auto allocate_command_list() const -> GFXResult<Shared<IGFXCommandList>> = 0;
	};

	class IGFXCommandList {
	public:
		virtual ~IGFXCommandList() = default;

		virtual auto begin() -> bool = 0;
		virtual auto end() -> bool = 0;

		virtual auto set_viewport(float x, float y, float width, float height, float min_depth, float max_depth) const -> void = 0;
		virtual auto set_scissor(uint32_t x, uint32_t y, uint32_t width, uint32_t height) const -> void = 0;

		virtual auto draw(uint32_t vertex_count, uint32_t first_vertex, uint32_t first_instance, uint32_t instance_count) const -> void = 0;
		virtual auto draw_indexed(uint32_t index_count, uint32_t first_index, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance) const -> void = 0;

		virtual auto dispatch(uint32_t group_x, uint32_t group_y, uint32_t group_z) const -> void = 0;

		virtual auto begin_marker(std::string_view name, uint32_t color) const -> void = 0;
		virtual auto insert_marker(std::string_view name, uint32_t color) const -> void = 0;
		virtual auto end_marker() const -> void = 0;
	};

	class IGFXPresentationFrame {
	public:
		virtual ~IGFXPresentationFrame() = default;

		virtual auto get_image() const -> Shared<IGFXImage> = 0;
		virtual auto get_image_view() const -> Shared<IGFXImageView> = 0;

		virtual auto get_command_list() const -> Shared<IGFXCommandList> = 0;
	};

	class IGFXPresentationEngine {
	public:
		virtual ~IGFXPresentationEngine() = default;

		virtual auto begin(uint32_t* frame_index) -> bool = 0;
		virtual auto end(const PresentInfo& present_info) -> bool = 0;

		virtual auto get_queue() const -> Shared<IGFXQueue> = 0;
		virtual auto get_command_allocator() const -> Shared<IGFXCommandAllocator> = 0;

		virtual auto get_current_frame() const -> Shared<IGFXPresentationFrame> = 0;
	};

	class IGFXContext {
	public:
		virtual ~IGFXContext() = default;

		virtual auto create(const GraphicsContextCreateInfo& create_info) -> bool = 0;

		[[nodiscard]] virtual auto create_queue(QueueType queue_type) const -> GFXResult<Shared<IGFXQueue>> = 0;
		[[nodiscard]] virtual auto create_semaphore(uint64_t value) const -> GFXResult<Shared<IGFXSemaphore>> = 0;

		[[nodiscard]] virtual auto create_presentation_engine(const PresentationEngineCreateInfo& create_info) -> GFXResult<Shared<IGFXPresentationEngine>> = 0;

		[[nodiscard]] virtual auto create_buffer(const BufferCreateInfo& create_info) -> GFXResult<Shared<IGFXBuffer>> = 0;
		[[nodiscard]] virtual auto create_image(const ImageCreateInfo& create_info) -> GFXResult<Shared<IGFXImage>> = 0;
	};
}