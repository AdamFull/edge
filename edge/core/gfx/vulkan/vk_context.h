#pragma once

#include <atomic>
#include <array>
#include <vector>
#include <thread>
#include <mutex>
#include <string_view>
#include <unordered_map>
#include <set>

#include "../gfx_context.h"

#include "vk_wrapper.h"

namespace edge::gfx::vulkan {
	inline auto to_gfx_result(vk::Result vk_result) -> Result {
		switch (vk_result)
		{
		case vk::Result::eSuccess: return Result::eSuccess;
		case vk::Result::eTimeout: return Result::eTimeout;
		case vk::Result::eErrorDeviceLost: return Result::eDeviceLost;
		default: return Result::eUndefined;
		}
	}

	inline auto to_vk_color_space(ColorSpace color_space) -> vk::ColorSpaceKHR {
		switch (color_space) {
		case ColorSpace::eSrgbNonLinear: return vk::ColorSpaceKHR::eSrgbNonlinear;

		case ColorSpace::eRec709NonLinear: return vk::ColorSpaceKHR::eBt709NonlinearEXT;
		case ColorSpace::eRec709Linear: return vk::ColorSpaceKHR::eBt709LinearEXT;

		case ColorSpace::eRec2020Linear: return vk::ColorSpaceKHR::eBt2020LinearEXT;
		case ColorSpace::eRec2020Pq: return vk::ColorSpaceKHR::eHdr10St2084EXT;
		case ColorSpace::eRec2020Hlg: return vk::ColorSpaceKHR::eHdr10HlgEXT;

		case ColorSpace::eDisplayP3NonLinear: return vk::ColorSpaceKHR::eDisplayP3NonlinearEXT;
		case ColorSpace::eDisplayP3Linear: return vk::ColorSpaceKHR::eDisplayP3LinearEXT;

		case ColorSpace::eAdobeRgbNonLinear: return vk::ColorSpaceKHR::eAdobergbNonlinearEXT;
		case ColorSpace::eAdobeRgbLinear: return vk::ColorSpaceKHR::eAdobergbLinearEXT;

		case ColorSpace::ePassThrough: return vk::ColorSpaceKHR::ePassThroughEXT;
		case ColorSpace::eExtendedSrgbLinear: return vk::ColorSpaceKHR::eExtendedSrgbLinearEXT;
		default: return vk::ColorSpaceKHR::eSrgbNonlinear;
		}
	}

	inline auto to_gfx_color_space(vk::ColorSpaceKHR color_space) -> ColorSpace {
		switch (color_space)
		{
		case vk::ColorSpaceKHR::eSrgbNonlinear: return ColorSpace::eSrgbNonLinear;

		case vk::ColorSpaceKHR::eBt709NonlinearEXT: return ColorSpace::eRec709NonLinear;
		case vk::ColorSpaceKHR::eBt709LinearEXT: return ColorSpace::eRec709Linear;

		case vk::ColorSpaceKHR::eBt2020LinearEXT: return ColorSpace::eRec2020Linear;
		case vk::ColorSpaceKHR::eHdr10St2084EXT: return ColorSpace::eRec2020Pq;
		case vk::ColorSpaceKHR::eHdr10HlgEXT: return ColorSpace::eRec2020Hlg;

		case vk::ColorSpaceKHR::eDisplayP3NonlinearEXT: return ColorSpace::eDisplayP3NonLinear;
		case vk::ColorSpaceKHR::eDisplayP3LinearEXT: return ColorSpace::eDisplayP3Linear;

		case vk::ColorSpaceKHR::eAdobergbNonlinearEXT: return ColorSpace::eAdobeRgbNonLinear;
		case vk::ColorSpaceKHR::eAdobergbLinearEXT: return ColorSpace::eAdobeRgbLinear;

		case vk::ColorSpaceKHR::ePassThroughEXT: return ColorSpace::ePassThrough;
		case vk::ColorSpaceKHR::eExtendedSrgbLinearEXT: return ColorSpace::eExtendedSrgbLinear;

		default: return ColorSpace::eSrgbNonLinear;
		}
	}

