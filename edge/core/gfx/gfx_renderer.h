#pragma once

#include "gfx_context.h"

namespace edge::gfx {
	class Frame {
	public:
		Frame() = default;
		Frame(const Context& context, Image&& image);
		~Frame();

		Frame(const Frame&) = delete;
		auto operator=(const Frame&) -> Frame& = delete;

		Frame(Frame&& other)
			: image_{ std::exchange(other.image_, nullptr) }
			, image_view_{ std::exchange(other.image_view_, nullptr) }
			, image_available_{ std::exchange(other.image_available_, nullptr) }
			, rendering_finished_{ std::exchange(other.rendering_finished_, nullptr) }
			, fence_{ std::exchange(other.fence_, nullptr) } {
		}

		auto operator=(Frame&& other) -> Frame& {
			image_ = std::exchange(other.image_, nullptr);
			image_view_ = std::exchange(other.image_view_, nullptr);
			image_available_ = std::exchange(other.image_available_, nullptr);
			rendering_finished_ = std::exchange(other.rendering_finished_, nullptr);
			fence_ = std::exchange(other.fence_, nullptr);
			return *this;
		}
	private:
		Image image_;
		ImageView image_view_;

		Semaphore image_available_;
		Semaphore rendering_finished_;
		Fence fence_;
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
			, swapchain_{ std::exchange(other.swapchain_, nullptr) }
			, frames_{ std::exchange(other.frames_, {}) } {
		}

		auto operator=(Renderer&& other) -> Renderer& {
			context_ = std::exchange(other.context_, VK_NULL_HANDLE);
			swapchain_ = std::exchange(other.swapchain_, nullptr);
			frames_ = std::exchange(other.frames_, {});
			return *this;
		}

		static auto construct(Context&& context, const RendererCreateInfo& create_info) -> Result<std::unique_ptr<Renderer>>;
	private:
		auto _construct(const RendererCreateInfo& create_info) -> vk::Result;

		Context context_;
		Swapchain swapchain_;

		FixedVector<Frame> frames_{};
	};
}