#include "vk_context.h"

namespace edge::gfx {
	// Helper functions
	auto choose_extent(vk::Extent2D request_extent, const vk::Extent2D& min_image_extent,
		const vk::Extent2D& max_image_extent, const vk::Extent2D& current_extent) -> vk::Extent2D {
		if (current_extent.width == 0xFFFFFFFF) {
			return request_extent;
		}

		if (request_extent.width < 1 || request_extent.height < 1) {
			spdlog::warn("[Vulkan Presentation Engine] Image extent ({}, {}) not supported. Selecting ({}, {}).",
				request_extent.width, request_extent.height,
				current_extent.width, current_extent.height);
			return current_extent;
		}

		request_extent.width = std::clamp(request_extent.width, min_image_extent.width, max_image_extent.width);
		request_extent.height = std::clamp(request_extent.height, min_image_extent.height, max_image_extent.height);

		return request_extent;
	}

	auto choose_present_mode(vk::PresentModeKHR request_present_mode, std::span<const vk::PresentModeKHR> available_present_modes,
		std::span<const vk::PresentModeKHR> present_mode_priority_list) -> vk::PresentModeKHR {
		// Try to find the requested present mode in the available present modes
		auto const present_mode_it = std::find(available_present_modes.begin(), available_present_modes.end(), request_present_mode);
		if (present_mode_it == available_present_modes.end()) {
			// If the requested present mode isn't found, then try to find a mode from the priority list
			auto const chosen_present_mode_it =
				std::find_if(present_mode_priority_list.begin(), present_mode_priority_list.end(),
					[&available_present_modes](vk::PresentModeKHR present_mode) {
						return std::find(available_present_modes.begin(), available_present_modes.end(), present_mode) != available_present_modes.end();
					});

			// If nothing found, always default to FIFO
			vk::PresentModeKHR const chosen_present_mode = (chosen_present_mode_it != present_mode_priority_list.end()) ? *chosen_present_mode_it : vk::PresentModeKHR::eFifo;

			spdlog::warn("[VkSwapchain] Present mode '{}' not supported. Selecting '{}'.", vk::to_string(request_present_mode), vk::to_string(chosen_present_mode));
			return chosen_present_mode;
		}
		else {
			spdlog::debug("[VkSwapchain] Present mode selected: {}", vk::to_string(request_present_mode));
			return request_present_mode;
		}
	}

	auto choose_surface_format(const vk::SurfaceFormatKHR requested_surface_format, std::span<const vk::SurfaceFormatKHR> available_surface_formats,
		std::span<const vk::SurfaceFormatKHR> surface_format_priority_list) -> vk::SurfaceFormatKHR {
		// Try to find the requested surface format in the available surface formats
		auto const surface_format_it = std::find(available_surface_formats.begin(), available_surface_formats.end(), requested_surface_format);
		
		// If the requested surface format isn't found, then try to request a format from the priority list
		if (surface_format_it == available_surface_formats.end()) {
			auto const chosen_surface_format_it =
				std::find_if(surface_format_priority_list.begin(), surface_format_priority_list.end(),
					[&available_surface_formats](vk::SurfaceFormatKHR surface_format) {
						return std::find(available_surface_formats.begin(), available_surface_formats.end(), surface_format) != available_surface_formats.end();
					});
		
			// If nothing found, default to the first available format
			vk::SurfaceFormatKHR const& chosen_surface_format = (chosen_surface_format_it != surface_format_priority_list.end()) ? *chosen_surface_format_it : available_surface_formats[0];
		
			spdlog::warn("[VkSwapchain] Surface format ({}) not supported. Selecting ({}).",
				vk::to_string(requested_surface_format.format) + ", " + vk::to_string(requested_surface_format.colorSpace),
				vk::to_string(chosen_surface_format.format) + ", " + vk::to_string(chosen_surface_format.colorSpace));
			return chosen_surface_format;
		}
		else {
			spdlog::debug("[VkSwapchain] Surface format selected: {}", vk::to_string(requested_surface_format.format) + ", " + vk::to_string(requested_surface_format.colorSpace));
			return requested_surface_format;
		}
		return {};
	}