	inline auto to_vk_queue_type(QueueType type) -> vk::QueueFlagBits {
		switch (type)
		{
		case edge::gfx::QueueType::eDirect: return vk::QueueFlagBits::eGraphics;
		case edge::gfx::QueueType::eCompute: return vk::QueueFlagBits::eCompute;
		case edge::gfx::QueueType::eCopy: return vk::QueueFlagBits::eTransfer;
		default: return vk::QueueFlagBits::eGraphics;
		}
	}

	class GraphicsContext;
	class Queue;

	struct MemoryAllocationDesc {
		size_t size;
		size_t align;
		vk::SystemAllocationScope scope;
        std::thread::id thread_id;
	};

	struct MemoryAllocationStats {
        std::atomic<size_t> total_bytes_allocated;
        std::atomic<size_t> allocation_count;
        std::atomic<size_t> deallocation_count;
        std::mutex mutex;
		std::unordered_map<void*, MemoryAllocationDesc> allocation_map;
	};

	class Semaphore final : public IGFXSemaphore {
	public:
		Semaphore() = default;
		~Semaphore() override;

		Semaphore(const Semaphore&) = delete;
		auto operator=(const Semaphore&) -> Semaphore & = delete;

		Semaphore(Semaphore&& other) 
			: handle_{ std::exchange(other.handle_, VK_NULL_HANDLE) }
			, device_{ std::exchange(other.device_, nullptr) } {}

		auto operator=(Semaphore&& other) -> Semaphore& {
			handle_ = std::exchange(other.handle_, VK_NULL_HANDLE);
			device_ = std::exchange(other.device_, nullptr);
			return *this;
		}

		static auto construct(const GraphicsContext& ctx, uint64_t initial_value) -> GFXResult<Owned<Semaphore>>;

		auto signal(uint64_t value) -> SyncResult override;
		auto wait(uint64_t value, std::chrono::nanoseconds timeout = std::chrono::nanoseconds::max()) -> SyncResult override;

		auto is_completed(uint64_t value) const -> GFXResult<bool> override;
		auto get_value() const -> GFXResult<uint64_t> override;

		auto get_handle() const -> vk::Semaphore {
			return handle_;
		}
	private:
		auto _construct(const GraphicsContext& ctx, uint64_t initial_value) -> Result;

		vkw::Device const* device_{ nullptr };
		vk::Semaphore handle_{ VK_NULL_HANDLE };
	};

	class Queue final : public IGFXQueue {
	public:
		Queue() = default;
		~Queue() override = default;

		Queue(const Queue&) = delete;
		auto operator=(const Queue&) -> Queue & = delete;

		Queue(Queue&& other) 
			: handle_{ std::exchange(other.handle_, VK_NULL_HANDLE) }
			, device_{ std::exchange(other.device_, nullptr) } {}

		auto operator=(Queue&& other) -> Queue& {
			handle_ = std::exchange(other.handle_, VK_NULL_HANDLE);
			device_ = std::exchange(other.device_, nullptr);
			return *this;
		}

		static auto construct(GraphicsContext& ctx, QueueType type) -> GFXResult<Owned<Queue>>;

		auto create_command_allocator() const -> GFXResult<Shared<IGFXCommandAllocator>> override;

		auto submit(const SubmitQueueInfo& submit_info) -> void override;
		auto submit(Span<const vk::SemaphoreSubmitInfo> wait_semaphores, Span<const vk::SemaphoreSubmitInfo> signal_semaphores, Span<const vk::CommandBufferSubmitInfo> command_buffers) -> void;
		auto wait_idle() -> SyncResult override;

		auto get_handle() const -> vkw::Queue const& { return handle_; }
	private:
		auto _construct(GraphicsContext& ctx, QueueType type) -> Result;

		vkw::Device* device_{ nullptr };
		vkw::Queue handle_{ VK_NULL_HANDLE };
	};

