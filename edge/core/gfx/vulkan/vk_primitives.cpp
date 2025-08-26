#include "vk_context.h"
#include "vk_util.h"

#include <volk.h>

namespace edge::gfx {
	// Semaphore 
	VulkanSemaphore::~VulkanSemaphore() {
		if (handle_) {
			vkDestroySemaphore(device_, handle_, allocator_);
		}
	}

	auto VulkanSemaphore::construct(const VulkanGraphicsContext& ctx, uint64_t initial_value) -> std::unique_ptr<VulkanSemaphore> {
		auto self = std::make_unique<VulkanSemaphore>();
		self->_construct(ctx, initial_value);
		return self;
	}

	auto VulkanSemaphore::signal(uint64_t value) -> SyncResult {
		VkSemaphoreSignalInfo signal_info{ VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO };
		signal_info.semaphore = handle_;
		signal_info.value = value;

		VkResult result = vkSignalSemaphore(device_, &signal_info);
		if (result == VK_SUCCESS) {
			value_ = std::max(value_, value);
			return SyncResult::eSuccess;
		}

		spdlog::error("[VulkanSemaphore]: Failed while signaling semaphore from cpu. Reason: {}.", get_error_string(result));
		return (result == VK_ERROR_DEVICE_LOST) ? SyncResult::eDeviceLost : SyncResult::eError;
	}

	auto VulkanSemaphore::wait(uint64_t value, std::chrono::nanoseconds timeout) -> SyncResult {
		VkSemaphoreWaitInfo wait_info{ VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO };
		wait_info.semaphoreCount = 1;
		wait_info.pSemaphores = &handle_;
		wait_info.pValues = &value;

		uint64_t timeout_ns = (timeout == std::chrono::nanoseconds::max()) ? UINT64_MAX : timeout.count();

		VkResult result = vkWaitSemaphores(device_, &wait_info, timeout_ns);
		switch (result) {
		case VK_SUCCESS: return SyncResult::eSuccess;
		case VK_TIMEOUT: return SyncResult::eTimeout;
		case VK_ERROR_DEVICE_LOST: return SyncResult::eDeviceLost;
		default: {
			spdlog::error("[VulkanSync]: Failed while waiting semaphore on cpu. Reason: {}.", get_error_string(result));
			return SyncResult::eError;
		}
		}
	}

	auto VulkanSemaphore::is_completed(uint64_t value) const -> bool {
		return get_value() >= value;
	}

	auto VulkanSemaphore::get_value() const -> uint64_t {
		uint64_t value;
		VkResult result = vkGetSemaphoreCounterValue(device_, handle_, &value);
		return (result == VK_SUCCESS) ? value : 0;
	}

