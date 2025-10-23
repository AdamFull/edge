#pragma once

#include "gfx_context.h"
#include "gfx_shader_library.h"

#include <future>

namespace edge::gfx {
	class Frame : public NonCopyable {
	public:
		Frame() = default;
		~Frame();

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

		static auto construct(CommandBuffer&& command_buffer, DescriptorSetLayout const& descriptor_layout) -> Result<Frame>;

		auto begin() -> void;
		auto end() -> void;

		auto get_image_available_semaphore() const -> Semaphore const& { return image_available_; }
		auto get_rendering_finished_semaphore() const -> Semaphore const& { return rendering_finished_; }
		auto get_fence() const -> Fence const& { return fence_; }
		auto get_command_buffer() const -> CommandBuffer const& { return command_buffer_; }
	private:
		auto _construct(DescriptorSetLayout const& descriptor_layout) -> vk::Result;

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
		Renderer();
		~Renderer();

		Renderer(const Renderer&) = delete;
		auto operator=(const Renderer&) -> Renderer& = delete;

		Renderer(Renderer&& other)
			: queue_{ std::exchange(other.queue_, nullptr) }
			, command_pool_{ std::exchange(other.command_pool_, nullptr) }
			, swapchain_{ std::exchange(other.swapchain_, nullptr) }
			, swapchain_images_{ std::exchange(other.swapchain_images_, {}) }
			, swapchain_image_views_{ std::exchange(other.swapchain_image_views_, {}) }
			, swapchain_image_index_{ std::exchange(other.swapchain_image_index_, {}) }
			, frames_{ std::exchange(other.frames_, {}) }
			, frame_number_{ std::exchange(other.frame_number_, {}) }
			
			, descriptor_layout_{ std::exchange(other.descriptor_layout_, nullptr) }
			, descriptor_pool_{ std::exchange(other.descriptor_pool_, nullptr) }
			, descriptor_set_{ std::exchange(other.descriptor_set_, nullptr) }
			, pipeline_layout_{ std::exchange(other.pipeline_layout_, nullptr) }
			, push_constant_buffer_{ std::exchange(other.push_constant_buffer_, {}) } 
			, shader_library_{ std::exchange(other.shader_library_, {}) } {
		}

		auto operator=(Renderer&& other) -> Renderer& {
			queue_ = std::exchange(other.queue_, nullptr);
			command_pool_ = std::exchange(other.command_pool_, nullptr);
			swapchain_ = std::exchange(other.swapchain_, nullptr);
			swapchain_images_ = std::exchange(other.swapchain_images_, {});
			swapchain_image_views_ = std::exchange(other.swapchain_image_views_, {});
			swapchain_image_index_ = std::exchange(other.swapchain_image_index_, {});
			frames_ = std::exchange(other.frames_, {});
			frame_number_ = std::exchange(other.frame_number_, {});

			descriptor_layout_ = std::exchange(other.descriptor_layout_, nullptr);
			descriptor_pool_ = std::exchange(other.descriptor_pool_, nullptr);
			descriptor_set_ = std::exchange(other.descriptor_set_, nullptr);
			pipeline_layout_ = std::exchange(other.pipeline_layout_, nullptr);
			push_constant_buffer_ = std::exchange(other.push_constant_buffer_, {});

			shader_library_ = std::exchange(other.shader_library_, {});
			return *this;
		}

		static auto construct(const RendererCreateInfo& create_info) -> Result<std::unique_ptr<Renderer>>;

		auto create_shader(const std::string& shader_path) -> void;

		auto begin_frame(float delta_time) -> void;
		auto end_frame() -> void;

		auto get_current_frame_index() const -> uint32_t { return frame_number_ % k_frame_overlap_; }
		auto get_current_frame() const -> Frame const* { return frames_.data() + get_current_frame_index(); }
		auto get_current_frame() -> Frame* { return frames_.data() + get_current_frame_index(); }

		auto get_gpu_delta_time() const -> float { return gpu_delta_time_; }
	private:
		auto _construct(const RendererCreateInfo& create_info) -> vk::Result;

		auto handle_surface_change(bool force = false) -> bool;
		auto create_swapchain(const Swapchain::State& state) -> vk::Result;

		Queue queue_;
		CommandPool command_pool_;
		QueryPool timestamp_query_;
		double timestamp_frequency_{ 1.0 };

		Swapchain swapchain_;
		mi::Vector<Image> swapchain_images_;
		mi::Vector<ImageView> swapchain_image_views_;
		uint32_t swapchain_image_index_{ 0u };

		mi::Vector<Frame> frames_{};
		uint32_t frame_number_{ 0u };
		static constexpr uint32_t k_frame_overlap_{ 2u };

		vk::Semaphore acquired_senmaphore_{ VK_NULL_HANDLE };
		Frame* active_frame_{ nullptr };
		float delta_time_{ 0.0f };
		float gpu_delta_time_{ 0.0f };

		DescriptorSetLayout descriptor_layout_;
		DescriptorPool descriptor_pool_;
		DescriptorSet descriptor_set_;
		PipelineLayout pipeline_layout_;
		mi::Vector<uint8_t> push_constant_buffer_;

		ShaderLibrary shader_library_;
	};
}