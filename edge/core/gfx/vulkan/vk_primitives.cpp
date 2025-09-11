#include "vk_context.h"

namespace edge::gfx::vulkan {
#define EDGE_LOGGER_SCOPE "Semaphore"

	Semaphore::~Semaphore() {
		if (handle_) {
			device_->destroy_handle(handle_);
		}
	}

	auto Semaphore::construct(const GraphicsContext& ctx, uint64_t initial_value) -> Owned<Semaphore> {
		auto self = std::make_unique<Semaphore>();
		self->_construct(ctx, initial_value);
		return self;
	}

	auto Semaphore::signal(uint64_t value) -> SyncResult {
		vk::SemaphoreSignalInfo signal_info{};
		signal_info.semaphore = handle_;
		signal_info.value = value;

		auto result = device_->signal_semaphore(signal_info);
		if (result == vk::Result::eSuccess) {
			return SyncResult::eSuccess;
		}

		EDGE_SLOGE("Failed while signaling semaphore from cpu. Reason: {}.", vk::to_string(result));
		return (result == vk::Result::eErrorDeviceLost) ? SyncResult::eDeviceLost : SyncResult::eError;
	}

	auto Semaphore::wait(uint64_t value, std::chrono::nanoseconds timeout) -> SyncResult {
		vk::SemaphoreWaitInfo wait_info{};
		wait_info.semaphoreCount = 1;
		wait_info.pSemaphores = &handle_;
		wait_info.pValues = &value;

		uint64_t timeout_ns = (timeout == std::chrono::nanoseconds::max()) ? UINT64_MAX : timeout.count();

		vk::Result result = device_->wait_semaphore(wait_info, timeout_ns);
		switch (result) {
		case vk::Result::eSuccess: return SyncResult::eSuccess;
		case vk::Result::eTimeout: return SyncResult::eTimeout;
		case vk::Result::eErrorDeviceLost: return SyncResult::eDeviceLost;
		default: {
			EDGE_SLOGE("Failed while waiting semaphore on cpu. Reason: {}.", vk::to_string(result));
			return SyncResult::eError;
		}
		}
	}

	auto Semaphore::is_completed(uint64_t value) const -> bool {
		return get_value() >= value;
	}

	auto Semaphore::get_value() const -> uint64_t {
		auto result = device_->get_semaphore_counter_value(handle_);
		if (result) {
			return result.value();
		}

		EDGE_SLOGE("Failed to get semaphore value. Reason: {}.", vk::to_string(result.error()));

		return ~0ull;
	}

	auto Semaphore::_construct(const GraphicsContext& ctx, uint64_t initial_value) -> bool {
		device_ = &ctx.get_device();

		vk::SemaphoreTypeCreateInfo timeline_create_info{};
		timeline_create_info.semaphoreType = vk::SemaphoreType::eTimeline;
		timeline_create_info.initialValue = initial_value;

		vk::SemaphoreCreateInfo create_info{};
		create_info.pNext = &timeline_create_info;

		if (auto result = device_->create_handle(create_info, handle_); result != vk::Result::eSuccess) {
			EDGE_SLOGE("Failed to create semaphore. Reason: {}.", vk::to_string(result));
			return false;
		}

		return true;
	}

#undef EDGE_LOGGER_SCOPE // Semaphore

#define EDGE_LOGGER_SCOPE "Queue"

	auto Queue::construct(const GraphicsContext& ctx, uint32_t family_index, uint32_t queue_index) -> Owned<Queue> {
		auto self = std::make_unique<Queue>();
		self->_construct(ctx, family_index, queue_index);
		return self;
	}

	auto Queue::create_command_allocator() const -> Shared<IGFXCommandAllocator> {
		return CommandAllocator::construct(*device_, family_index_);
	}