	auto choose_transform(vk::SurfaceTransformFlagBitsKHR request_transform, vk::SurfaceTransformFlagsKHR supported_transform,
		vk::SurfaceTransformFlagBitsKHR current_transform) -> vk::SurfaceTransformFlagBitsKHR {
		if (request_transform & supported_transform) {
			return request_transform;
		}

		spdlog::warn("[VkSwapchain] Surface transform '{}' not supported. Selecting '{}'.", vk::to_string(request_transform), vk::to_string(current_transform));
		return current_transform;
	}

	auto choose_composite_alpha(vk::CompositeAlphaFlagBitsKHR request_composite_alpha,
		vk::CompositeAlphaFlagsKHR supported_composite_alpha) -> vk::CompositeAlphaFlagBitsKHR {
		if (request_composite_alpha & supported_composite_alpha) {
			return request_composite_alpha;
		}

		static const std::array<vk::CompositeAlphaFlagBitsKHR, 4ull> composite_alpha_priority_list = {
			vk::CompositeAlphaFlagBitsKHR::eOpaque, vk::CompositeAlphaFlagBitsKHR::ePreMultiplied,
			vk::CompositeAlphaFlagBitsKHR::ePostMultiplied, vk::CompositeAlphaFlagBitsKHR::eInherit
		};

		auto const chosen_composite_alpha_it = std::find_if(composite_alpha_priority_list.begin(), composite_alpha_priority_list.end(),
			[&supported_composite_alpha](vk::CompositeAlphaFlagBitsKHR composite_alpha) { return composite_alpha & supported_composite_alpha; });

		if (chosen_composite_alpha_it == composite_alpha_priority_list.end()) {
			spdlog::error("[VkSwapchain] No compatible composite alpha found.");
		}
		else {
			spdlog::warn("[VkSwapchain] Composite alpha '{}' not supported. Selecting '{}.", vk::to_string(request_composite_alpha), vk::to_string(*chosen_composite_alpha_it));
			return *chosen_composite_alpha_it;
		}

		return {};
	}

	auto validate_format_feature(vk::ImageUsageFlagBits image_usage, vk::FormatFeatureFlags supported_features) -> bool {
		return (image_usage != vk::ImageUsageFlagBits::eStorage) || ((supported_features & vk::FormatFeatureFlagBits::eStorageImage) == vk::FormatFeatureFlagBits::eStorageImage);
	}

	auto choose_image_usage(const std::set<vk::ImageUsageFlagBits>& requested_image_usage_flags,
		vk::ImageUsageFlags supported_image_usage, vk::FormatFeatureFlags supported_features) -> std::set<vk::ImageUsageFlagBits> {
		std::set<vk::ImageUsageFlagBits> validated_image_usage_flags;
		for (auto flag : requested_image_usage_flags) {
			if ((flag & supported_image_usage) && validate_format_feature(flag, supported_features)) {
				validated_image_usage_flags.insert(flag);
			}
			else {
				spdlog::warn("[VkSwapchain] Image usage ({}) requested but not supported.", vk::to_string(flag));
			}
		}

		if (validated_image_usage_flags.empty()) {
			// Pick the first format from list of defaults, if supported
			static const std::array<vk::ImageUsageFlagBits, 4ull> image_usage_priority_list = {
				vk::ImageUsageFlagBits::eColorAttachment, vk::ImageUsageFlagBits::eStorage, vk::ImageUsageFlagBits::eSampled, vk::ImageUsageFlagBits::eTransferDst
			};

			auto const priority_list_it = std::find_if(image_usage_priority_list.begin(), image_usage_priority_list.end(),
				[&supported_image_usage, &supported_features](auto const image_usage) {
					return ((image_usage & supported_image_usage) && validate_format_feature(image_usage, supported_features));
				});

			if (priority_list_it != image_usage_priority_list.end()) {
				validated_image_usage_flags.insert(*priority_list_it);
			}
		}

		if (validated_image_usage_flags.empty()) {
			spdlog::error("[VkSwapchain] No compatible image usage found.");
		}
		else {
			// Log image usage flags used
			std::string usage_list;
			for (vk::ImageUsageFlagBits image_usage : validated_image_usage_flags) {
				usage_list += to_string(image_usage) + " ";
			}
			spdlog::debug("[VkSwapchain] Image usage flags: {}", usage_list);
		}

		return validated_image_usage_flags;
	}

	auto composite_image_flags(std::set<vk::ImageUsageFlagBits>& image_usage_flags) -> vk::ImageUsageFlags {
		vk::ImageUsageFlags image_usage;
		for (auto flag : image_usage_flags) {
			image_usage |= flag;
		}
		return image_usage;
	}

