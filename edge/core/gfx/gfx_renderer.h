#pragma once

#include "gfx_context.h"
#include "gfx_shader_library.h"

#include <future>
#include <variant>

namespace edge::gfx {
	class RenderResource : public NonCopyable {
	public:
		RenderResource() = default;
		~RenderResource();

		RenderResource(RenderResource&& other) noexcept;
		auto operator=(RenderResource&& other) noexcept -> RenderResource&;

		auto setup(Image&& image, ResourceStateFlags initial_flags) -> void;
		auto setup(Buffer&& buffer, ResourceStateFlags initial_flags) -> void;

		template<typename T>
		auto get_handle() -> T& {
			return std::get<T>(resource_handle_);
		}

		template<typename T> 
		auto get_srv_view() -> T& {
			return std::get<T>(srv_view_);
		}

		template<typename T>
		auto get_uav_view(uint32_t mip) -> T& {
			return std::get<T>(uav_views_[mip]);
		}

		auto get_state() const -> const ResourceStateFlags& {
			return resource_state_;
		}

		auto has_handle() const -> bool;
	private:
		static mi::FreeList<uint32_t> srv_free_list_;
		static mi::FreeList<uint32_t> uav_free_list_;

		using HandleType = std::variant<std::monostate, Buffer, Image>;
		using ViewType = std::variant<std::monostate, BufferView, ImageView>;

		HandleType resource_handle_{};

		ViewType srv_view_{};
		uint32_t srv_resource_index_{ ~0u };
		mi::Vector<ViewType> uav_views_{};
		mi::Vector<uint32_t> uav_resource_indices_{};

		ResourceStateFlags resource_state_{};
	};

	class Frame : public NonCopyable {
	public:
		Frame() = default;
		~Frame();

		Frame(Frame&& other)
			: image_available_{ std::exchange(other.image_available_, {}) }
			, rendering_finished_{ std::exchange(other.rendering_finished_, {}) }
			, fence_{ std::exchange(other.fence_, {}) }
			, command_buffer_{ std::exchange(other.command_buffer_, {}) } {
		}

		auto operator=(Frame&& other) -> Frame& {
			image_available_ = std::exchange(other.image_available_, {});
			rendering_finished_ = std::exchange(other.rendering_finished_, {});
			fence_ = std::exchange(other.fence_, {});
			command_buffer_ = std::exchange(other.command_buffer_, {});
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
		Renderer() = default;
		~Renderer();

		Renderer(const Renderer&) = delete;
		auto operator=(const Renderer&) -> Renderer& = delete;

		Renderer(Renderer&& other)
			: queue_{ std::exchange(other.queue_, {}) }
			, command_pool_{ std::exchange(other.command_pool_, {}) }
			, swapchain_{ std::exchange(other.swapchain_, {}) }
			, swapchain_images_{ std::exchange(other.swapchain_images_, {}) }
			, swapchain_image_views_{ std::exchange(other.swapchain_image_views_, {}) }
			, swapchain_image_index_{ std::exchange(other.swapchain_image_index_, {}) }
			, frames_{ std::exchange(other.frames_, {}) }
			, frame_number_{ std::exchange(other.frame_number_, {}) }
			
			, descriptor_layout_{ std::exchange(other.descriptor_layout_, {}) }
			, descriptor_pool_{ std::exchange(other.descriptor_pool_, {}) }
			, descriptor_set_{ std::exchange(other.descriptor_set_, {}) }
			, pipeline_layout_{ std::exchange(other.pipeline_layout_, {}) }
			, push_constant_buffer_{ std::exchange(other.push_constant_buffer_, {}) } 
			, shader_library_{ std::exchange(other.shader_library_, {}) }
			, render_resources_{ std::exchange(other.render_resources_, {}) }
			, render_resource_free_list_{ std::move(other.render_resource_free_list_) } {
		}

		auto operator=(Renderer&& other) -> Renderer& {
			if (this != &other) {
				queue_ = std::exchange(other.queue_, {});
				command_pool_ = std::exchange(other.command_pool_, {});
				swapchain_ = std::exchange(other.swapchain_, {});
				swapchain_images_ = std::exchange(other.swapchain_images_, {});
				swapchain_image_views_ = std::exchange(other.swapchain_image_views_, {});
				swapchain_image_index_ = std::exchange(other.swapchain_image_index_, {});
				frames_ = std::exchange(other.frames_, {});
				frame_number_ = std::exchange(other.frame_number_, {});

				descriptor_layout_ = std::exchange(other.descriptor_layout_, {});
				descriptor_pool_ = std::exchange(other.descriptor_pool_, {});
				descriptor_set_ = std::exchange(other.descriptor_set_, {});
				pipeline_layout_ = std::exchange(other.pipeline_layout_, {});
				push_constant_buffer_ = std::exchange(other.push_constant_buffer_, {});

				shader_library_ = std::exchange(other.shader_library_, {});

				render_resources_ = std::exchange(other.render_resources_, {});
				render_resource_free_list_ = std::move(other.render_resource_free_list_);
			}			
			return *this;
		}

		static auto construct(const RendererCreateInfo& create_info) -> Result<std::unique_ptr<Renderer>>;

		auto create_render_resource() -> uint32_t;
		auto get_render_resource(uint32_t resource_id) -> RenderResource&;

		auto begin_frame(float delta_time) -> void;
		auto end_frame(Span<vk::SemaphoreSubmitInfoKHR> wait_external_semaphores = {}) -> void;

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

		vk::Semaphore acquired_semaphore_{ VK_NULL_HANDLE };
		Frame* active_frame_{ nullptr };
		float delta_time_{ 0.0f };
		float gpu_delta_time_{ 0.0f };

		DescriptorSetLayout descriptor_layout_;
		DescriptorPool descriptor_pool_;
		DescriptorSet descriptor_set_;
		PipelineLayout pipeline_layout_;
		mi::Vector<uint8_t> push_constant_buffer_;

		ShaderLibrary shader_library_;

		mi::Vector<RenderResource> render_resources_{};
		mi::FreeList<uint32_t> render_resource_free_list_{};
	};
}