	auto Queue::submit(const SubmitQueueInfo& submit_info) -> void {
		FixedVector<vk::SemaphoreSubmitInfo> wait_semaphores{};
		FixedVector<vk::SemaphoreSubmitInfo> signal_semaphores{};
		FixedVector<vk::CommandBufferSubmitInfo> command_buffers{};

		for (int32_t i = 0; i < static_cast<int32_t>(submit_info.wait_semaphores.size()); ++i) {
			auto& semaphore_info = submit_info.wait_semaphores[i];
			if (semaphore_info.semaphore) {
				auto& semaphore_submit_info = wait_semaphores.emplace_back();
				semaphore_submit_info.semaphore = std::static_pointer_cast<Semaphore>(semaphore_info.semaphore)->get_handle();
				semaphore_submit_info.value = semaphore_info.value;
				semaphore_submit_info.stageMask = vk::PipelineStageFlagBits2::eAllCommands;
			}
		}

		for (int32_t i = 0; i < static_cast<int32_t>(submit_info.signal_semaphores.size()); ++i) {
			auto& semaphore_info = submit_info.signal_semaphores[i];
			if (semaphore_info.semaphore) {
				auto& semaphore_submit_info = signal_semaphores.emplace_back();
				semaphore_submit_info.semaphore = std::static_pointer_cast<Semaphore>(semaphore_info.semaphore)->get_handle();
				semaphore_submit_info.value = semaphore_info.value;
				semaphore_submit_info.stageMask = vk::PipelineStageFlagBits2::eAllCommands;
			}
		}

		for (int32_t i = 0; i < submit_info.command_lists.size(); ++i) {
			auto& cmd_list_submit_info = command_buffers.emplace_back();
			cmd_list_submit_info.commandBuffer = std::static_pointer_cast<CommandList>(submit_info.command_lists[i])->get_handle();
		}

		submit(wait_semaphores, signal_semaphores, command_buffers);
	}

	auto Queue::submit(Span<const vk::SemaphoreSubmitInfo> wait_semaphores, Span<const vk::SemaphoreSubmitInfo> signal_semaphores, Span<const vk::CommandBufferSubmitInfo> command_buffers) -> void {
		vk::SubmitInfo2KHR submit_info{};
		submit_info.pWaitSemaphoreInfos = wait_semaphores.data();
		submit_info.waitSemaphoreInfoCount = static_cast<uint32_t>(wait_semaphores.size());
		submit_info.pSignalSemaphoreInfos = signal_semaphores.data();
		submit_info.signalSemaphoreInfoCount = static_cast<uint32_t>(signal_semaphores.size());
		submit_info.pCommandBufferInfos = command_buffers.data();
		submit_info.commandBufferInfoCount = static_cast<uint32_t>(command_buffers.size());

		vk::Result result = handle_.submit2(1, &submit_info, VK_NULL_HANDLE);
		if (result != vk::Result::eSuccess) {
			EDGE_SLOGE("Failed while signaling semaphore from gpu. Reason: {}.", vk::to_string(result));
		}
	}

	// TODO: Implement present

	auto Queue::wait_idle() -> SyncResult {
		handle_.waitIdle();
		return SyncResult::eSuccess;
	}

	auto Queue::_construct(const GraphicsContext& ctx, uint32_t family_index, uint32_t queue_index) -> bool {
		device_ = &ctx.get_device();

		family_index_ = family_index;
		queue_index_ = queue_index;

		vk::DeviceQueueInfo2 device_queue_info{};
		device_queue_info.queueFamilyIndex = family_index;
		device_queue_info.queueIndex = queue_index;

		device_->get_queue(device_queue_info, handle_);

		return true;
	}

#undef EDGE_LOGGER_SCOPE // Queue

#define EDGE_LOGGER_SCOPE "CommandAllocator"

	CommandAllocator::~CommandAllocator() {
		if (handle_) {
			device_->destroy_handle(handle_);
		}
	}

	auto CommandAllocator::construct(vkw::Device const& device, uint32_t family_index) -> Owned<CommandAllocator> {
		auto self = std::make_unique<CommandAllocator>();
		self->_construct(device, family_index);
		return self;
	}

	auto CommandAllocator::allocate_command_list() const -> Shared<IGFXCommandList> {
		return CommandList::construct(*device_, handle_);
	}

	auto CommandAllocator::reset() -> void {
		// TODO: Not sure do i need individual reset for this one
	}

	auto CommandAllocator::_construct(vkw::Device const& device, uint32_t family_index) -> bool {
		device_ = &device;
		family_index_ = family_index;

		vk::CommandPoolCreateInfo create_info{};
		create_info.queueFamilyIndex = family_index_;
		create_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;

		if (auto result = device_->create_handle(create_info, handle_); result != vk::Result::eSuccess) {
			EDGE_SLOGE("Failed to create command allocator. Reason: {}.", vk::to_string(result));
			return false;
		}

		return true;
	}

#undef EDGE_LOGGER_SCOPE // CommandAllocator

#define EDGE_LOGGER_SCOPE "CommandList"

