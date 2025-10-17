#pragma once

#include "gfx_context.h"

#include <future>

namespace edge::gfx {
	constexpr vk::DeviceSize k_uniform_pool_default_block_size{ 4ull * 1024ull * 1024ull };
	constexpr vk::DeviceSize k_staging_allocator_default_block_size{ 64ull * 1024ull * 1024ull };

	class BufferArena : public NonCopyable {
	public:
		BufferArena(Context const* ctx = nullptr)
			: context_{ ctx } {

		}

		BufferArena(BufferArena&& other)
			: context_{ std::exchange(other.context_, nullptr) }
			, buffer_{ std::exchange(other.buffer_, nullptr) }
			, offset_{ other.offset_.load(std::memory_order_relaxed) } {
			other.offset_.store(0ull, std::memory_order_relaxed);
		}

		auto operator=(BufferArena&& other) -> BufferArena& {
			context_ = std::exchange(other.context_, nullptr);
			buffer_ = std::exchange(other.buffer_, nullptr);
			offset_.store(other.offset_.load(std::memory_order_relaxed), std::memory_order_relaxed);
			other.offset_.store(0ull, std::memory_order_relaxed);
			return *this;
		}

		static auto construct(Context const* ctx, vk::DeviceSize size, BufferFlags flags) -> Result<BufferArena>;

		auto allocate(vk::DeviceSize size, vk::DeviceSize alignment = 1ull) -> Result<BufferRange>;
		auto reset() -> void;
		auto is_fits(vk::DeviceSize size, vk::DeviceSize alignment = 1ull) -> bool;

		auto get_buffer() -> Buffer const& { return buffer_; }
	private:
		auto _construct(vk::DeviceSize size, BufferFlags flags) -> vk::Result;
		Context const* context_{ nullptr };

		Buffer buffer_;
		std::atomic<vk::DeviceSize> offset_;
	};

	class ResourceLoader : public NonCopyable {
	public:
		using ImagePromise = std::promise<Result<Image>>;
		using ImageFuture = std::future<Result<Image>>;

		ResourceLoader(Context const* ctx = nullptr)
			: context_{ ctx }
			, staging_arenas_{ ctx ? ctx->get_allocator() : nullptr } {

		}
		~ResourceLoader() {}

		ResourceLoader(ResourceLoader&& other)
			: context_{ std::exchange(other.context_, nullptr) }
			, staging_arenas_{ std::exchange(other.staging_arenas_, {}) } {
		}

		auto operator=(ResourceLoader&& other) -> ResourceLoader& {
			context_ = std::exchange(other.context_, nullptr);
			staging_arenas_ = std::exchange(other.staging_arenas_, {});
			return *this;
		}

		static auto construct(Context const* ctx, vk::DeviceSize arena_size,uint32_t frames_in_flight) -> Result<ResourceLoader>;

		auto load_image_from_memory(Span<uint8_t> image_data) -> Result<Image>;
	private:
		auto _construct(vk::DeviceSize arena_size, uint32_t frames_in_flight) -> vk::Result;

		auto _load_image_from_stbi(Span<uint8_t> image_data) -> Result<Image>;
		auto _load_image_from_ktx(Span<uint8_t> image_data) -> Result<Image>;

		auto _allocate_staging_memory(vk::DeviceSize image_size) -> Result<BufferRange>;

		Context const* context_{ nullptr };
		Vector<BufferArena> staging_arenas_;

	};

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

		static auto construct(const Context& ctx, CommandBuffer&& command_buffer, DescriptorSetLayout const& descriptor_layout) -> Result<Frame>;

		auto begin() -> void;
		auto end() -> void;

		auto get_image_available_semaphore() const -> Semaphore const& { return image_available_; }
		auto get_rendering_finished_semaphore() const -> Semaphore const& { return rendering_finished_; }
		auto get_fence() const -> Fence const& { return fence_; }
		auto get_command_buffer() const -> CommandBuffer const& { return command_buffer_; }
	private:
		auto _construct(const Context& ctx, DescriptorSetLayout const& descriptor_layout) -> vk::Result;

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
		Renderer(Context&& context);
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
			, frame_number_{ std::exchange(other.frame_number_, {}) }
			
			, descriptor_layout_{ std::exchange(other.descriptor_layout_, nullptr) }
			, descriptor_pool_{ std::exchange(other.descriptor_pool_, nullptr) }
			, descriptor_set_{ std::exchange(other.descriptor_set_, nullptr) }
			, pipeline_layout_{ std::exchange(other.pipeline_layout_, nullptr) }
			, push_constant_buffer_{ std::exchange(other.push_constant_buffer_, nullptr) } {
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

			descriptor_layout_ = std::exchange(other.descriptor_layout_, nullptr);
			descriptor_pool_ = std::exchange(other.descriptor_pool_, nullptr);
			descriptor_set_ = std::exchange(other.descriptor_set_, nullptr);
			pipeline_layout_ = std::exchange(other.pipeline_layout_, nullptr);
			push_constant_buffer_ = std::exchange(other.push_constant_buffer_, {});
			return *this;
		}

		static auto construct(Context&& context, const RendererCreateInfo& create_info) -> Result<std::unique_ptr<Renderer>>;

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
		Frame* active_frame_{ nullptr };
		float delta_time_{ 0.0f };
		float gpu_delta_time_{ 0.0f };

		DescriptorSetLayout descriptor_layout_;
		DescriptorPool descriptor_pool_;
		DescriptorSet descriptor_set_;
		PipelineLayout pipeline_layout_;
		Vector<uint8_t> push_constant_buffer_;
	};
}