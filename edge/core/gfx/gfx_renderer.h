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
			: image_available_{ std::exchange(other.image_available_, nullptr) }
			, rendering_finished_{ std::exchange(other.rendering_finished_, nullptr) }
			, fence_{ std::exchange(other.fence_, nullptr) }
			, command_buffer_{ std::exchange(other.command_buffer_, nullptr) } {
		}

		auto operator=(Frame&& other) -> Frame& {
			image_available_ = std::exchange(other.image_available_, nullptr);
			rendering_finished_ = std::exchange(other.rendering_finished_, nullptr);
			fence_ = std::exchange(other.fence_, nullptr);
			command_buffer_ = std::exchange(other.command_buffer_, nullptr);
			return *this;
		}

		static auto construct(const Context& ctx, CommandBuffer&& command_buffer) -> Result<Frame>;

		auto get_image_available_semaphore() const -> Semaphore const& { return image_available_; }
		auto get_rendering_finished_semaphore() const -> Semaphore const& { return rendering_finished_; }
		auto get_fence() const -> Fence const& { return fence_; }
		auto get_command_buffer() const -> CommandBuffer const& { return command_buffer_; }
	private:
		auto _construct(const Context& ctx) -> vk::Result;

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
		~Renderer();

		Renderer(const Renderer&) = delete;
		auto operator=(const Renderer&) -> Renderer& = delete;

		Renderer(Renderer&& other)
			: context_{ std::exchange(other.context_, nullptr) }
			, queue_{ std::exchange(other.queue_, nullptr) }
			, command_pool_{ std::exchange(other.command_pool_, nullptr) }
			, swapchain_{ std::exchange(other.swapchain_, nullptr) }
			, swapchain_images_{ std::exchange(other.swapchain_images_, {}) }
			, swapchain_image_views_{ std::exchange(other.swapchain_image_views_, {}) }
			, swapchain_image_index_{ std::exchange(other.swapchain_image_index_, {}) }
			, frames_{ std::exchange(other.frames_, {}) }
			, frame_number_{ std::exchange(other.frame_number_, {}) } {
		}

		auto operator=(Renderer&& other) -> Renderer& {
			context_ = std::exchange(other.context_, VK_NULL_HANDLE);
			queue_ = std::exchange(other.queue_, nullptr);
			command_pool_ = std::exchange(other.command_pool_, nullptr);
			swapchain_ = std::exchange(other.swapchain_, nullptr);
			swapchain_images_ = std::exchange(other.swapchain_images_, {});
			swapchain_image_views_ = std::exchange(other.swapchain_image_views_, {});
			swapchain_image_index_ = std::exchange(other.swapchain_image_index_, {});
			frames_ = std::exchange(other.frames_, {});
			frame_number_ = std::exchange(other.frame_number_, {});
			return *this;
		}

		static auto construct(Context&& context, const RendererCreateInfo& create_info) -> Result<std::unique_ptr<Renderer>>;

		auto begin_frame(float delta_time) -> void;
		auto end_frame() -> void;

		auto get_current_frame_index() const -> uint32_t { return frame_number_ % k_frame_overlap_; }
		auto get_current_frame() const -> Frame const* { return frames_.data() + get_current_frame_index(); }

		auto get_gpu_delta_time() const -> float { return gpu_delta_time_; }
	private:
		auto _construct(const RendererCreateInfo& create_info) -> vk::Result;

		auto handle_surface_change(bool force = false) -> bool;
		auto create_swapchain(const Swapchain::State& state) -> vk::Result;

		Context context_;
		Queue queue_;
		CommandPool command_pool_;
		QueryPool timestamp_query_;
		double timestamp_frequency_{ 1.0 };

		Swapchain swapchain_;
		Vector<Image> swapchain_images_;
		Vector<ImageView> swapchain_image_views_;
		uint32_t swapchain_image_index_{ 0u };

		Vector<Frame> frames_{};
		uint32_t frame_number_{ 0u };
		static constexpr uint32_t k_frame_overlap_{ 2u };

		vk::Semaphore acquired_senmaphore_{ VK_NULL_HANDLE };
		Frame const* active_frame_{ nullptr };
		float delta_time_{ 0.0f };
		float gpu_delta_time_{ 0.0f };
	};
}