	class CommandAllocator final : public IGFXCommandAllocator {
	public:
		CommandAllocator() = default;
		~CommandAllocator() override;

		CommandAllocator(const CommandAllocator&) = delete;
		auto operator=(const CommandAllocator&) -> CommandAllocator & = delete;

		CommandAllocator(CommandAllocator&& other)
			: handle_{ std::exchange(other.handle_, VK_NULL_HANDLE) }
			, device_{ std::exchange(other.device_, nullptr) }
			, family_index_{ std::exchange(other.family_index_, ~0u) } {}

		auto operator=(CommandAllocator&& other) -> CommandAllocator& {
			handle_ = std::exchange(other.handle_, VK_NULL_HANDLE);
			device_ = std::exchange(other.device_, nullptr);
			family_index_ = std::exchange(other.family_index_, ~0u);
			return *this;
		}

		static auto construct(vkw::Device const& device, uint32_t family_index) -> GFXResult<Owned<CommandAllocator>>;

		auto allocate_command_list() const -> GFXResult<Shared<IGFXCommandList>> override;

		auto get_handle() const -> vk::CommandPool {
			return handle_;
		}
	private:
		auto _construct(vkw::Device const& device, uint32_t family_index) -> Result;

		vkw::Device const* device_{ nullptr };
		uint32_t family_index_{ 0u };
		vk::CommandPool handle_{ VK_NULL_HANDLE };
	};

	class CommandList final : public IGFXCommandList {
	public:
		CommandList() = default;
		~CommandList() override;

		CommandList(const CommandList&) = delete;
		auto operator=(const CommandList&) -> CommandList & = delete;

		CommandList(CommandList&& other)
			: handle_{ std::exchange(other.handle_, VK_NULL_HANDLE) }
			, command_pool_{ std::exchange(other.command_pool_, VK_NULL_HANDLE) }
			, device_{ std::exchange(other.device_, nullptr) }{}

		auto operator=(CommandList&& other) -> CommandList& {
			handle_ = std::exchange(other.handle_, VK_NULL_HANDLE);
			command_pool_ = std::exchange(other.command_pool_, VK_NULL_HANDLE);
			device_ = std::exchange(other.device_, nullptr);
			return *this;
		}

		static auto construct(vkw::Device const& device, vk::CommandPool command_pool) -> GFXResult<Owned<CommandList>>;

		auto begin() -> bool override;
		auto end() -> bool override;

		auto set_viewport(float x, float y, float width, float height, float min_depth, float max_depth) const -> void override;
		auto set_scissor(uint32_t x, uint32_t y, uint32_t width, uint32_t height) const -> void override;

		auto draw(uint32_t vertex_count, uint32_t first_vertex, uint32_t first_instance, uint32_t instance_count) const -> void override;
		auto draw_indexed(uint32_t index_count, uint32_t first_index, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance) const -> void override;

		auto dispatch(uint32_t group_x, uint32_t group_y, uint32_t group_z) const -> void override;

		auto begin_marker(std::string_view name, uint32_t color) const -> void override;
		auto insert_marker(std::string_view name, uint32_t color) const -> void override;
		auto end_marker() const -> void override;

		auto get_handle() const -> vk::CommandBuffer { return handle_; }
	private:
		auto _construct(vkw::Device const& device, vk::CommandPool command_pool) -> Result;

		vkw::Device const* device_{ nullptr };

		vk::CommandBuffer handle_{ VK_NULL_HANDLE };
		vk::CommandPool command_pool_{ VK_NULL_HANDLE };
	};

	class PresentationFrame final : public IGFXPresentationFrame {
	public:
		~PresentationFrame() override;

		static auto construct(const GraphicsContext& ctx, Shared<CommandAllocator> cmd_allocator) -> GFXResult<Owned<PresentationFrame>>;

		auto begin() -> bool;
		auto end() -> bool;

		auto get_image() const -> Shared<IGFXImage> override { return nullptr; }
		auto get_image_view() const -> Shared<IGFXImageView> override { return nullptr; }

