#pragma once

#include "gfx_shader_library.h"

#include <variant>

namespace edge::gfx {
	class Renderer;

	using PassSetupCb = std::function<void(Renderer&, CommandBuffer const&)>;
	using PassExecuteCb = std::function<void(Renderer&, CommandBuffer const&, float)>;

	struct ShaderPassInfo {
		mi::U8String name{};
		mi::String pipeline_name{};
		PassSetupCb setup_cb{};
		PassExecuteCb execute_cb{};
	};

	class RenderResource : public NonCopyable {
	public:
		using HandleType = std::variant<std::monostate, Buffer, Image>;
		using ViewType = std::variant<std::monostate, BufferView, ImageView>;
		using DescriptorType = std::variant<std::monostate, vk::DescriptorBufferInfo, vk::DescriptorImageInfo>;

		RenderResource() = default;
		~RenderResource();

		RenderResource(RenderResource&& other) noexcept;
		auto operator=(RenderResource&& other) noexcept -> RenderResource&;

		auto setup(Image&& image, ResourceStateFlags initial_flags) -> void;
		auto setup(Buffer&& buffer, ResourceStateFlags initial_flags) -> void;

		auto update(Image&& image, ResourceStateFlags initial_flags) -> void;
		auto update(Buffer&& buffer, ResourceStateFlags initial_flags) -> void;

		auto reset() -> void;

		auto get_descriptor(uint32_t mip = ~0u) const -> DescriptorType;

		template<typename T>
		auto get_handle() -> T& { return std::get<T>(resource_handle_); }

		template<typename T>
		auto get_srv_view() -> T& { return std::get<T>(srv_view_); }

		auto get_state() const -> const ResourceStateFlags& { return state_; }
		auto set_state(ResourceStateFlags new_state) -> void { state_ = new_state; }

		auto get_srv_index() const -> uint32_t { return srv_resource_index_; }
		auto get_uav_index(uint32_t mip) const -> uint32_t { return uav_resource_indices_[mip]; }

		template<typename T>
		auto is_contain_handle() const -> bool {
			return std::holds_alternative<T>(resource_handle_);
		}

		auto has_handle() const -> bool;

		auto transfer_state(CommandBuffer const& cmdbuf, ResourceStateFlags new_state) -> void;
	private:
		static mi::FreeList<uint32_t> srv_free_list_;
		static mi::FreeList<uint32_t> uav_free_list_;

		HandleType resource_handle_{};

		ViewType srv_view_{};
		uint32_t srv_resource_index_{ ~0u };
		mi::Vector<ViewType> uav_views_{};
		mi::Vector<uint32_t> uav_resource_indices_{};

		ResourceStateFlags state_{};
	};

	class Frame : public NonCopyable {
	public:
		Frame() = default;
		~Frame();

		Frame(Frame&& other) noexcept
			: image_available_{ std::move(other.image_available_) }
			, rendering_finished_{ std::move(other.rendering_finished_) }
			, fence_{ std::move(other.fence_) }
			, command_buffer_{ std::move(other.command_buffer_) }
			, is_recording_{ std::exchange(other.is_recording_, false) } {
		}

		auto operator=(Frame&& other) noexcept -> Frame& {
			if (this != &other) {
				image_available_ = std::move(other.image_available_);
				rendering_finished_ = std::move(other.rendering_finished_);
				fence_ = std::move(other.fence_);
				command_buffer_ = std::move(other.command_buffer_);
				is_recording_ = std::exchange(other.is_recording_, false);
			}
			return *this;
		}

		static auto construct(CommandBuffer&& command_buffer, DescriptorSetLayout const& descriptor_layout) -> Result<Frame>;

		auto begin() -> void;
		auto end() -> void;

		auto get_image_available_semaphore() const noexcept -> Semaphore const& { return image_available_; }
		auto get_rendering_finished_semaphore() const noexcept -> Semaphore const& { return rendering_finished_; }
		auto get_fence() const noexcept -> Fence const& { return fence_; }
		auto get_command_buffer() const noexcept -> CommandBuffer const& { return command_buffer_; }
		auto is_recording() const noexcept -> bool { return is_recording_; }
	private:
		auto _construct(DescriptorSetLayout const& descriptor_layout) -> vk::Result;

		Semaphore image_available_{};
		Semaphore rendering_finished_{};
		Fence fence_{};

		CommandBuffer command_buffer_{};
		bool is_recording_{ false };
	};