	CommandList::~CommandList() {
		if (handle_) {
			device_->free_command_buffer(command_pool_, handle_);
		}
	}

	auto CommandList::construct(vkw::Device const& device, vk::CommandPool command_pool) -> Owned<CommandList> {
		auto self = std::make_unique<CommandList>();
		self->_construct(device, command_pool);
		return self;
	}

	auto CommandList::begin() -> bool {
		handle_.reset(vk::CommandBufferResetFlagBits::eReleaseResources);
		
		vk::CommandBufferBeginInfo begin_info{};
		begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

		if (auto result = handle_.begin(&begin_info); result != vk::Result::eSuccess) {
			EDGE_SLOGE("Failed to begin command list. Reason: {}.", vk::to_string(result));
			return false;
		}

		return true;
	}

	auto CommandList::end() -> bool {
		handle_.end();
		return true;
	}

	auto CommandList::set_viewport(float x, float y, float width, float height, float min_depth, float max_depth) const -> void {
		vk::Viewport viewport{ x, y, width, height, min_depth, max_depth };
		handle_.setViewport(0, 1, &viewport);
	}

	auto CommandList::set_scissor(uint32_t x, uint32_t y, uint32_t width, uint32_t height) const -> void {
		vk::Rect2D scissor{ { static_cast<int32_t>(x), static_cast<int32_t>(y) }, { width, height } };
		handle_.setScissor(0, 1, &scissor);
	}

	auto CommandList::draw(uint32_t vertex_count, uint32_t first_vertex, uint32_t first_instance, uint32_t instance_count) const -> void {
		handle_.draw(vertex_count, instance_count, first_vertex, first_instance);
	}

	auto CommandList::draw_indexed(uint32_t index_count, uint32_t first_index, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance) const -> void {
		handle_.drawIndexed(index_count, instance_count, first_index, first_vertex, first_instance);
	}

	auto CommandList::dispatch(uint32_t group_x, uint32_t group_y, uint32_t group_z) const -> void {
		handle_.dispatch(group_x, group_y, group_z);
	}

	auto CommandList::begin_marker(std::string_view name, uint32_t color) const -> void {
		vk::DebugMarkerMarkerInfoEXT marker_info{};
		marker_info.pMarkerName = name.data();
		vkw::make_color_array(color, marker_info.color);
		handle_.debugMarkerBeginEXT(marker_info);
	}

	auto CommandList::insert_marker(std::string_view name, uint32_t color) const -> void {
		vk::DebugMarkerMarkerInfoEXT marker_info{};
		marker_info.pMarkerName = name.data();
		vkw::make_color_array(color, marker_info.color);
		handle_.debugMarkerInsertEXT(marker_info);
	}

	auto CommandList::end_marker() const -> void {
		handle_.debugMarkerEndEXT();
	}

	auto CommandList::_construct(vkw::Device const& device, vk::CommandPool command_pool) -> bool {
		device_ = &device;
		command_pool_ = command_pool;

		vk::CommandBufferAllocateInfo allocate_info{};
		allocate_info.commandPool = command_pool;
		allocate_info.level = vk::CommandBufferLevel::ePrimary;
		allocate_info.commandBufferCount = 1;

		if (auto result = device_->allocate_command_buffer(allocate_info, handle_); result != vk::Result::eSuccess) {
			EDGE_SLOGE("Failed to allocate command lists. Reason: {}.", vk::to_string(result));
			return false;
		}

		return true;
	}

#undef EDGE_LOGGER_SCOPE // CommandList

	// Presentation engine
//	auto VulkanPresentationEngine::construct(const GraphicsContext& ctx, QueueType queue_type, uint32_t frames_in_flight) -> Owned<VulkanPresentationEngine> {
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
//	auto VulkanPresentationEngine::_construct(const GraphicsContext& ctx, QueueType queue_type, uint32_t frames_in_flight) -> bool {
//		device_ = ctx.get_logical_device();
//		physical_ = ctx.get_physical_device();
//		surface_ = ctx.get_surface();
//		allocator_ = ctx.get_allocation_callbacks();
//
//		queue_ = std::static_pointer_cast<Queue>(ctx.create_queue(queue_type));
//		command_allocator_ = std::static_pointer_cast<CommandAllocator>(queue_->create_command_allocator());
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