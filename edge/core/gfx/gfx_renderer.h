#pragma once

#include "gfx_shader_library.h"
#include "gfx_shader_pass.h"

#include <variant>

namespace edge::gfx {
	class Renderer;

	class RenderResource : public NonCopyable {
	public:
		using HandleType = std::variant<std::monostate, Buffer, Image>;
		using ViewType = std::variant<std::monostate, BufferView, ImageView>;
		using DescriptorType = std::variant<std::monostate, vk::DescriptorBufferInfo, vk::DescriptorImageInfo>;
		using BarrierType = std::variant<std::monostate, vk::BufferMemoryBarrier2KHR, vk::ImageMemoryBarrier2KHR>;

		RenderResource() = default;
		RenderResource(Renderer* renderer);
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

		auto make_translation(ResourceStateFlags new_state) -> BarrierType;

		auto transfer_state(CommandBuffer const& cmdbuf, ResourceStateFlags new_state) -> void;
	private:
		Renderer* renderer_{ nullptr };

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
		using ResourceVariant = std::variant<Buffer, Image, BufferView, ImageView>;

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

		static auto construct(CommandBuffer&& command_buffer, DescriptorSetLayout const& descriptor_layout) -> Frame;

		auto begin() -> void;
		auto end() -> void;

		auto enqueue_resource_deletion(ResourceVariant&& resource) -> void {
			deletion_queue_.push_back(std::move(resource));
		}

		auto get_image_available_semaphore() const noexcept -> Semaphore const& { return image_available_; }
		auto get_rendering_finished_semaphore() const noexcept -> Semaphore const& { return rendering_finished_; }
		auto get_fence() const noexcept -> Fence const& { return fence_; }
		auto get_command_buffer() const noexcept -> CommandBuffer const& { return command_buffer_; }
		auto is_recording() const noexcept -> bool { return is_recording_; }
	private:
		auto _construct(DescriptorSetLayout const& descriptor_layout) -> void;

		Semaphore image_available_{};
		Semaphore rendering_finished_{};
		Fence fence_{};

		CommandBuffer command_buffer_{};
		bool is_recording_{ false };

		mi::Vector<ResourceVariant> deletion_queue_{};
	};

	struct RendererCreateInfo {
		vk::Extent2D extent{ 1u, 1u };
		vk::Format preferred_format{ vk::Format::eUndefined };
		vk::ColorSpaceKHR preferred_color_space{};

		bool enable_hdr{ false };
		bool enable_vsync{ false };
		Queue* queue{ nullptr };
	};

	class Renderer : public NonCopyMovable{
	public:
		friend class RenderResource;

		Renderer() = default;
		~Renderer();

		static auto construct(const RendererCreateInfo& create_info) -> Owned<Renderer>;

		auto create_render_resource() -> uint32_t;
		auto setup_render_resource(uint32_t resource_id, Image&& image, ResourceStateFlags initial_state) -> void;
		auto setup_render_resource(uint32_t resource_id, Buffer&& buffer, ResourceStateFlags initial_state) -> void;
		auto get_render_resource(uint32_t resource_id) -> RenderResource&;
		auto destroy_render_resource(uint32_t resource_id) -> void;

		auto add_shader_pass(Owned<IShaderPass>&& pass) -> void;

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

		auto get_pipeline_layout() const noexcept -> PipelineLayout const&;
		auto get_swapchain() const noexcept -> Swapchain const&;
	private:
		auto _construct(const RendererCreateInfo& create_info) -> void;

		auto handle_surface_change(bool force = false) -> bool;
		auto create_swapchain(const Swapchain::State& state) -> void;

		Queue* queue_{ nullptr };
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

		mi::Vector<RenderResource> render_resources_{};
		mi::Vector<Owned<IShaderPass>> shader_passes_{};
		mi::FreeList<uint32_t> render_resource_free_list_{};
	};
}