	struct RendererCreateInfo {
		vk::Extent2D extent{ 1u, 1u };
		vk::Format preferred_format{ vk::Format::eUndefined };
		vk::ColorSpaceKHR preferred_color_space{};

		bool enable_hdr{ false };
		bool enable_vsync{ false };
	};

	class Renderer : public NonCopyable{
	public:
		Renderer() = default;
		~Renderer();

		Renderer(Renderer&& other) noexcept
			: queue_{ std::exchange(other.queue_, {}) }
			, command_pool_{ std::exchange(other.command_pool_, {}) }
			, swapchain_{ std::exchange(other.swapchain_, {}) }
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
			, shader_passes_{ std::exchange(other.shader_passes_, {}) }
			, render_resource_free_list_{ std::move(other.render_resource_free_list_) } {
		}

		auto operator=(Renderer&& other) noexcept -> Renderer& {
			if (this != &other) {
				queue_ = std::exchange(other.queue_, {});
				command_pool_ = std::exchange(other.command_pool_, {});
				swapchain_ = std::exchange(other.swapchain_, {});
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
				shader_passes_ = std::exchange(other.shader_passes_, {});
				render_resource_free_list_ = std::move(other.render_resource_free_list_);
			}			
			return *this;
		}

		static auto construct(const RendererCreateInfo& create_info) -> Result<std::unique_ptr<Renderer>>;

		auto create_render_resource() -> uint32_t;
		auto setup_render_resource(uint32_t resource_id, Image&& image, ResourceStateFlags initial_state) -> void;
		auto setup_render_resource(uint32_t resource_id, Buffer&& buffer, ResourceStateFlags initial_state) -> void;
		auto get_render_resource(uint32_t resource_id) -> RenderResource&;

		auto set_render_area(vk::Rect2D render_area) -> void;
		auto set_layer_count(uint32_t layer_count) -> void;
		auto add_color_attachment(uint32_t resource_id, vk::AttachmentLoadOp load_op = vk::AttachmentLoadOp::eClear, vk::ClearColorValue clear_color = {}) -> void;

		auto add_shader_pass(ShaderPassInfo&& shader_pass_info) -> void;

		auto begin_frame(float delta_time) -> void;
		auto execute_graph(float delta_time) -> void;
		auto end_frame(Span<vk::SemaphoreSubmitInfoKHR> wait_external_semaphores = {}) -> void;

		auto get_current_frame_index() const -> uint32_t { return frame_number_ % k_frame_overlap_; }
		auto get_current_frame() const -> Frame const* { return frames_.data() + get_current_frame_index(); }
		auto get_current_frame() -> Frame* { return frames_.data() + get_current_frame_index(); }

		auto get_backbuffer_resource_id() -> uint32_t;
		auto get_backbuffer_resource() -> RenderResource&;

		auto get_gpu_delta_time() const -> float { return gpu_delta_time_; }

		auto push_constant_range(CommandBuffer const& cmd, vk::ShaderStageFlags stage_flags, Span<const uint8_t> range) const -> void;
	private:
		auto _construct(const RendererCreateInfo& create_info) -> vk::Result;

		auto handle_surface_change(bool force = false) -> bool;
		auto create_swapchain(const Swapchain::State& state) -> vk::Result;

		Queue queue_;
		CommandPool command_pool_;
		QueryPool timestamp_query_;
		double timestamp_frequency_{ 1.0 };

		Swapchain swapchain_;
		mi::Vector<uint32_t> swapchain_targets_;
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
		mi::Vector<vk::WriteDescriptorSet> write_descriptor_sets_{};
		mi::Vector<vk::DescriptorImageInfo> image_descriptors_{};
		mi::Vector<vk::DescriptorBufferInfo> buffer_descriptors_{};
		Sampler test_sampler_{};

		ShaderLibrary shader_library_;

		mi::Vector<RenderResource> render_resources_{};
		mi::Vector<ShaderPassInfo> shader_passes_{};
		mi::FreeList<uint32_t> render_resource_free_list_{};

		// TODO: This is temporary
		mi::Vector<ImageBarrier> image_barriers_{};
		vk::Rect2D render_area_{};
		uint32_t layer_count_{ 1u };
		mi::Vector<vk::RenderingAttachmentInfo> color_attachments_{};
		vk::RenderingAttachmentInfo depth_attachment_{};
		vk::RenderingAttachmentInfo stencil_attachment_{};
	};
}