	// Semaphore 
	auto VulkanSemaphore::construct(const VulkanGraphicsContext& ctx, uint64_t initial_value) -> std::unique_ptr<VulkanSemaphore> {
		auto self = std::make_unique<VulkanSemaphore>();
		self->_construct(ctx, initial_value);
		return self;
	}

	auto VulkanSemaphore::signal(uint64_t value) -> SyncResult {
		vk::SemaphoreSignalInfo signal_info{};
		signal_info.semaphore = handle_;
		signal_info.value = value;

		vk::Result result = device_.signalSemaphore(&signal_info);
		if (result == vk::Result::eSuccess) {
			value_ = std::max(value_, value);
			return SyncResult::eSuccess;
		}

		spdlog::error("[VulkanSemaphore]: Failed while signaling semaphore from cpu. Reason: {}.", vk::to_string(result));
		return (result == vk::Result::eErrorDeviceLost) ? SyncResult::eDeviceLost : SyncResult::eError;
	}

	auto VulkanSemaphore::wait(uint64_t value, std::chrono::nanoseconds timeout) -> SyncResult {
		vk::SemaphoreWaitInfo wait_info{};
		wait_info.semaphoreCount = 1;
		wait_info.pSemaphores = &handle_;
		wait_info.pValues = &value;

		uint64_t timeout_ns = (timeout == std::chrono::nanoseconds::max()) ? UINT64_MAX : timeout.count();

		vk::Result result = device_.waitSemaphores(&wait_info, timeout_ns);
		switch (result) {
		case vk::Result::eSuccess: return SyncResult::eSuccess;
		case vk::Result::eTimeout: return SyncResult::eTimeout;
		case vk::Result::eErrorDeviceLost: return SyncResult::eDeviceLost;
		default: {
			spdlog::error("[VulkanSync]: Failed while waiting semaphore on cpu. Reason: {}.", vk::to_string(result));
			return SyncResult::eError;
		}
		}
	}

	auto VulkanSemaphore::is_completed(uint64_t value) const -> bool {
		return get_value() >= value;
	}

	auto VulkanSemaphore::get_value() const -> uint64_t {
		uint64_t value;
		vk::Result result = device_.getSemaphoreCounterValue(handle_, &value);
		return (result == vk::Result::eSuccess) ? value : 0;
	}

