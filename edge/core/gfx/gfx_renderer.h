#pragma once

#include "gfx_context.h"

namespace edge::gfx {
	class Frame {
	public:
		Frame() = default;
		~Frame();

		Frame(const Frame&) = delete;
		auto operator=(const Frame&) -> Frame& = delete;

		Frame(Frame&& other)
			: image_{ std::exchange(other.image_, nullptr) }
			, image_view_{ std::exchange(other.image_view_, nullptr) }
			, image_available_{ std::exchange(other.image_available_, nullptr) }
			, rendering_finished_{ std::exchange(other.rendering_finished_, nullptr) }
			, fence_{ std::exchange(other.fence_, nullptr) }
			, command_buffer_{ std::exchange(other.command_buffer_, nullptr) } {
		}

		auto operator=(Frame&& other) -> Frame& {
			image_ = std::exchange(other.image_, nullptr);
			image_view_ = std::exchange(other.image_view_, nullptr);
			image_available_ = std::exchange(other.image_available_, nullptr);
			rendering_finished_ = std::exchange(other.rendering_finished_, nullptr);
			fence_ = std::exchange(other.fence_, nullptr);
			command_buffer_ = std::exchange(other.command_buffer_, nullptr);
			return *this;
		}

		static auto construct(const Context& ctx, Image&& image, CommandBuffer&& command_buffer) -> Result<Frame>;

		auto get_image() const -> Image const& { return image_; }
		auto get_image_view() const -> ImageView const& { return image_view_; }

		auto get_image_available_semaphore() const -> Semaphore const& { return image_available_; }
		auto get_rendering_finished_semaphore() const -> Semaphore const& { return rendering_finished_; }
		auto get_fence() const -> Fence const& { return fence_; }
		auto get_command_buffer() const -> CommandBuffer const& { return command_buffer_; }
	private:
		auto _construct(const Context& ctx) -> vk::Result;

		Image image_;
		ImageView image_view_;

		Semaphore image_available_;
		Semaphore rendering_finished_;
		Fence fence_;

		CommandBuffer command_buffer_;
	};

	struct RendererCreateInfo {
		vk::Extent2D extent{ 1u, 1u };
		vk::Format preferred_format{ vk::Format::eUndefined };
		vk::ColorSpaceKHR preferred_color_space{};

		bool enable_hdr{ false };
		bool enable_vsync{ false };
	};

	class Renderer {
	public:
		Renderer() = default;

		Renderer(const Renderer&) = delete;
		auto operator=(const Renderer&) -> Renderer& = delete;

		Renderer(Renderer&& other)
			: context_{ std::exchange(other.context_, nullptr) }
			, queue_{ std::exchange(other.queue_, nullptr) }
			, command_pool_{ std::exchange(other.command_pool_, nullptr) }
			, swapchain_{ std::exchange(other.swapchain_, nullptr) }
			, frames_{ std::exchange(other.frames_, {}) } {
		}

		auto operator=(Renderer&& other) -> Renderer& {
			context_ = std::exchange(other.context_, VK_NULL_HANDLE);
			queue_ = std::exchange(other.queue_, nullptr);
			command_pool_ = std::exchange(other.command_pool_, nullptr);
			swapchain_ = std::exchange(other.swapchain_, nullptr);
			frames_ = std::exchange(other.frames_, {});
			return *this;
		}

		static auto construct(Context&& context, const RendererCreateInfo& create_info) -> Result<std::unique_ptr<Renderer>>;

		auto begin_frame(float delta_time) -> void;
		auto end_frame() -> void;
	private:
		auto _construct(const RendererCreateInfo& create_info) -> vk::Result;

		auto handle_surface_change(bool force = false) -> bool;

		Context context_;
		Queue queue_;
		CommandPool command_pool_;

		Swapchain swapchain_;

		Vector<Frame> frames_{};
		uint32_t active_frame_infex_{ 0u };

		Semaphore const* acquired_senmaphore_{ nullptr };
		Frame* active_frame_{ nullptr };
		float delta_time_{ 0.0f };
	};
}