		auto get_command_list() const -> Shared<IGFXCommandList> override { return command_list_; }

	private:
		auto _construct(const GraphicsContext& ctx, Shared<CommandAllocator> cmd_allocator) -> Result;

		vkw::Device const* device_{ nullptr };

		vk::Semaphore image_available_{ VK_NULL_HANDLE };
		vk::Semaphore rendering_finished_{ VK_NULL_HANDLE };
		vk::Fence fence_{ VK_NULL_HANDLE };

		//Shared<Image> image_;
		//Shared<ImageView> image_view_;

		Shared<CommandList> command_list_;
	};

	class PresentationEngine final : public IGFXPresentationEngine {
	public:
		PresentationEngine() = default;
		~PresentationEngine() override;

		PresentationEngine(const PresentationEngine&) = delete;
		auto operator=(const PresentationEngine&) -> PresentationEngine & = delete;

		PresentationEngine(PresentationEngine&& other)
			: swapchain_{ std::exchange(other.swapchain_, VK_NULL_HANDLE) }
			, context_{ std::exchange(other.context_, nullptr) } {
		}

		auto operator=(PresentationEngine&& other) -> PresentationEngine& {
			swapchain_ = std::exchange(other.swapchain_, VK_NULL_HANDLE);
			context_ = std::exchange(other.context_, nullptr);
			return *this;
		}

		static auto construct(const GraphicsContext& ctx, const PresentationEngineCreateInfo& create_info) -> GFXResult<Owned<PresentationEngine>>;

		auto begin(uint32_t* frame_index) -> bool override;
		auto end(const PresentInfo& present_info) -> bool override;

		auto get_queue() const -> Shared<IGFXQueue> override { return queue_; }
		auto get_command_allocator() const -> Shared<IGFXCommandAllocator> override { return command_allocator_; }

		auto get_current_frame() const -> Shared<IGFXPresentationFrame> override { return presentation_frames_[current_image_]; }
	private:
		auto _construct(const GraphicsContext& ctx, const PresentationEngineCreateInfo& create_info) -> Result;

		GraphicsContext const* context_{ nullptr };
		vkw::Swapchain swapchain_;

		Shared<Queue> queue_;
		Shared<CommandAllocator> command_allocator_;

		uint32_t current_image_{ 0u };
		FixedVector<Shared<PresentationFrame>, 8ull> presentation_frames_;

	};

	class GraphicsContext final : public IGFXContext {
	public:
		GraphicsContext();
		~GraphicsContext() override;

		static auto construct() -> Owned<GraphicsContext>;

		auto create(const GraphicsContextCreateInfo& create_info) -> bool override;

		auto create_queue(QueueType queue_type) const -> GFXResult<Shared<IGFXQueue>> override;
		auto create_semaphore(uint64_t value) const -> GFXResult<Shared<IGFXSemaphore>> override;

		auto create_presentation_engine(const PresentationEngineCreateInfo& create_info) -> GFXResult<Shared<IGFXPresentationEngine>> override;

		auto get_device() const -> vkw::Device const& { return vkw_device_; }
		auto get_device() -> vkw::Device& { return vkw_device_; }
		auto get_allocation_callbacks() const -> vk::AllocationCallbacks const* { return &vk_alloc_callbacks_; }

		auto get_surface() const -> vk::SurfaceKHR { return vk_surface_; }
	private:
		vk::detail::DynamicLoader vk_dynamic_loader_;
		vk::AllocationCallbacks vk_alloc_callbacks_{};
		MemoryAllocationStats memalloc_stats_{};

		vk::Instance vk_instance_{ VK_NULL_HANDLE };
		vk::SurfaceKHR vk_surface_{ VK_NULL_HANDLE };

		vk::DebugUtilsMessengerEXT vk_debug_utils_{ VK_NULL_HANDLE };
		vk::DebugReportCallbackEXT vk_debug_report_{ VK_NULL_HANDLE };

		vkw::Device vkw_device_;
		vkw::MemoryAllocator vkw_memory_allocator_;
	};
}