	auto VulkanSemaphore::_construct(const VulkanGraphicsContext& ctx, uint64_t initial_value) -> bool {
		//device_ = ctx.get_logical_device();
		allocator_ = ctx.get_allocation_callbacks();

		VkSemaphoreTypeCreateInfo timeline_create_info{ VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO };
		timeline_create_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
		timeline_create_info.initialValue = value_ = initial_value;

		VkSemaphoreCreateInfo create_info{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
		create_info.pNext = &timeline_create_info;

		//VK_CHECK_RESULT(vkCreateSemaphore(device_, &create_info, allocator_, &handle_),
		//	"Failed to create semaphore.");

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

	auto VulkanQueue::submit(const SubmitQueueInfo& submit_info) -> void {
		std::array<vk::SemaphoreSubmitInfo, 16ull> wait_semaphores{};
		std::array<vk::SemaphoreSubmitInfo, 16ull> signal_semaphores{};
		std::array<vk::CommandBufferSubmitInfo, 16ull> command_buffers{};

		vk::SubmitInfo2KHR vk_submit_info{};
		vk_submit_info.pWaitSemaphoreInfos = wait_semaphores.data();
		vk_submit_info.pSignalSemaphoreInfos = signal_semaphores.data();
		vk_submit_info.pCommandBufferInfos = command_buffers.data();

		for (int32_t i = 0; i < static_cast<int32_t>(submit_info.wait_semaphores.size()); ++i) {
			auto& semaphore_info = submit_info.wait_semaphores[i];
			if (semaphore_info.semaphore) {
				auto& semaphore_submit_info = wait_semaphores[vk_submit_info.waitSemaphoreInfoCount++];
				semaphore_submit_info.semaphore = std::static_pointer_cast<VulkanSemaphore>(semaphore_info.semaphore)->get_handle();
				semaphore_submit_info.value = semaphore_info.value;
				semaphore_submit_info.stageMask = vk::PipelineStageFlagBits2::eAllCommands;
			}
		}

		for (int32_t i = 0; i < static_cast<int32_t>(submit_info.signal_semaphores.size()); ++i) {
			auto& semaphore_info = submit_info.signal_semaphores[i];
			if (semaphore_info.semaphore) {
				auto& semaphore_submit_info = wait_semaphores[vk_submit_info.signalSemaphoreInfoCount++];
				semaphore_submit_info.semaphore = std::static_pointer_cast<VulkanSemaphore>(semaphore_info.semaphore)->get_handle();
				semaphore_submit_info.value = semaphore_info.value;
				semaphore_submit_info.stageMask = vk::PipelineStageFlagBits2::eAllCommands;
			}
		}

		for (int32_t i = 0; i < submit_info.command_lists.size(); ++i) {
			auto& cmd_list_submit_info = command_buffers[vk_submit_info.commandBufferInfoCount++];
			cmd_list_submit_info.commandBuffer = std::static_pointer_cast<VulkanCommandList>(submit_info.command_lists[i])->get_handle();
		}

		vk::Result result = handle_.submit2(1, &vk_submit_info, VK_NULL_HANDLE);
		if (result == vk::Result::eSuccess) {
			for (auto& semaphore_submit_info : submit_info.signal_semaphores) {
				auto semaphore = std::static_pointer_cast<VulkanSemaphore>(semaphore_submit_info.semaphore);
				semaphore->set_value(semaphore_submit_info.value);
			}
			return;
		}

		spdlog::error("[VulkanQueue]: Failed while signaling semaphore from gpu. Reason: {}.", vk::to_string(result));
	}

	// TODO: Implement present

	auto VulkanQueue::wait_idle() -> SyncResult {
		vkQueueWaitIdle(handle_);
		return SyncResult::eSuccess;
	}

	auto VulkanQueue::_construct(const VulkanGraphicsContext& ctx, uint32_t family_index, uint32_t queue_index) -> bool {
		//device_ = ctx.get_logical_device();
		allocator_ = ctx.get_allocation_callbacks();
		family_index_ = family_index;
		queue_index_ = queue_index;

		vk::DeviceQueueInfo2 device_queue_info{};
		device_queue_info.queueFamilyIndex = family_index;
		device_queue_info.queueIndex = queue_index;

		device_.getQueue2(&device_queue_info, &handle_);

		return true;
	}

	// Command allocator
	
	VulkanCommandAllocator::~VulkanCommandAllocator() {
		if (handle_) {
			device_.destroyCommandPool(handle_, allocator_);
		}
	}

	auto VulkanCommandAllocator::construct(vk::Device device, vk::AllocationCallbacks const* allocator, uint32_t family_index) -> std::unique_ptr<VulkanCommandAllocator> {
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

	auto VulkanCommandAllocator::_construct(vk::Device device, vk::AllocationCallbacks const* allocator, uint32_t family_index) -> bool {
		device_ = device;
		allocator_ = allocator;
		family_index_ = family_index;

		VkCommandPoolCreateInfo create_info{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
		create_info.queueFamilyIndex = family_index_;
		create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

		//VK_CHECK_RESULT(vkCreateCommandPool(device_, &create_info, allocator_, &handle_),
		//	"Failed to create command pool");

		return true;
	}

	// Command list
	VulkanCommandList::~VulkanCommandList() {

	}

	auto VulkanCommandList::construct(vk::Device device, vk::CommandPool command_pool) -> std::unique_ptr<VulkanCommandList> {
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
		
		//VK_CHECK_RESULT(vkBeginCommandBuffer(handle_, &begin_info),
		//	"Failed to begin command buffer.");

		return true;
	}

	auto VulkanCommandList::end() -> bool {
		//VK_CHECK_RESULT(vkEndCommandBuffer(handle_),
		//	"Failed to end command buffer.");
		
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

	auto VulkanCommandList::_construct(vk::Device device, vk::CommandPool command_pool) -> bool {
		command_pool_ = command_pool;

		VkCommandBufferAllocateInfo allocate_info{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
		allocate_info.commandPool = command_pool;
		allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocate_info.commandBufferCount = 1;

		//VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &allocate_info, &handle_), 
		//	"Failed to allocate command buffers.");

		return true;
	}

	// Swapchain
	VulkanSwapchain::~VulkanSwapchain() {
		if (handle_) {
			device_.destroySwapchainKHR(handle_, allocator_);
		}
	}

	auto VulkanSwapchain::construct(const VulkanGraphicsContext& ctx, const SwapchainCreateInfo& create_info) -> std::unique_ptr<VulkanSwapchain> {
		auto self = std::make_unique<VulkanSwapchain>();
		self->_construct(ctx, create_info);
		return self;
	}

	auto VulkanSwapchain::get_current_image_index() const -> uint32_t {
		return current_image_;
	}

	auto VulkanSwapchain::get_current_image() const -> std::shared_ptr<IGFXImage> {
		return nullptr;
	}

	auto VulkanSwapchain::acquire_next_image(uint32_t* next_image_index) -> bool {
		auto prev_frame_index = get_prev_frame_index();
		auto& prev_frame = frames_in_flight_[prev_frame_index];
		
		// Wait end of 
		//VK_CHECK_RESULT(vkWaitForFences(device_, 1u, &prev_frame.fence_, VK_TRUE, UINT64_MAX),
		//	"Failed waiting on previous frame fence.");

		VkAcquireNextImageInfoKHR acquire_info{ VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR };
		//acquire_info.semaphore = current_frame.image_available_;


		vkAcquireNextImage2KHR(device_, &acquire_info, &current_image_);

		if (next_image_index) {
			*next_image_index = current_image_;
		}

		return false;
	}

	auto VulkanSwapchain::reset() -> bool {
		return false;
	}
	
	auto VulkanSwapchain::_construct(const VulkanGraphicsContext& ctx, const SwapchainCreateInfo& create_info) -> bool {
		return false;
	}

	auto VulkanSwapchain::get_prev_frame_index() const -> uint32_t {
		return current_image_ == 0u ? create_info_.minImageCount - 1u : current_image_ - 1u;
	}


	// Presentation engine
//	auto VulkanPresentationEngine::construct(const VulkanGraphicsContext& ctx, QueueType queue_type, uint32_t frames_in_flight) -> std::unique_ptr<VulkanPresentationEngine> {
//		auto self = std::make_unique<VulkanPresentationEngine>();
//		self->_construct(ctx, queue_type, frames_in_flight);
//		return self;
//	}
//
//	auto VulkanPresentationEngine::next_frame() -> void {
//
//	}
//
//	auto VulkanPresentationEngine::present(const PresentInfo& present_info) -> void {
//		// End all command lists 
//		for (auto& command_list : command_lists_) {
//			command_list->end();
//		}
//
//		SubmitQueueInfo submit_info{};
//		submit_info.command_lists = command_lists_;
//		submit_info.wait_semaphores = present_info.wait_semaphores;
//		submit_info.signal_semaphores = present_info.signal_semaphores;
//		queue_->submit(submit_info);
//
//
//	}
//
//	auto VulkanPresentationEngine::get_frame_index() const -> uint32_t {
//		return 0;
//	}
//
//	auto VulkanPresentationEngine::_construct(const VulkanGraphicsContext& ctx, QueueType queue_type, uint32_t frames_in_flight) -> bool {
//		device_ = ctx.get_logical_device();
//		physical_ = ctx.get_physical_device();
//		surface_ = ctx.get_surface();
//		allocator_ = ctx.get_allocation_callbacks();
//
//		queue_ = std::static_pointer_cast<VulkanQueue>(ctx.create_queue(queue_type));
//		command_allocator_ = std::static_pointer_cast<VulkanCommandAllocator>(queue_->create_command_allocator());
//		
//		if (!update_swapchain()) {
//			return false;
//		}
//
//		return true;
//	}
//
//	auto VulkanPresentationEngine::update_swapchain() -> bool {
//#ifdef EDGE_PLATFORM_ANDROID
//		vk::PresentModeKHR present_mode = vsync_ ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_MAILBOX_KHR;
//		static constexpr std::array<vk::PresentModeKHR, 3ull> present_mode_priority_list{ VK_PRESENT_MODE_FIFO_KHR, VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR };
//#else
//		vk::PresentModeKHR present_mode = vsync_ ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_MAILBOX_KHR;
//		static constexpr std::array<vk::PresentModeKHR, 3ull> present_mode_priority_list{ VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_FIFO_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR };
//#endif
//
//		uint32_t surface_format_count;
//		VK_CHECK_RESULT(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_, surface_, &surface_format_count, nullptr), 
//			"Failed to request surface supported format count.");
//
//		std::vector<vk::SurfaceFormatKHR> surface_formats(surface_format_count);
//		VK_CHECK_RESULT(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_, surface_, &surface_format_count, surface_formats.data()),
//			"Failed to request surface supported formats.");
//
//		uint32_t present_mode_count;
//		VK_CHECK_RESULT(vkGetPhysicalDeviceSurfacePresentModesKHR(physical_, surface_, &present_mode_count, nullptr),
//			"Failed to request surface supported mode count.");
//
//		std::vector<vk::PresentModeKHR> present_modes(present_mode_count);
//		VK_CHECK_RESULT(vkGetPhysicalDeviceSurfacePresentModesKHR(physical_, surface_, &present_mode_count, present_modes.data()),
//			"Failed to request surface supported modes.");
//
//		VkSurfaceCapabilitiesKHR surface_caps;
//		VK_CHECK_RESULT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_, surface_, &surface_caps),
//			"Failed to request surface caps.");
//
//		auto potential_extent = requested_extent_;
//		if (potential_extent.width == 1 || potential_extent.height == 1) {
//			potential_extent = surface_caps.currentExtent;
//		}
//
//		VkSwapchainKHR old_swapchain{ handle_ };
//		swapchain_create_info.oldSwapchain = old_swapchain;
//		swapchain_create_info.minImageCount = std::clamp(requested_image_count_, surface_caps.minImageCount, surface_caps.maxImageCount ? surface_caps.maxImageCount : std::numeric_limits<uint32_t>::max());
//		swapchain_create_info.imageExtent = choose_extent(potential_extent, surface_caps.minImageExtent, surface_caps.maxImageExtent, surface_caps.currentExtent);
//		swapchain_create_info.imageArrayLayers = 1;
//
//		static const std::array<vk::SurfaceFormatKHR, 2ull> surface_format_priority_list = {
//			vk::SurfaceFormatKHR(VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR),
//			vk::SurfaceFormatKHR(VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
//		};
//
//		auto surface_format = choose_surface_format({}, surface_formats, surface_format_priority_list);
//		swapchain_create_info.imageFormat = surface_format.format;
//
//		VkFormatProperties format_properties;
//		vkGetPhysicalDeviceFormatProperties(physical_, swapchain_create_info.imageFormat, &format_properties);
//
//		std::set<vk::ImageUsageFlagBits> image_usage_flags{ VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_IMAGE_USAGE_TRANSFER_SRC_BIT };
//		image_usage_flags = choose_image_usage(image_usage_flags, surface_caps.supportedUsageFlags, format_properties.optimalTilingFeatures);
//
//		swapchain_create_info.imageUsage = composite_image_flags(image_usage_flags);
//		swapchain_create_info.preTransform = choose_transform(requested_transform_, surface_caps.supportedTransforms, surface_caps.currentTransform);
//		swapchain_create_info.compositeAlpha = choose_composite_alpha(VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR, surface_caps.supportedCompositeAlpha);
//		swapchain_create_info.presentMode = choose_present_mode(present_mode, present_modes, present_mode_priority_list);
//
//		swapchain_create_info.surface = surface_;
//		swapchain_create_info.imageFormat = surface_format.format;
//		swapchain_create_info.imageColorSpace = surface_format.colorSpace;
//		swapchain_create_info.clipped = VK_TRUE;
//		swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
//
//		//auto queue_families = device.get_queue_family_indices();
//		//if (queue_families.size() > 1) {
//		//	swapchain_create_info.queueFamilyIndexCount = static_cast<uint32_t>(queue_families.size());
//		//	swapchain_create_info.pQueueFamilyIndices = &queue_families[0];
//		//	swapchain_create_info.imageSharingMode = vk::SharingMode::eConcurrent;
//		//}
//
//		VK_CHECK_RESULT(vkCreateSwapchainKHR(device_, &swapchain_create_info, allocator_, &handle_),
//			"Failed to create swapchain.");
//
//		uint32_t swapchain_image_count;
//		VK_CHECK_RESULT(vkGetSwapchainImagesKHR(device_, handle_, &swapchain_image_count, nullptr),
//			"Failed to request number of swapchain images.");
//
//		std::vector<VkImage> swapchain_images(swapchain_image_count);
//		VK_CHECK_RESULT(vkGetSwapchainImagesKHR(device_, handle_, &swapchain_image_count, swapchain_images.data()),
//			"Failed to request swapchain images.");
//
//		// TODO: create images and update them
//
//#ifdef NDEBUG
//		
//#endif
//	}
}