	auto VulkanSemaphore::_construct(const VulkanGraphicsContext& ctx, uint64_t initial_value) -> bool {
		device_ = ctx.get_logical_device();
		allocator_ = ctx.get_allocation_callbacks();

		VkSemaphoreTypeCreateInfo timeline_create_info{ VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO };
		timeline_create_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
		timeline_create_info.initialValue = value_ = initial_value;

		VkSemaphoreCreateInfo create_info{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
		create_info.pNext = &timeline_create_info;

		VK_CHECK_RESULT(vkCreateSemaphore(device_, &create_info, allocator_, &handle_),
			"Failed to create semaphore.");

		return true;
	}

	// Queue
	VulkanQueue::~VulkanQueue() {
		
	}

	auto VulkanQueue::construct(const VulkanGraphicsContext& ctx, uint32_t family_index, uint32_t queue_index) -> std::unique_ptr<VulkanQueue> {
		auto self = std::make_unique<VulkanQueue>();
		self->_construct(ctx, family_index, queue_index);
		return self;
	}

	auto VulkanQueue::create_command_allocator() const -> std::shared_ptr<IGFXCommandAllocator> {
		return VulkanCommandAllocator::construct(device_, allocator_, family_index_);
	}

	auto VulkanQueue::submit(const SignalQueueInfo& submit_info) -> void {
		std::array<VkSemaphoreSubmitInfo, 16ull> wait_semaphores{};
		std::array<VkSemaphoreSubmitInfo, 16ull> signal_semaphores{};
		std::array<VkCommandBufferSubmitInfo, 16ull> command_buffers{};

		VkSubmitInfo2KHR vk_submit_info{ VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR };
		vk_submit_info.pWaitSemaphoreInfos = wait_semaphores.data();
		vk_submit_info.pSignalSemaphoreInfos = signal_semaphores.data();
		vk_submit_info.pCommandBufferInfos = command_buffers.data();

		for (int32_t i = 0; i < static_cast<int32_t>(submit_info.wait_semaphores.size()); ++i) {
			auto& semaphore_info = submit_info.wait_semaphores[i];
			if (semaphore_info.semaphore) {
				auto& semaphore_submit_info = wait_semaphores[vk_submit_info.waitSemaphoreInfoCount++];
				semaphore_submit_info.semaphore = std::static_pointer_cast<VulkanSemaphore>(semaphore_info.semaphore)->get_handle();
				semaphore_submit_info.value = semaphore_info.value;
				semaphore_submit_info.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
			}
		}

		for (int32_t i = 0; i < static_cast<int32_t>(submit_info.signal_semaphores.size()); ++i) {
			auto& semaphore_info = submit_info.signal_semaphores[i];
			if (semaphore_info.semaphore) {
				auto& semaphore_submit_info = wait_semaphores[vk_submit_info.signalSemaphoreInfoCount++];
				semaphore_submit_info.semaphore = std::static_pointer_cast<VulkanSemaphore>(semaphore_info.semaphore)->get_handle();
				semaphore_submit_info.value = semaphore_info.value;
				semaphore_submit_info.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
			}
		}

		for (int32_t i = 0; i < submit_info.command_lists.size(); ++i) {
			auto& cmd_list_submit_info = command_buffers[vk_submit_info.commandBufferInfoCount++];
			cmd_list_submit_info.commandBuffer = std::static_pointer_cast<VulkanCommandList>(submit_info.command_lists[i])->get_handle();
		}

		VkResult result = vkQueueSubmit2KHR(handle_, 1, &vk_submit_info, VK_NULL_HANDLE);
		if (result == VK_SUCCESS) {
			for (auto& semaphore_submit_info : submit_info.signal_semaphores) {
				auto semaphore = std::static_pointer_cast<VulkanSemaphore>(semaphore_submit_info.semaphore);
				semaphore->set_value(semaphore_submit_info.value);
			}
			return;
		}

		spdlog::error("[VulkanQueue]: Failed while signaling semaphore from gpu. Reason: {}.", get_error_string(result));
	}

	// TODO: Implement present

	auto VulkanQueue::wait_idle() -> SyncResult {
		vkQueueWaitIdle(handle_);
		return SyncResult::eSuccess;
	}

	auto VulkanQueue::_construct(const VulkanGraphicsContext& ctx, uint32_t family_index, uint32_t queue_index) -> bool {
		device_ = ctx.get_logical_device();
		allocator_ = ctx.get_allocation_callbacks();
		family_index_ = family_index;
		queue_index_ = queue_index;

		VkDeviceQueueInfo2 device_queue_info{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2 };
		device_queue_info.queueFamilyIndex = family_index;
		device_queue_info.queueIndex = queue_index;

		vkGetDeviceQueue2(device_, &device_queue_info, &handle_);

		return true;
	}

	// Command allocator
	
	VulkanCommandAllocator::~VulkanCommandAllocator() {
		if (handle_) {
			vkDestroyCommandPool(device_, handle_, allocator_);
		}
	}

	auto VulkanCommandAllocator::construct(VkDevice device, VkAllocationCallbacks const* allocator, uint32_t family_index) -> std::unique_ptr<VulkanCommandAllocator> {
		auto self = std::make_unique<VulkanCommandAllocator>();
		self->_construct(device, allocator, family_index);
		return self;
	}

	auto VulkanCommandAllocator::allocate_command_list() const -> std::shared_ptr<IGFXCommandList> {
		return VulkanCommandList::construct(device_, handle_);
	}

	auto VulkanCommandAllocator::reset() -> void {
		// TODO: Not sure do i need individual reset for this one
	}

	auto VulkanCommandAllocator::_construct(VkDevice device, VkAllocationCallbacks const* allocator, uint32_t family_index) -> bool {
		device_ = device;
		allocator_ = allocator;
		family_index_ = family_index;

		VkCommandPoolCreateInfo create_info{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
		create_info.queueFamilyIndex = family_index_;
		create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

		VK_CHECK_RESULT(vkCreateCommandPool(device_, &create_info, allocator_, &handle_),
			"Failed to create command pool");

		return true;
	}

	// Command list
	VulkanCommandList::~VulkanCommandList() {

	}

	auto VulkanCommandList::construct(VkDevice device, VkCommandPool command_pool) -> std::unique_ptr<VulkanCommandList> {
		auto self = std::make_unique<VulkanCommandList>();
		self->_construct(device, command_pool);
		return self;
	}

	auto VulkanCommandList::reset() -> void {
		assert(false && "NOT IMPLEMENTED");
	}

	auto VulkanCommandList::begin() -> bool {
		
		VkCommandBufferBeginInfo begin_info{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		
		VK_CHECK_RESULT(vkBeginCommandBuffer(handle_, &begin_info),
			"Failed to begin command buffer.");

		return true;
	}

	auto VulkanCommandList::end() -> bool {
		VK_CHECK_RESULT(vkEndCommandBuffer(handle_),
			"Failed to end command buffer.");
		
		return true;
	}

	auto VulkanCommandList::set_viewport(float x, float y, float width, float height, float min_depth, float max_depth) const -> void {
		VkViewport viewport{ x, y, width, height, min_depth, max_depth };
		vkCmdSetViewport(handle_, 0, 1, &viewport);
	}

	auto VulkanCommandList::set_scissor(uint32_t x, uint32_t y, uint32_t width, uint32_t height) const -> void {
		VkRect2D scissor{ { static_cast<int32_t>(x), static_cast<int32_t>(y) }, { width, height } };
		vkCmdSetScissor(handle_, 0, 1, &scissor);
	}

	auto VulkanCommandList::draw(uint32_t vertex_count, uint32_t first_vertex, uint32_t first_instance, uint32_t instance_count) const -> void {
		vkCmdDraw(handle_, vertex_count, instance_count, first_vertex, first_instance);
	}

	auto VulkanCommandList::draw_indexed(uint32_t index_count, uint32_t first_index, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance) const -> void {
		vkCmdDrawIndexed(handle_, index_count, instance_count, first_index, first_vertex, first_instance);
	}

	auto VulkanCommandList::dispatch(uint32_t group_x, uint32_t group_y, uint32_t group_z) const -> void {
		vkCmdDispatch(handle_, group_x, group_y, group_z);
	}

	auto VulkanCommandList::begin_marker(std::string_view name, uint32_t color) const -> void {
		assert(false && "NOT IMPLEMENTED");
	}

	auto VulkanCommandList::insert_marker(std::string_view name, uint32_t color) const -> void {
		assert(false && "NOT IMPLEMENTED");
	}

	auto VulkanCommandList::end_marker() const -> void {
		assert(false && "NOT IMPLEMENTED");
	}

	auto VulkanCommandList::_construct(VkDevice device, VkCommandPool command_pool) -> bool {
		command_pool_ = command_pool;

		VkCommandBufferAllocateInfo allocate_info{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
		allocate_info.commandPool = command_pool;
		allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocate_info.commandBufferCount = 1;

		VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &allocate_info, &handle_), 
			"Failed to allocate command buffers.");

		return true;
	}
}