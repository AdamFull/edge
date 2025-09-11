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

		static auto construct(const GraphicsContext& ctx, uint64_t initial_value) -> Owned<Semaphore>;

		auto signal(uint64_t value) -> SyncResult override;
		auto wait(uint64_t value, std::chrono::nanoseconds timeout = std::chrono::nanoseconds::max()) -> SyncResult override;

		auto is_completed(uint64_t value) const -> bool override;
		auto get_value() const -> uint64_t override;

		auto get_handle() const -> vk::Semaphore {
			return handle_;
		}
	private:
		auto _construct(const GraphicsContext& ctx, uint64_t initial_value) -> bool;

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
			, device_{ std::exchange(other.device_, nullptr) }
			, family_index_{ std::exchange(other.family_index_, ~0u) }
			, queue_index_{ std::exchange(other.queue_index_, ~0u) } {}

		auto operator=(Queue&& other) -> Queue& {
			handle_ = std::exchange(other.handle_, VK_NULL_HANDLE);
			device_ = std::exchange(other.device_, nullptr);
			family_index_ = std::exchange(other.family_index_, ~0u);
			queue_index_ = std::exchange(other.queue_index_, ~0u);
			return *this;
		}

		static auto construct(const GraphicsContext& ctx, uint32_t family_index, uint32_t queue_index) -> Owned<Queue>;

		auto create_command_allocator() const -> Shared<IGFXCommandAllocator> override;

		auto submit(const SubmitQueueInfo& submit_info) -> void override;
		auto submit(Span<const vk::SemaphoreSubmitInfo> wait_semaphores, Span<const vk::SemaphoreSubmitInfo> signal_semaphores, Span<const vk::CommandBufferSubmitInfo> command_buffers) -> void;
		auto wait_idle() -> SyncResult override;

		auto get_handle() const -> vk::Queue { return handle_; }
		auto get_family_index() const -> uint32_t { return family_index_; }
	private:
		auto _construct(const GraphicsContext& ctx, uint32_t family_index, uint32_t queue_index) -> bool;

		vkw::Device const* device_{ nullptr };

		vk::Queue handle_{ VK_NULL_HANDLE };
		uint32_t family_index_{ 0u };
		uint32_t queue_index_{ 0u };
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

		static auto construct(vkw::Device const& device, uint32_t family_index) -> Owned<CommandAllocator>;

		auto allocate_command_list() const -> Shared<IGFXCommandList> override;
		auto reset() -> void override;

		auto get_handle() const -> vk::CommandPool {
			return handle_;
		}
	private:
		auto _construct(vkw::Device const& device, uint32_t family_index) -> bool;

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

		static auto construct(vkw::Device const& device, vk::CommandPool command_pool) -> Owned<CommandList>;

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
		auto _construct(vkw::Device const& device, vk::CommandPool command_pool) -> bool;

		vkw::Device const* device_{ nullptr };

		vk::CommandBuffer handle_{ VK_NULL_HANDLE };
		vk::CommandPool command_pool_{ VK_NULL_HANDLE };
	};

	class PresentationFrame final : public IGFXPresentationFrame {
	public:
		~PresentationFrame() override;
	private:
		vkw::Device const* device_{ nullptr };

		vk::Semaphore image_available_{ VK_NULL_HANDLE };
		vk::Semaphore rendering_finished_{ VK_NULL_HANDLE };
		vk::Fence fence_{ VK_NULL_HANDLE };
	};

	class PresentationEngine final : public IGFXPresentationEngine {
	public:
		~PresentationEngine() override;

		static auto construct(const GraphicsContext& ctx, const PresentationEngineCreateInfo& create_info) -> Owned<PresentationEngine>;

		auto get_current_image_index() const -> uint32_t override;
		auto get_current_image() const -> Shared<IGFXImage> override;

		auto acquire_next_image(uint32_t* next_image_index) -> bool override;

		auto reset() -> bool override;
	private:
		auto _construct(const GraphicsContext& ctx, const PresentationEngineCreateInfo& create_info) -> bool;
		auto get_prev_frame_index() const -> uint32_t;

		vkw::Device const* device_{ nullptr };
		vkw::Swapchain swapchain_;

		uint32_t current_image_{ 0u };

	};

	//class VulkanPresentationEngine final : public IGFXPresentationEngine {
	//public:
	//	class Frame {
	//	public:
	//		~Frame();
	//
	//		//Shared<VulkanImage> image_;
	//		//Shared<VulkanImageView> image_view_;
	//
	//		vk::Semaphore image_available_{ VK_NULL_HANDLE };
	//		vk::Semaphore execution_finished_{ VK_NULL_HANDLE };
	//		vk::Fence fence_{ VK_NULL_HANDLE };
	//	};
	//
	//	~VulkanPresentationEngine() override;
	//
	//	static auto construct(const GraphicsContext& ctx, QueueType queue_type, uint32_t frames_in_flight) -> Owned<VulkanPresentationEngine>;
	//
	//	auto get_queue() const -> Shared<IGFXQueue> override {
	//		return queue_;
	//	}
	//
	//	auto next_frame() -> void override;
	//	auto present(const PresentInfo& present_info) -> void override;
	//
	//	auto get_frame_index() const -> uint32_t override;
	//private:
	//	auto _construct(const GraphicsContext& ctx, QueueType queue_type, uint32_t frames_in_flight) -> bool;
	//	auto update_swapchain() -> bool;
	//
	//	vk::Device device_{ VK_NULL_HANDLE };
	//	vk::PhysicalDevice physical_{ VK_NULL_HANDLE };
	//	vk::SurfaceKHR surface_{ VK_NULL_HANDLE };
	//	vk::AllocationCallbacks const* allocator_{ nullptr };
	//	vk::SwapchainKHR handle_{ VK_NULL_HANDLE };
	//	vk::SwapchainCreateInfoKHR swapchain_create_info;
	//
	//	Shared<VulkanQueue> queue_;
	//	Shared<VulkanCommandAllocator> command_allocator_;
	//	std::vector<Shared<IGFXCommandList>> command_lists_;
	//
	//	// Swapchain settings
	//	bool vsync_{ false };
	//	uint32_t requested_image_count_{ 1u };
	//	VkExtent2D requested_extent_{ 1u, 1u };
	//	VkSurfaceTransformFlagBitsKHR requested_transform_{ VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR };
	//};

	class GraphicsContext final : public IGFXContext {
	public:
		~GraphicsContext() override;

		static auto construct() -> Owned<GraphicsContext>;

		auto create(const GraphicsContextCreateInfo& create_info) -> bool override;

		auto create_queue(QueueType queue_type) const -> Shared<IGFXQueue> override;
		auto create_semaphore(uint64_t value) const -> Shared<IGFXSemaphore> override;

		auto create_presentation_engine(const PresentationEngineCreateInfo& create_info) -> Shared<IGFXPresentationEngine> override;

		auto get_device() const -> vkw::Device const& { return vkw_device_; }
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

        VmaAllocator vma_allocator_{ VK_NULL_HANDLE };
	};
}