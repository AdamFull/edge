#include "vk_context.h"

#include <numeric>

namespace edge::gfx::vulkan {
#define EDGE_LOGGER_SCOPE "Semaphore"

	Semaphore::~Semaphore() {
		if (handle_) {
			device_->destroy_handle(handle_);
		}
	}

	auto Semaphore::construct(const GraphicsContext& ctx, uint64_t initial_value) -> GFXResult<Owned<Semaphore>> {
		auto self = std::make_unique<Semaphore>();
		if (auto result = self->_construct(ctx, initial_value); result != Result::eSuccess) {
			return std::unexpected(result);
		}
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

	auto Semaphore::is_completed(uint64_t value) const -> GFXResult<bool> {
		auto result = get_value();
		if (!result.has_value()) {
			return std::unexpected(result.error());
		}
		return result.value() >= value;
	}

	auto Semaphore::get_value() const -> GFXResult<uint64_t> {
		auto result = device_->get_semaphore_counter_value(handle_);
		if (result) {
			return result.value();
		}

		auto error_code = result.error();
		EDGE_SLOGE("Failed to get semaphore value. Reason: {}.", vk::to_string(error_code));
		return std::unexpected(to_gfx_result(error_code));
	}

	auto Semaphore::_construct(const GraphicsContext& ctx, uint64_t initial_value) -> Result {
		device_ = &ctx.get_device();

		vk::SemaphoreTypeCreateInfo timeline_create_info{};
		timeline_create_info.semaphoreType = vk::SemaphoreType::eTimeline;
		timeline_create_info.initialValue = initial_value;

		vk::SemaphoreCreateInfo create_info{};
		create_info.pNext = &timeline_create_info;

		auto result = device_->create_handle(create_info, handle_);
		if (result != vk::Result::eSuccess) {
			EDGE_SLOGE("Failed to create semaphore. Reason: {}.", vk::to_string(result));
		}

		return to_gfx_result(result);
	}

#undef EDGE_LOGGER_SCOPE // Semaphore

#define EDGE_LOGGER_SCOPE "Queue"

	auto Queue::construct(GraphicsContext& ctx, QueueType type) -> GFXResult<Owned<Queue>> {
		auto self = std::make_unique<Queue>();
		if (auto result = self->_construct(ctx, type); result != Result::eSuccess) {
			return std::unexpected(result);
		}
		return self;
	}

	auto Queue::create_command_allocator() const -> GFXResult<Shared<IGFXCommandAllocator>> {
		return CommandAllocator::construct(*device_, handle_.get_family_index());
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

		if (auto result = handle_.submit(submit_info); result != vk::Result::eSuccess) {
			EDGE_SLOGE("Failed while signaling semaphore from gpu. Reason: {}.", vk::to_string(result));
		}
	}

	auto Queue::wait_idle() -> SyncResult {
		handle_.wait_idle();
		return SyncResult::eSuccess;
	}

	auto Queue::_construct(GraphicsContext& ctx, QueueType type) -> Result {
		device_ = &ctx.get_device();

		auto queue_result = device_->get_queue(to_vk_queue_type(type));
		if (!queue_result) {
			return to_gfx_result(queue_result.error());
		}

		handle_ = std::move(queue_result.value());

		return Result::eSuccess;
	}

#undef EDGE_LOGGER_SCOPE // Queue

#define EDGE_LOGGER_SCOPE "CommandAllocator"

	CommandAllocator::~CommandAllocator() {
		if (handle_) {
			device_->destroy_handle(handle_);
		}
	}

	auto CommandAllocator::construct(vkw::Device const& device, uint32_t family_index) -> GFXResult<Owned<CommandAllocator>> {
		auto self = std::make_unique<CommandAllocator>();
		if (auto result = self->_construct(device, family_index); result != Result::eSuccess) {
			return std::unexpected(result);
		}
		return self;
	}

	auto CommandAllocator::allocate_command_list() const -> GFXResult<Shared<IGFXCommandList>> {
		return CommandList::construct(*device_, handle_);
	}

	auto CommandAllocator::_construct(vkw::Device const& device, uint32_t family_index) -> Result {
		device_ = &device;
		family_index_ = family_index;

		vk::CommandPoolCreateInfo create_info{};
		create_info.queueFamilyIndex = family_index_;
		create_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;

		auto result = device_->create_handle(create_info, handle_);
		if (result != vk::Result::eSuccess) {
			EDGE_SLOGE("Failed to create command allocator. Reason: {}.", vk::to_string(result));
		}

		return to_gfx_result(result);
	}

#undef EDGE_LOGGER_SCOPE // CommandAllocator

#define EDGE_LOGGER_SCOPE "CommandList"

	CommandList::~CommandList() {
		if (handle_) {
			device_->free_command_buffer(command_pool_, handle_);
		}
	}

	auto CommandList::construct(vkw::Device const& device, vk::CommandPool command_pool) ->GFXResult<Owned<CommandList>> {
		auto self = std::make_unique<CommandList>();
		if (auto result = self->_construct(device, command_pool); result != Result::eSuccess) {
			return std::unexpected(result);
		}
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

	auto CommandList::_construct(vkw::Device const& device, vk::CommandPool command_pool) -> Result {
		device_ = &device;
		command_pool_ = command_pool;

		vk::CommandBufferAllocateInfo allocate_info{};
		allocate_info.commandPool = command_pool;
		allocate_info.level = vk::CommandBufferLevel::ePrimary;
		allocate_info.commandBufferCount = 1;

		auto result = device_->allocate_command_buffer(allocate_info, handle_);
		if (result != vk::Result::eSuccess) {
			EDGE_SLOGE("Failed to allocate command lists. Reason: {}.", vk::to_string(result));
		}

		return to_gfx_result(result);
	}

#undef EDGE_LOGGER_SCOPE // CommandList

#define EDGE_LOGGER_SCOPE "Buffer"

	auto Buffer::construct(const GraphicsContext& ctx, const BufferCreateInfo& create_info) -> GFXResult<Owned<Buffer>> {
		auto self = std::make_unique<Buffer>();
		if (auto result = self->_construct(ctx, create_info); result != Result::eSuccess) {
			return std::unexpected(result);
		}
		return self;
	}

	auto Buffer::create_view(const BufferViewCreateInfo& create_info) const -> GFXResult<Shared<IGFXBufferView>> {
		return BufferView::construct(*this, create_info);
	}

	auto Buffer::map() -> GFXResult<std::span<uint8_t>> {
		auto result = handle_.map();
		if (!result) {
			return std::unexpected(to_gfx_result(result.error()));
		}
		return result.value();
	}

	auto Buffer::unmap() -> void {
		handle_.unmap();
	}

	auto Buffer::flush(uint64_t offset, uint64_t size) -> Result {
		return to_gfx_result(handle_.flush(offset, size));
	}

	auto Buffer::update(const void* data, uint64_t size, uint64_t offset) -> GFXResult<uint64_t> {
		if (auto result = handle_.update(data, size, offset); result != vk::Result::eSuccess) {
			return std::unexpected(to_gfx_result(result));
		}
		return size;
	}

	auto Buffer::get_size() const noexcept -> uint64_t {
		return handle_.get_size();
	}

	auto Buffer::get_address() const -> uint64_t {
		return handle_.get_gpu_virtual_address();
	}

	auto Buffer::_construct(const GraphicsContext& ctx, const BufferCreateInfo& create_info) -> Result {
		auto const& device = ctx.get_device();
		auto properties = device.get_physical().getProperties();

		uint64_t minimal_alignment{ 1ull };
		vk::BufferCreateInfo buffer_create_info{};
		buffer_create_info.usage |= vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eShaderDeviceAddressKHR;

		VmaAllocationCreateInfo allocation_create_info{};
		allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO;

		constexpr VmaAllocationCreateFlags DYNAMIC_BUFFER_FLAGS =
			VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
			VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
			VMA_ALLOCATION_CREATE_MAPPED_BIT;
		
		switch (create_info.type)
		{
		case BufferType::eRaw: {
			allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
			break;
		}
		case BufferType::eStaging: {
			allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
			allocation_create_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
			break;
		}
		case BufferType::eReadback: {
			allocation_create_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
			break;
		}
		case BufferType::eVertex:
		case BufferType::eVertexDynamic: {
			minimal_alignment = std::max<uint64_t>(minimal_alignment, 4ull);
			buffer_create_info.usage |= vk::BufferUsageFlagBits::eVertexBuffer;
			if (create_info.type == BufferType::eVertexDynamic) {
				allocation_create_info.flags = DYNAMIC_BUFFER_FLAGS;
			}
			break;
		}
		case BufferType::eIndex:
		case BufferType::eIndexDynamic: {
			minimal_alignment = std::max<uint64_t>(minimal_alignment, 1ull);
			buffer_create_info.usage |= vk::BufferUsageFlagBits::eIndexBuffer;
			if (create_info.type == BufferType::eVertexDynamic) {
				allocation_create_info.flags = DYNAMIC_BUFFER_FLAGS;
			}
			break;
		}
		case BufferType::eUniform: {
			minimal_alignment = std::lcm(properties.limits.minUniformBufferOffsetAlignment, properties.limits.nonCoherentAtomSize);
			buffer_create_info.usage |= vk::BufferUsageFlagBits::eUniformBuffer;
			break;
		}
		case BufferType::eStorage:
		case BufferType::eStorageDynamic: {
			minimal_alignment = std::max(minimal_alignment, properties.limits.minStorageBufferOffsetAlignment);
			buffer_create_info.usage |= vk::BufferUsageFlagBits::eStorageBuffer;
			if (create_info.type == BufferType::eVertexDynamic) {
				allocation_create_info.flags = DYNAMIC_BUFFER_FLAGS;
			}
			break;
		}
		case BufferType::eIndirectArgument:
		case BufferType::eIndirectArgumentDynamic: {
			buffer_create_info.usage |= vk::BufferUsageFlagBits::eIndirectBuffer;
			if (create_info.type == BufferType::eVertexDynamic) {
				allocation_create_info.flags = DYNAMIC_BUFFER_FLAGS;
			}
			break;
		}
		case BufferType::eAccelerationStructureBuild: {
			buffer_create_info.usage |= vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR;
			break;
		}
		case BufferType::eAccelerationStructureStorage: {
			buffer_create_info.usage |= vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR;
			break;
		}
		case BufferType::eShaderBindingTable: {
			buffer_create_info.usage |= vk::BufferUsageFlagBits::eShaderBindingTableKHR;
			break;
		}
		default:
			break;
		}

		buffer_create_info.size = aligned_size(create_info.block_size, minimal_alignment) * create_info.count_block;

		auto const& allocator = ctx.get_memory_allocator();
		auto result = allocator.allocate_buffer(buffer_create_info, allocation_create_info);
		if (!result) {
			return to_gfx_result(result.error());
		}
		handle_ = std::move(result.value());

		return Result::eSuccess;
	}

#undef EDGE_LOGGER_SCOPE // Buffer

#define EDGE_LOGGER_SCOPE "BufferView"

	auto BufferView::construct(const Buffer& buffer, const BufferViewCreateInfo& create_info) -> GFXResult<Owned<BufferView>> {
		auto self = std::make_unique<BufferView>();
		if (auto result = self->_construct(buffer, create_info); result != Result::eSuccess) {
			return std::unexpected(result);
		}
		return self;
	}

	auto BufferView::get_offset() const noexcept -> uint64_t {
		return handle_.get_offset();
	}

	auto BufferView::get_size() const noexcept -> uint64_t {
		return handle_.get_size();
	}

	auto BufferView::get_format() const noexcept -> TinyImageFormat {
		return TinyImageFormat_FromVkFormat(static_cast<TinyImageFormat_VkFormat>(handle_.get_format()));
	}

	auto BufferView::_construct(const Buffer& buffer, const BufferViewCreateInfo& create_info) -> Result {
		auto const& buffer_handle = buffer.get_handle();

		auto result = buffer_handle.create_view(create_info.byte_offset, create_info.size, static_cast<vk::Format>(TinyImageFormat_ToVkFormat(create_info.format)));
		if (!result) {
			return to_gfx_result(result.error());
		}

		handle_ = std::move(result.value());

		return Result::eSuccess;
	}

#undef EDGE_LOGGER_SCOPE // BufferView

#define EDGE_LOGGER_SCOPE "Image"

	auto Image::construct(const GraphicsContext& ctx, const ImageCreateInfo& create_info)->GFXResult<Owned<Image>> {
		auto self = std::make_unique<Image>();
		if (auto result = self->_construct(ctx, create_info); result != Result::eSuccess) {
			return std::unexpected(result);
		}
		return self;
	}

	auto Image::create_view(const ImageViewCreateInfo& create_info) const -> GFXResult<Shared<IGFXImageView>> {
		return nullptr;
	}
	
	auto Image::_construct(const GraphicsContext& ctx, const ImageCreateInfo& create_info) -> Result {
		VmaAllocationCreateInfo allocation_create_info{};
		allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO;

		vk::ImageCreateInfo image_create_info{};
		image_create_info.extent.width = create_info.extent.width;
		image_create_info.extent.height = create_info.extent.height;
		image_create_info.extent.depth = create_info.extent.depth;
		image_create_info.arrayLayers = create_info.layers;
		image_create_info.mipLevels = create_info.levels;
		image_create_info.format = static_cast<vk::Format>(TinyImageFormat_ToVkFormat(create_info.format));
		image_create_info.flags = (create_info.layers == 6u) ? vk::ImageCreateFlagBits::eCubeCompatible : vk::ImageCreateFlagBits::eExtendedUsage;
		image_create_info.imageType = (create_info.extent.depth > 1u) ? vk::ImageType::e3D : (create_info.extent.height > 1u) ? vk::ImageType::e2D : vk::ImageType::e1D;
		image_create_info.sharingMode = vk::SharingMode::eExclusive;

		if (create_info.flags & ImageFlag::eShaderResource) {
			image_create_info.usage |= vk::ImageUsageFlagBits::eSampled;
		}

		if (create_info.flags & ImageFlag::eUnorderedAccess) {
			image_create_info.usage |= vk::ImageUsageFlagBits::eStorage;
		}

		if (create_info.flags & ImageFlag::eCopyable) {
			image_create_info.usage |= vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst;
		}

		if (create_info.flags & ImageFlag::eRenderTarget) {
			image_create_info.usage |= ((TinyImageFormat_IsDepthAndStencil(create_info.format) || TinyImageFormat_IsDepthOnly(create_info.format)) ? vk::ImageUsageFlagBits::eDepthStencilAttachment : vk::ImageUsageFlagBits::eColorAttachment);
			allocation_create_info.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
			allocation_create_info.priority = 1.0f;
		}

		auto const& device = ctx.get_device();
		auto queue_family_properties = device.get_queue_family_properties();
		vkw::Vector<uint32_t> queue_family_indices(queue_family_properties.size(), device.get_allocator());
		std::iota(queue_family_indices.begin(), queue_family_indices.end(), 0);

		if (queue_family_indices.size() > 1) {
			image_create_info.queueFamilyIndexCount = static_cast<uint32_t>(queue_family_indices.size());
			image_create_info.pQueueFamilyIndices = queue_family_indices.data();
			image_create_info.sharingMode = vk::SharingMode::eConcurrent;
		}

		auto const& allocator = ctx.get_memory_allocator();
		auto result = allocator.allocate_image(image_create_info, allocation_create_info);
		if (!result) {
			return to_gfx_result(result.error());
		}
		handle_ = std::move(result.value());

		return Result::eSuccess;
	}

	auto ImageView::construct(const Image& image, const ImageViewCreateInfo& create_info) -> GFXResult<Owned<ImageView>> {
		auto self = std::make_unique<ImageView>();
		if (auto result = self->_construct(image, create_info); result != Result::eSuccess) {
			return std::unexpected(result);
		}
		return self;
	}

	auto ImageView::_construct(const Image& image, const ImageViewCreateInfo& create_info) -> Result {
		auto const& image_handle = image.get_handle();

		auto result = image_handle.create_view(create_info.first_layer, create_info.layers, create_info.first_level, create_info.levels, to_vk_image_view_type(create_info.type));
		if (!result) {
			return to_gfx_result(result.error());
		}

		handle_ = std::move(result.value());
		return Result::eSuccess;
	}

#undef EDGE_LOGGER_SCOPE // Image

#define EDGE_LOGGER_SCOPE "PresentationFrame"

	PresentationFrame::~PresentationFrame() {
		if (image_available_) {
			device_->destroy_handle(image_available_);
		}

		if (rendering_finished_) {
			device_->destroy_handle(rendering_finished_);
		}

		if (fence_) {
			device_->destroy_handle(fence_);
		}
	}

	auto PresentationFrame::construct(const GraphicsContext& ctx, Shared<CommandAllocator> cmd_allocator) -> GFXResult<Owned<PresentationFrame>> {
		auto self = std::make_unique<PresentationFrame>();
		if (auto result = self->_construct(ctx, cmd_allocator); result != Result::eSuccess) {
			return std::unexpected(result);
		}
		return self;
	}

	auto PresentationFrame::_construct(const GraphicsContext& ctx, Shared<CommandAllocator> cmd_allocator) -> Result {
		device_ = &ctx.get_device();

		vk::SemaphoreTypeCreateInfo semaphore_type{};
		semaphore_type.semaphoreType = vk::SemaphoreType::eBinary;

		vk::SemaphoreCreateInfo semaphore_create_info{};
		semaphore_create_info.pNext = &semaphore_type;

		auto result = device_->create_handle(semaphore_create_info, image_available_);
		if (result != vk::Result::eSuccess) { return to_gfx_result(result); }

		result = device_->create_handle(semaphore_create_info, rendering_finished_);
		if (result != vk::Result::eSuccess) { return to_gfx_result(result); }

		vk::FenceCreateInfo fence_create_info{};
		fence_create_info.flags = vk::FenceCreateFlagBits::eSignaled;

		result = device_->create_handle(fence_create_info, fence_);
		if (result != vk::Result::eSuccess) { return to_gfx_result(result); }

		auto allocation_result = cmd_allocator->allocate_command_list();
		if (!allocation_result) {
			return allocation_result.error();
		}

		auto new_cmd_list = std::move(allocation_result.value());
		command_list_ = std::move(std::static_pointer_cast<CommandList>(new_cmd_list));

		return Result::eSuccess;
	}

#undef EDGE_LOGGER_SCOPE // PresentationFrame

#define EDGE_LOGGER_SCOPE "PresentationEngine"

	PresentationEngine::~PresentationEngine() {

	}

	auto PresentationEngine::construct(const GraphicsContext& ctx, const PresentationEngineCreateInfo& create_info) -> GFXResult<Owned<PresentationEngine>> {
		auto self = std::make_unique<PresentationEngine>();
		if (auto result = self->_construct(ctx, create_info); result != Result::eSuccess) {
			return std::unexpected(result);
		}
		return self;
	}

	auto PresentationEngine::begin(uint32_t* frame_index) -> bool {
		return false;
	}

	auto PresentationEngine::end(const PresentInfo& present_info) -> bool {
		return false;
	}

	auto PresentationEngine::_construct(const GraphicsContext& ctx, const PresentationEngineCreateInfo& create_info) -> Result {
		context_ = &ctx;

		auto queue_creation_result = ctx.create_queue(create_info.queue_type);
		if (!queue_creation_result) {
			return queue_creation_result.error();
		}

		auto new_queue = std::move(queue_creation_result.value());
		queue_ = std::move(std::static_pointer_cast<Queue>(new_queue));

		auto allocator_creation_result = queue_->create_command_allocator();
		if (!allocator_creation_result) {
			return allocator_creation_result.error();
		}

		auto new_allocator = std::move(allocator_creation_result.value());
		command_allocator_ = std::move(std::static_pointer_cast<CommandAllocator>(new_allocator));

		auto swapchain_result = vkw::SwapchainBuilder{ ctx.get_device(), ctx.get_surface() }
			.set_image_extent(create_info.extent.width, create_info.extent.height)
			.set_image_count(create_info.image_count)
			.set_image_format(static_cast<vk::Format>(TinyImageFormat_ToVkFormat(create_info.format)))
			.set_color_space(to_vk_color_space(create_info.color_space))
			.enable_vsync(create_info.vsync)
			.enable_hdr(create_info.hdr)
			.build();
		
		if (!swapchain_result) {
			return to_gfx_result(swapchain_result.error());
		}

		swapchain_ = std::move(swapchain_result.value());

		// TODO: Create images
		// TODO: Create views
		// TODO: Create frames

		return Result::eSuccess;
	}

#undef EDGE_LOGGER_SCOPE // PresentationEngine
}