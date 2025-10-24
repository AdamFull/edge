#pragma once

#include "gfx_base.h"
#include "gfx_enum.h"

namespace edge::platform {
	class IPlatformWindow;
}

namespace edge::gfx {
	class Instance;
	class Surface;
	class Adapter;
	class Device;
	class Queue;
	class MemoryAllocator;
	class Image;
	class ImageView;
	class Buffer;
	class BufferView;
	class CommandPool;
	class CommandBuffer;
	class QueryPool;
	class PipelineCache;
	class Pipeline;
	class PipelineLayout;
	class DescriptorSetLayout;
	class DescriptorPool;
	class DescriptorSet;

	extern vk::detail::DynamicLoader loader_;
	extern vk::AllocationCallbacks const* allocator_;
	extern Instance instance_;
	extern Surface surface_;
	extern Adapter adapter_;
	extern Device device_;
	extern MemoryAllocator memory_allocator_;

	template<typename T>
	class Handle : public NonCopyable {
	public:
		Handle(std::nullptr_t) noexcept {};

		Handle(T handle = VK_NULL_HANDLE) noexcept
			: handle_{ handle } {
		}

		Handle(Handle&& other) noexcept
			: handle_{ std::exchange(other.handle_, VK_NULL_HANDLE) } {
		}

		auto operator=(Handle&& other) noexcept -> Handle& {
			handle_ = std::exchange(other.handle_, VK_NULL_HANDLE);
			return *this;
		}

		auto operator*() const noexcept -> T const& { return handle_; }
		auto operator*() noexcept -> T& { return handle_; }

		auto operator->() const noexcept -> T const* { return &handle_; }
		auto operator->() noexcept -> T* { return &handle_; }

		operator bool() const noexcept { return handle_; }
		operator T() const noexcept { return handle_; }
		operator typename T::CType() const noexcept { return handle_; }
		[[nodiscard]] auto get_handle() const noexcept -> T { return handle_; }
	protected:
		T handle_{ VK_NULL_HANDLE };
	};

	class Instance : public Handle<vk::Instance> {
	public:
		Instance() = default;
		Instance(vk::Instance handle, vk::DebugUtilsMessengerEXT debug_messenger, mi::Vector<const char*>&& enabled_extensions, mi::Vector<const char*> enabled_layers);
		~Instance();

		Instance(Instance&& other) noexcept
			: Handle(std::move(other))
			, debug_messenger_{ std::exchange(other.debug_messenger_, VK_NULL_HANDLE) }
			, enabled_extensions_{ std::exchange(other.enabled_extensions_, {}) }
			, enabled_layers_{ std::exchange(other.enabled_layers_, {}) } {
		}

		auto operator=(Instance&& other) noexcept -> Instance& {
			Handle::operator=(std::move(other));
			debug_messenger_ = std::exchange(other.debug_messenger_, VK_NULL_HANDLE);
			enabled_extensions_ = std::exchange(other.enabled_extensions_, {});
			enabled_layers_ = std::exchange(other.enabled_layers_, {});
			return *this;
		}

		auto is_extension_enabled(const char* extension_name) const -> bool;
		auto is_layer_enabled(const char* layer_name) const -> bool;

		auto get_adapters() const -> Result<mi::Vector<Adapter>>;
	private:
		vk::DebugUtilsMessengerEXT debug_messenger_{ VK_NULL_HANDLE };
		mi::Vector<const char*> enabled_extensions_{};
		mi::Vector<const char*> enabled_layers_{};
	};

	template<typename T>
	class InstanceHandle : public Handle<T> {
	public:
		InstanceHandle(T handle = VK_NULL_HANDLE) noexcept
			: Handle<T>{ handle } {
		}

		~InstanceHandle() {
			if (handle_ && instance_) {
				instance_->destroy(handle_, allocator_);
				handle_ = VK_NULL_HANDLE;
			}
		}

		InstanceHandle(InstanceHandle&& other) noexcept
			: Handle<T>{ std::move(other) } {
		}

		auto operator=(InstanceHandle&& other) noexcept -> InstanceHandle& {
			Handle<T>::operator=(std::move(other));
			return *this;
		}
	protected:
		using Handle<T>::handle_;
	};

	class Surface : public InstanceHandle<vk::SurfaceKHR> {
	public:
		Surface(vk::SurfaceKHR handle = VK_NULL_HANDLE) noexcept
			: InstanceHandle{ handle } {

		}

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
		static auto create(const vk::AndroidSurfaceCreateInfoKHR& create_info) -> Result<Surface>;
#elif defined(VK_USE_PLATFORM_WIN32_KHR)
		static auto create(const vk::Win32SurfaceCreateInfoKHR& create_info) -> Result<Surface>;
#endif
	};

	class InstanceBuilder {
	public:
		InstanceBuilder() = default;

		// Application info setters
		auto set_app_name(const char* name) -> InstanceBuilder& {
			app_info_.pApplicationName = name;
			return *this;
		}

		auto set_app_version(uint32_t version) -> InstanceBuilder& {
			app_info_.applicationVersion = version;
			return *this;
		}

		auto set_app_version(uint32_t major, uint32_t minor, uint32_t patch) -> InstanceBuilder& {
			app_info_.applicationVersion = VK_MAKE_VERSION(major, minor, patch);
			return *this;
		}

		auto set_engine_name(const char* name) -> InstanceBuilder& {
			app_info_.pEngineName = name;
			return *this;
		}

		auto set_engine_version(uint32_t version) -> InstanceBuilder& {
			app_info_.engineVersion = version;
			return *this;
		}

		auto set_engine_version(uint32_t major, uint32_t minor, uint32_t patch) -> InstanceBuilder& {
			app_info_.engineVersion = VK_MAKE_VERSION(major, minor, patch);
			return *this;
		}

		auto set_api_version(uint32_t version) -> InstanceBuilder& {
			app_info_.apiVersion = version;
			return *this;
		}

		auto set_api_version(uint32_t major, uint32_t minor, uint32_t patch) -> InstanceBuilder& {
			app_info_.apiVersion = VK_MAKE_API_VERSION(0, major, minor, patch);
			return *this;
		}

		// Extension management
		auto add_extension(const char* extension_name, bool required = false) -> InstanceBuilder& {
			requested_extensions_.push_back(std::make_pair(extension_name, required));
			return *this;
		}

		// Layer management
		auto add_layer(const char* layer_name, bool required = false) -> InstanceBuilder& {
			requested_layers_.push_back(std::make_pair(layer_name, required));
			return *this;
		}

		auto add_validation_feature_enable(vk::ValidationFeatureEnableEXT enables) -> InstanceBuilder& {
			validation_feature_enables_.push_back(enables);
			return *this;
		}

		auto add_validation_feature_disable(vk::ValidationFeatureDisableEXT disables) -> InstanceBuilder& {
			validation_feature_disables_.push_back(disables);
			return *this;
		}

		// Debug utilities convenience methods
		auto enable_debug_utils() -> InstanceBuilder& {
			enable_debug_utils_ = true;
			return *this;
		}

		auto enable_portability() -> InstanceBuilder& {
			enable_portability_ = true;
			return *this;
		}

		// Surface extension convenience methods
		auto enable_surface() -> InstanceBuilder& {
			enable_surface_ = true;
			return *this;
		}

		// Flags and pNext chain
		auto add_flag(vk::InstanceCreateFlagBits flags) -> InstanceBuilder& {
			create_info_.flags |= flags;
			return *this;
		}

		auto set_flags(vk::InstanceCreateFlags flags) -> InstanceBuilder& {
			create_info_.flags = flags;
			return *this;
		}

		auto set_next_chain(const void* next) -> InstanceBuilder& {
			create_info_.pNext = next;
			return *this;
		}

		auto is_valid() const -> bool;

		// Build the instance
		auto build() -> Result<Instance>;

		// Get current configuration for inspection
		auto get_app_info() const -> const vk::ApplicationInfo& { return app_info_; }

	private:
		vk::ApplicationInfo app_info_{};
		vk::InstanceCreateInfo create_info_{};
		mi::Vector<std::pair<const char*, bool>> requested_extensions_{};
		mi::Vector<std::pair<const char*, bool>> requested_layers_{};
		mi::Vector<vk::ValidationFeatureEnableEXT> validation_feature_enables_{};
		mi::Vector<vk::ValidationFeatureDisableEXT> validation_feature_disables_{};

		bool enable_debug_utils_{ false };
		bool enable_surface_{ false };
		bool enable_portability_{ false };
	};

	class Adapter : public Handle<vk::PhysicalDevice> {
	public:
		Adapter() = default;
		Adapter(vk::PhysicalDevice handle, mi::Vector<vk::ExtensionProperties>&& device_extensions);
		~Adapter() = default;

		Adapter(Adapter&& other) noexcept
			: Handle(std::move(other))
			, supported_extensions_{ std::exchange(other.supported_extensions_, {}) } {
		}

		auto operator=(Adapter&& other) noexcept -> Adapter& {
			Handle::operator=(std::move(other));
			supported_extensions_ = std::exchange(other.supported_extensions_, {});
			return *this;
		}

		auto get_core_extension_names(uint32_t core_version) const -> mi::Vector<const char*>;
		auto is_supported(const char* extension_name) const -> bool;
	private:
		mi::Vector<vk::ExtensionProperties> supported_extensions_;
	};

	class Device : public Handle<vk::Device> {
	public:
		struct QueueFamilyInfo {
			uint32_t index;
			mi::Vector<uint32_t> queue_indices;
		};

		Device() = default;
		Device(vk::Device handle, mi::Vector<const char*>&& enabled_extensions, Array<mi::Vector<QueueFamilyInfo>, 3ull>&& queue_family_map);
		~Device();

		Device(Device&& other) noexcept
			: Handle(std::move(other))
			, enabled_extensions_{ std::exchange(other.enabled_extensions_, {}) }
			, queue_family_map_{ std::exchange(other.queue_family_map_, {}) } {
		}

		auto operator=(Device&& other) noexcept -> Device& {
			Handle::operator=(std::move(other));
			enabled_extensions_ = std::exchange(other.enabled_extensions_, {});
			queue_family_map_ = std::exchange(other.queue_family_map_, {});
			return *this;
		}

		auto get_queue(QueueType type) const -> Result<Queue>;

		auto is_enabled(const char* extension_name) const -> bool;
	private:
		mi::Vector<const char*> enabled_extensions_;
		mutable Array<mi::Vector<QueueFamilyInfo>, 3ull> queue_family_map_;
	};

	class DeviceSelector {
	public:
		DeviceSelector() = default;

		auto set_surface(vk::SurfaceKHR surface) -> DeviceSelector& {
			surface_ = surface;
			return *this;
		}

		auto set_api_version(uint32_t version) -> DeviceSelector& {
			minimal_api_ver = version;
			return *this;
		}

		auto set_api_version(uint32_t major, uint32_t minor, uint32_t patch) -> DeviceSelector& {
			minimal_api_ver = VK_MAKE_API_VERSION(0, major, minor, patch);
			return *this;
		}

		auto set_preferred_device_type(vk::PhysicalDeviceType type) -> DeviceSelector& {
			preferred_type_ = type;
			return *this;
		}

		// Extension management
		auto add_extension(const char* extension_name, bool required = true) -> DeviceSelector& {
			requested_extensions_.push_back(std::make_pair(extension_name, required));
			return *this;
		}

		auto remove_extension(const char* extension_name) -> DeviceSelector& {
			requested_extensions_.erase(
				std::remove_if(requested_extensions_.begin(), requested_extensions_.end(),
					[extension_name](const std::pair<const char*, bool>& ext) { return std::strcmp(ext.first, extension_name) == 0; }),
				requested_extensions_.end()
			);
			return *this;
		}

		auto clear_extensions() -> DeviceSelector& {
			requested_extensions_.clear();
			return *this;
		}

		template<typename StructureType>
		auto add_feature(bool required = true) -> DeviceSelector& {
			StructureType feature{};
			feature.pNext = nullptr;

			requested_features_.push_back(std::move(std::make_shared<StructureType>(feature)));
			auto* extension_ptr = static_cast<StructureType*>(requested_features_.back().get());

			if (last_feature_ptr_) {
				extension_ptr->pNext = last_feature_ptr_;
			}
			last_feature_ptr_ = extension_ptr;
			requested_extensions_.push_back(std::make_pair(FeatureTraits<StructureType>::extension_name, required));

			return *this;
		}

		auto select() -> Result<std::tuple<Adapter, Device>>;

	private:
		vk::SurfaceKHR surface_{ VK_NULL_HANDLE };
		uint32_t minimal_api_ver{ VK_VERSION_1_0 };
		vk::PhysicalDeviceType preferred_type_{ vk::PhysicalDeviceType::eDiscreteGpu };

		mi::Vector<std::pair<const char*, bool>> requested_extensions_{};

		mi::Vector<Shared<void>> requested_features_{};
		void* last_feature_ptr_{ nullptr };
	};

	template<typename T>
	class DeviceHandle : public Handle<T> {
	public:
		DeviceHandle(T handle = VK_NULL_HANDLE) noexcept
			: Handle<T>{handle } {
		}

		~DeviceHandle() {
			if (handle_ && device_) {
				device_->destroy(handle_, allocator_);
			}
		}

		DeviceHandle(DeviceHandle&& other) noexcept
			: Handle<T>{ std::move(other) } {
		}

		auto operator=(DeviceHandle&& other) noexcept -> DeviceHandle& {
			Handle<T>::operator=(std::move(other));
			return *this;
		}

		auto set_name(std::string_view name) const -> vk::Result {
			vk::DebugUtilsObjectNameInfoEXT name_info{ T::objectType, reinterpret_cast<uint64_t>(static_cast<T::CType>(handle_)), name.data() };
			return device_->setDebugUtilsObjectNameEXT(&name_info);
		}

		auto set_tag(uint64_t tag_name, const void* tag_data, size_t tag_size) const -> vk::Result {
			vk::DebugUtilsObjectTagInfoEXT tag_info{ T::objectType, reinterpret_cast<uint64_t>(static_cast<T::CType>(handle_)), tag_name, tag_size, tag_data };
			return device_->setDebugUtilsObjectTagEXT(&tag_info);
		}
	protected:
		using Handle<T>::handle_;
	};

	class Queue : public Handle<vk::Queue> {
	public:
		Queue(vk::Queue handle = VK_NULL_HANDLE, uint32_t family_index = ~0u, uint32_t queue_index = ~0u) noexcept
			: Handle{ handle }
			, family_index_{ family_index }
			, queue_index_{ queue_index } {
		}

		auto create_command_pool() const -> Result<CommandPool>;

		// create command pool
		auto get_family_index() const -> uint32_t { return family_index_; }
		auto get_queue_index() const -> uint32_t { return queue_index_; }
	private:
		uint32_t family_index_{ ~0u };
		uint32_t queue_index_{ ~0u };
	};

	class Fence : public DeviceHandle<vk::Fence> {
	public:
		Fence(vk::Fence handle = VK_NULL_HANDLE) noexcept
			: DeviceHandle{ handle } {
		}

		static auto create(vk::FenceCreateFlags flags) -> Result<Fence>;

		auto wait(uint64_t timeout = std::numeric_limits<uint64_t>::max()) const -> vk::Result;
		auto reset() const -> vk::Result;
	};

	class Semaphore : public DeviceHandle<vk::Semaphore> {
	public:
		Semaphore(vk::Semaphore handle = VK_NULL_HANDLE) noexcept
			: DeviceHandle{ handle } {
		}

		static auto create(vk::SemaphoreType type = vk::SemaphoreType::eBinary, uint64_t initial_value = 0ull) -> Result<Semaphore>;

		auto signal(uint64_t value) const -> vk::Result;
	};

	class MemoryAllocator : public NonCopyable {
	public:
		MemoryAllocator(VmaAllocator handle = VK_NULL_HANDLE) noexcept
			: handle_{ handle } {

		}

		~MemoryAllocator();

		MemoryAllocator(MemoryAllocator&& other) noexcept
			: handle_{ std::exchange(other.handle_, VK_NULL_HANDLE) } {
		}

		auto operator=(MemoryAllocator&& other) noexcept -> MemoryAllocator& {
			handle_ = std::exchange(other.handle_, VK_NULL_HANDLE);
			return *this;
		}

		auto allocate_image(const vk::ImageCreateInfo& create_info, const VmaAllocationCreateInfo& allocation_create_info) const -> Result<Image>;
		auto allocate_buffer(const vk::BufferCreateInfo& create_info, const VmaAllocationCreateInfo& allocation_create_info) const -> Result<Buffer>;

		auto operator*() const noexcept -> VmaAllocator const& { return handle_; }
		auto operator*() noexcept -> VmaAllocator& { return handle_; }

		operator VmaAllocator() const noexcept { return handle_; }
		auto get_handle() const noexcept -> VmaAllocator { return handle_; }
	private:
		VmaAllocator handle_{ VK_NULL_HANDLE };
	};

	template<typename T>
	class MemoryAllocation : public Handle<T> {
	public:
		MemoryAllocation(T handle = VK_NULL_HANDLE, VmaAllocation allocation = VK_NULL_HANDLE, VmaAllocationInfo allocation_info = {}) noexcept
			: Handle<T>{ handle }
			, allocation_{ allocation }
			, allocation_info_{ allocation_info } {

			if (memory_allocator_ && allocation_) {
				VkMemoryPropertyFlags memory_properties;
				vmaGetAllocationMemoryProperties(*memory_allocator_, allocation_, &memory_properties);

				coherent_ = (memory_properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
				persistent_ = allocation_info_.pMappedData != nullptr;
				mapped_memory_ = static_cast<uint8_t*>(allocation_info.pMappedData);
			}
		}

		~MemoryAllocation() {
			if (handle_ && allocation_ && memory_allocator_) {
				unmap();

				if constexpr (std::is_same_v<T, vk::Image>) {
					vmaDestroyImage(*memory_allocator_, static_cast<VkImage>(handle_), allocation_);
				}
				else if constexpr (std::is_same_v<T, vk::Buffer>) {
					vmaDestroyBuffer(*memory_allocator_, static_cast<VkBuffer>(handle_), allocation_);
				}

				handle_ = VK_NULL_HANDLE;
				allocation_ = VK_NULL_HANDLE;
			}
		}

		MemoryAllocation(MemoryAllocation&& other) noexcept
			: Handle<T>{ std::move(other) }
			, allocation_{ std::exchange(other.allocation_, VK_NULL_HANDLE) }
			, allocation_info_{ std::exchange(other.allocation_info_, {}) }
			, coherent_{ std::exchange(other.coherent_, {}) }
			, persistent_{ std::exchange(other.persistent_, {}) }
			, mapped_memory_{ std::exchange(other.mapped_memory_, nullptr) } {
		}

		auto operator=(MemoryAllocation&& other) noexcept -> MemoryAllocation& {
			Handle<T>::operator=(std::move(other));
			allocation_ = std::exchange(other.allocation_, VK_NULL_HANDLE);
			allocation_info_ = std::exchange(other.allocation_info_, {});
			coherent_ = std::exchange(other.coherent_, {});
			persistent_ = std::exchange(other.persistent_, {});
			mapped_memory_ = std::exchange(other.mapped_memory_, nullptr);
			return *this;
		}

		auto map() const -> Result<Span<uint8_t>> {
			if (!allocation_) {
				return std::unexpected(vk::Result::eErrorInitializationFailed);
			}

			if (!persistent_ && !is_mapped()) {
				if (auto result = vmaMapMemory(*memory_allocator_, allocation_, reinterpret_cast<void**>(&mapped_memory_)); result != VK_SUCCESS) {
					return std::unexpected(static_cast<vk::Result>(result));
				}
			}
			return Span<uint8_t>(mapped_memory_, allocation_info_.size);
		}

		auto unmap() const -> void {
			if (!allocation_) {
				return;
			}

			if (!persistent_ && is_mapped()) {
				vmaUnmapMemory(*memory_allocator_, allocation_);
				mapped_memory_ = nullptr;
			}
		}

		auto flush(vk::DeviceSize offset = 0, vk::DeviceSize size = VK_WHOLE_SIZE) const -> vk::Result {
			if (!allocation_) {
				return vk::Result::eErrorInitializationFailed;
			}

			if (!coherent_) {
				return static_cast<vk::Result>(vmaFlushAllocation(*memory_allocator_, allocation_, offset, size));
			}
			return vk::Result::eSuccess;
		}

		auto update(std::span<const uint8_t> data, uint64_t offset) const -> vk::Result {
			return update(data.data(), data.size(), offset);
		}

		auto update(const void* data, uint64_t size, uint64_t offset) const -> vk::Result {
			if (!allocation_) {
				return vk::Result::eErrorInitializationFailed;
			}

			if (persistent_) {
				std::memcpy(mapped_memory_ + offset, data, size);
				return flush(offset, size);
			}
			else {
				if (auto result = map(); !result) {
					return result.error();
				}

				std::memcpy(mapped_memory_ + offset, data, size);
				unmap();

				return flush(offset, size);
			}

			return vk::Result::eSuccess;
		}

		auto get_size() const -> uint64_t { return allocation_info_.size; }

		auto is_coherent() const -> bool { return coherent_; }
		auto is_persistent() const -> bool { return persistent_; }
		auto is_mapped() const -> bool { return mapped_memory_ != nullptr; }

		auto get_memory() const -> vk::DeviceMemory { return (vk::DeviceMemory)allocation_info_.deviceMemory; }
		auto get_allocation() const -> VmaAllocation { return allocation_; }
	protected:
		using Handle<T>::handle_;

		VmaAllocation allocation_{ VK_NULL_HANDLE };
		VmaAllocationInfo allocation_info_{};
		bool coherent_{ false };
		bool persistent_{ false };
		mutable uint8_t* mapped_memory_{ nullptr };
	};

	struct ImageCreateInfo {
		vk::Extent3D extent{ 1u, 1u, 1u };
		uint32_t layer_count{ 1u };
		uint32_t level_count{ 1u };
		ImageFlags flags{};
		vk::Format format{ vk::Format::eUndefined };
	};

	class Image : public MemoryAllocation<vk::Image> {
	public:
		Image() = default;

		Image(vk::Image handle, const vk::ImageCreateInfo& create_info) noexcept
			: MemoryAllocation{ handle, VK_NULL_HANDLE, {} }
			, create_info_{ create_info } {

		}

		Image(vk::Image handle, VmaAllocation allocation, VmaAllocationInfo allocation_info, const vk::ImageCreateInfo& create_info) noexcept
			: MemoryAllocation{ handle, allocation, allocation_info }
			, create_info_{ create_info } {
		}

		Image(Image&& other) noexcept
			: MemoryAllocation{ std::move(other) }
			, create_info_{ std::exchange(other.create_info_, {}) } {
		}

		auto operator=(Image&& other) noexcept -> Image& {
			MemoryAllocation::operator=(std::move(other));
			create_info_ = std::exchange(other.create_info_, {});
			return *this;
		}

		static auto create(const ImageCreateInfo& create_info) -> Result<Image>;
		auto create_view(const vk::ImageSubresourceRange& range, vk::ImageViewType type) -> Result<ImageView>;

		auto get_extent() const -> vk::Extent3D { return create_info_.extent; }
		auto get_layer_count() const -> uint32_t { return create_info_.arrayLayers; }
		auto get_level_count() const -> uint32_t { return create_info_.mipLevels; }
		auto get_format() const -> vk::Format { return create_info_.format; }
	private:
		vk::ImageCreateInfo create_info_;
	};

	class ImageView : public DeviceHandle<vk::ImageView> {
	public:
		ImageView(vk::ImageView handle = VK_NULL_HANDLE, const vk::ImageSubresourceRange& subresource_range = {}) noexcept
			: DeviceHandle{ handle }
			, subresource_range_{ subresource_range } {
		}

		auto get_first_layer() const -> uint32_t { return subresource_range_.baseArrayLayer; }
		auto get_layer_count() const -> uint32_t { return subresource_range_.layerCount; }
		auto get_first_level() const -> uint32_t { return subresource_range_.baseMipLevel; }
		auto gat_level_count() const -> uint32_t { return subresource_range_.levelCount; }
		auto get_subresource_range() const -> vk::ImageSubresourceRange const& { return subresource_range_; }
	private:
		vk::ImageSubresourceRange subresource_range_;
	};

	struct BufferCreateInfo {
		vk::DeviceSize size{ 1u };;
		vk::DeviceSize count{ 1u };
		vk::DeviceSize minimal_alignment{ 1u };
		BufferFlags flags{};
	};

	class Buffer : public MemoryAllocation<vk::Buffer> {
	public:
		Buffer(vk::Buffer handle = VK_NULL_HANDLE, VmaAllocation allocation = VK_NULL_HANDLE, VmaAllocationInfo allocation_info = {}, const vk::BufferCreateInfo& create_info = {}) noexcept
			: MemoryAllocation{ handle, allocation, allocation_info }
			, create_info_{ create_info } {
		}

		Buffer(Buffer&& other) noexcept
			: MemoryAllocation{ std::move(other) }
			, create_info_{ std::exchange(other.create_info_, {}) } {
		}

		auto operator=(Buffer&& other) noexcept -> Buffer& {
			MemoryAllocation::operator=(std::move(other));
			create_info_ = std::exchange(other.create_info_, {});
			return *this;
		}

		static auto create(const BufferCreateInfo& create_info) -> Result<Buffer>;
		auto create_view(vk::DeviceSize size, vk::DeviceSize offset = 0ull, vk::Format format = vk::Format::eUndefined) -> Result<BufferView>;
	private:
		vk::BufferCreateInfo create_info_;
	};

	class BufferRange : public NonCopyable {
	public:
		BufferRange(vk::Buffer buffer = VK_NULL_HANDLE, vk::DeviceSize offset = 0ull) noexcept
			: buffer_{ buffer }
			, offset_{ offset } {

		}

		BufferRange(BufferRange&& other) noexcept
			: buffer_{ std::move(other.buffer_) }
			, range_{ std::exchange(other.range_, {}) }
			, offset_{ std::exchange(other.offset_, {}) } {
		}

		auto operator=(BufferRange&& other) noexcept -> BufferRange& {
			buffer_ = std::move(other.buffer_);
			range_ = std::exchange(other.range_, {});
			offset_ = std::exchange(other.offset_, {});
			return *this;
		}

		static auto create(Buffer const* buffer = nullptr, vk::DeviceSize offset = 0ull, vk::DeviceSize size = 0ull) -> Result<BufferRange>;

		auto get_offset() const -> vk::DeviceSize { return offset_; }
		auto get_range() const -> Span<uint8_t> { return range_; }
		auto get_buffer() const -> vk::Buffer { return buffer_; }
	private:
		auto _construct(Buffer const* buffer, vk::DeviceSize size) -> vk::Result;

		vk::Buffer buffer_{ VK_NULL_HANDLE };
		vk::DeviceSize offset_{ 0ull };
		Span<uint8_t> range_;
	};

	class BufferView : public DeviceHandle<vk::BufferView> {
	public:
		BufferView(vk::BufferView handle = VK_NULL_HANDLE, vk::DeviceSize size = 0ull, vk::DeviceSize offset = 0ull, vk::Format format = vk::Format::eUndefined) noexcept
			: DeviceHandle{ handle }
			, size_{ size }
			, offset_{ offset }
			, format_{ format } {
		}

		auto get_offset() const -> uint64_t { return offset_; }
		auto get_size() const -> uint64_t { return size_; }
		auto get_format() const -> vk::Format { return format_; }
	private:
		vk::Format format_;
		vk::DeviceSize size_;
		vk::DeviceSize offset_;
	};

	struct ContextInfo {
		vk::PhysicalDeviceType preferred_device_type{ vk::PhysicalDeviceType::eDiscreteGpu };
		platform::IPlatformWindow* window{ nullptr };

		const char* application_name{ "MyApp" };
		const char* engine_name{ "EdgeGameEngine" };
		uint32_t minimal_api_version{ VK_API_VERSION_1_2 };
	};

	class Swapchain : public DeviceHandle<vk::SwapchainKHR> {
	public:
		struct State {
			uint32_t image_count;
			vk::SurfaceFormatKHR format;
			vk::Extent2D extent;
			vk::SurfaceTransformFlagBitsKHR transform;
			bool vsync;
			bool hdr;
		};

		Swapchain(vk::SwapchainKHR handle = VK_NULL_HANDLE, const State& new_state = {}) noexcept
			: DeviceHandle{ handle }
			, state_{ new_state } {

		}

		Swapchain(Swapchain&& other) noexcept
			: DeviceHandle{ std::move(other) }
			, state_{ std::exchange(other.state_, {}) } {
		}

		auto operator=(Swapchain&& other) noexcept -> Swapchain& {
			DeviceHandle::operator=(std::move(other));
			state_ = std::exchange(other.state_, {});
			return *this;
		}

		auto reset() -> void;

		auto get_images() const -> Result<mi::Vector<Image>>;

		auto get_state() const -> State const& { return state_; }
		auto get_image_count() const noexcept -> uint32_t { return state_.image_count; }
		auto get_format() const noexcept -> vk::Format { return state_.format.format; }
		auto get_color_space() const noexcept -> vk::ColorSpaceKHR { return state_.format.colorSpace; }
		auto get_extent() const noexcept -> vk::Extent2D { return state_.extent; }
		auto get_width() const noexcept -> uint32_t { return state_.extent.width; }
		auto get_height() const noexcept -> uint32_t { return state_.extent.height; }
		auto get_transform() const noexcept -> vk::SurfaceTransformFlagBitsKHR { return state_.transform; }
		auto is_vsync_enabled() const noexcept -> bool { return state_.vsync; }
		auto is_hdr_enabled() const noexcept -> bool { return state_.hdr; }
	private:
		State state_;
	};

	class SwapchainBuilder {
	public:
		SwapchainBuilder() = default;

		auto set_image_count(uint32_t count) -> SwapchainBuilder& {
			requested_state_.image_count = count;
			return *this;
		}

		auto set_image_format(vk::Format format) -> SwapchainBuilder& {
			requested_state_.format.format = format;
			return *this;
		}

		auto set_color_space(vk::ColorSpaceKHR color_space) -> SwapchainBuilder& {
			requested_state_.format.colorSpace = color_space;
			return *this;
		}

		auto set_image_extent(vk::Extent2D extent) -> SwapchainBuilder& {
			requested_state_.extent = extent;
			return *this;
		}

		auto set_image_extent(uint32_t width, uint32_t height) -> SwapchainBuilder& {
			requested_state_.extent = vk::Extent2D{ width, height };
			return *this;
		}

		auto enable_vsync(bool enable) -> SwapchainBuilder& {
			requested_state_.vsync = enable;
			return *this;
		}

		auto enable_hdr(bool enable) -> SwapchainBuilder& {
			requested_state_.hdr = enable;
			return *this;
		}

		auto set_old_swapchain(vk::SwapchainKHR old_swapchain) -> SwapchainBuilder& {
			old_swapchain_ = old_swapchain;
			return *this;
		}

		auto build() -> Result<Swapchain>;
	private:
		static auto choose_suitable_extent(vk::Extent2D request_extent, const vk::SurfaceCapabilitiesKHR& surface_caps) -> vk::Extent2D;
		static auto choose_surface_format(const vk::SurfaceFormatKHR requested_surface_format, const mi::Vector<vk::SurfaceFormatKHR>& available_surface_formats, bool prefer_hdr = false) -> vk::SurfaceFormatKHR;
		static auto choose_suitable_composite_alpha(vk::CompositeAlphaFlagBitsKHR request_composite_alpha, vk::CompositeAlphaFlagsKHR supported_composite_alpha) -> vk::CompositeAlphaFlagBitsKHR;
		static auto choose_suitable_present_mode(vk::PresentModeKHR request_present_mode, std::span<const vk::PresentModeKHR> available_present_modes, std::span<const vk::PresentModeKHR> present_mode_priority_list) -> vk::PresentModeKHR;

		Swapchain::State requested_state_{};
		vk::SwapchainKHR old_swapchain_{ VK_NULL_HANDLE };
	};

	struct ImageBarrier {
		Image const* image;
		ResourceStateFlags src_state;
		ResourceStateFlags dst_state;
		vk::ImageSubresourceRange subresource_range;
	};

	struct Barrier {
		Span<const ImageBarrier> image_barriers;
	};

	class CommandBuffer : public Handle<vk::CommandBuffer> {
	public:
		CommandBuffer(vk::CommandPool command_pool = VK_NULL_HANDLE, vk::CommandBuffer handle = VK_NULL_HANDLE) noexcept
			: Handle{ handle }
			, command_pool_{ command_pool } {

		}

		~CommandBuffer() {
			if (handle_ && command_pool_) {
				device_->freeCommandBuffers(command_pool_, 1u, &handle_);
				handle_ = VK_NULL_HANDLE;
				command_pool_ = VK_NULL_HANDLE;
			}
		}

		CommandBuffer(CommandBuffer&& other) noexcept
			: Handle(std::move(other))
			, command_pool_{ std::exchange(other.command_pool_, VK_NULL_HANDLE) } {
		}

		auto operator=(CommandBuffer&& other) noexcept -> CommandBuffer& {
			Handle::operator=(std::move(other));
			command_pool_ = std::exchange(other.command_pool_, VK_NULL_HANDLE);
			return *this;
		}

		auto begin() const -> vk::Result;
		auto end() const -> vk::Result;

		auto push_barrier(const Barrier& barrier) const -> void;
		auto push_barrier(const ImageBarrier& barrier) const -> void;

		auto begin_marker(std::string_view name, uint32_t color = 0xFFFFFFFF) const -> void;
		auto end_marker() const -> void;
	private:
		vk::CommandPool command_pool_{ VK_NULL_HANDLE };
	};

	class CommandPool : public DeviceHandle<vk::CommandPool> {
	public:
		CommandPool(vk::CommandPool handle = VK_NULL_HANDLE) noexcept
			: DeviceHandle{ handle } {

		}

		auto allocate_command_buffer(vk::CommandBufferLevel level = vk::CommandBufferLevel::ePrimary) const -> Result<CommandBuffer>;
	};

	class Sampler : public DeviceHandle<vk::Sampler> {
	public:
		Sampler(vk::Sampler handle = VK_NULL_HANDLE, const vk::SamplerCreateInfo& create_info = {}) noexcept
			: DeviceHandle{ handle }
			, create_info_{ create_info } {
		}

		static auto create(const vk::SamplerCreateInfo& create_info) -> Result<Sampler>;

		// TODO: implement move 

		auto get_mag_filter() const -> vk::Filter { return create_info_.magFilter; }
		auto get_min_filter() const -> vk::Filter { return create_info_.minFilter; }
		auto get_mipmap_mode() const -> vk::SamplerMipmapMode { return create_info_.mipmapMode; }
		auto get_address_mode_u() const -> vk::SamplerAddressMode { return create_info_.addressModeU; }
		auto get_address_mode_v() const -> vk::SamplerAddressMode { return create_info_.addressModeV; }
		auto get_address_mode_w() const -> vk::SamplerAddressMode { return create_info_.addressModeW; }
		auto get_mip_lod_bias() const -> float { return create_info_.mipLodBias; }
		auto get_anisotropy_enable() const -> bool { return create_info_.anisotropyEnable == VK_TRUE; }
		auto get_max_anisotropy() const -> float { return create_info_.maxAnisotropy; }
		auto get_compare_enable() const -> bool { return create_info_.compareEnable == VK_TRUE; }
		auto get_compare_op() const -> vk::CompareOp { return create_info_.compareOp; }
		auto get_min_lod() const -> float { return create_info_.minLod; }
		auto get_max_lod() const -> float { return create_info_.maxLod; }
		auto get_border_color() const -> vk::BorderColor { return create_info_.borderColor; }
	private:
		vk::SamplerCreateInfo create_info_;
	};

	class QueryPool : public DeviceHandle<vk::QueryPool> {
	public:
		QueryPool(vk::QueryPool handle = VK_NULL_HANDLE, vk::QueryType type = {}, uint32_t max_query = 0u) noexcept
			: DeviceHandle{ handle }
			, type_{ type }
			, max_query_{ max_query } {
		}

		QueryPool(QueryPool&& other) noexcept
			: DeviceHandle(std::move(other))
			, type_{ std::exchange(other.type_, {}) }
			, max_query_{ std::exchange(other.max_query_, {}) } {
		}

		auto operator=(QueryPool&& other) noexcept -> QueryPool& {
			DeviceHandle::operator=(std::move(other));
			type_ = std::exchange(other.type_, {});
			max_query_ = std::exchange(other.max_query_, {});
			return *this;
		}

		static auto create(vk::QueryType type, uint32_t query_count) -> Result<QueryPool>;

		auto get_data(uint32_t query_index, void* data) const -> vk::Result;
		auto get_data(uint32_t first_query, uint32_t query_count, void* data) const -> vk::Result;

		auto reset(uint32_t start_query, uint32_t query_count = 0u) const -> void;
	private:
		vk::QueryType type_;
		uint32_t max_query_;
	};

	class PipelineCache : public DeviceHandle<vk::PipelineCache> {
	public:
		PipelineCache(vk::PipelineCache handle = VK_NULL_HANDLE) noexcept
			: DeviceHandle{ handle } {
		}

		static auto create(Span<const uint8_t> data) -> Result<PipelineCache>;

		auto get_data(mi::Vector<uint8_t>& data) const -> vk::Result;
		auto get_data(void*& data, size_t& size) const -> vk::Result;
	};

	class Pipeline : public DeviceHandle<vk::Pipeline> {
	public:
		Pipeline(vk::Pipeline handle = VK_NULL_HANDLE) noexcept
			: DeviceHandle{ handle } {
		}

		// TODO: add static create functions
	};

	class ShaderModule : public DeviceHandle<vk::ShaderModule> {
	public:
		ShaderModule(vk::ShaderModule handle = VK_NULL_HANDLE) noexcept
			: DeviceHandle{ handle } {
		}

		static auto create(Span<const uint8_t> code) -> Result<ShaderModule>;
	private:
		vk::PipelineShaderStageCreateInfo shader_stage_create_info_;
	};

	class PipelineLayout : public DeviceHandle<vk::PipelineLayout> {
	public:
		PipelineLayout(vk::PipelineLayout handle = VK_NULL_HANDLE) noexcept
			: DeviceHandle{ handle } {
		}
	};

	struct PoolSizes {
		using counting_type = int32_t;
		static constexpr int32_t k_max_sizes{ 11 };

		std::array<counting_type, k_max_sizes> sizes_{};

		// Arithmetic operators (compound-assignment)
		PoolSizes& operator+=(const PoolSizes& rhs) {
			std::transform(sizes_.begin(), sizes_.end(), rhs.sizes_.begin(), sizes_.begin(), std::plus{});
			return *this;
		}

		PoolSizes& operator-=(const PoolSizes& rhs) {
			std::transform(sizes_.begin(), sizes_.end(), rhs.sizes_.begin(), sizes_.begin(), std::minus{});
			return *this;
		}

		PoolSizes& operator*=(const PoolSizes& rhs) {
			std::transform(sizes_.begin(), sizes_.end(), rhs.sizes_.begin(), sizes_.begin(), std::multiplies{});
			return *this;
		}

		PoolSizes& operator/=(const PoolSizes& rhs) {
			std::transform(sizes_.begin(), sizes_.end(), rhs.sizes_.begin(), sizes_.begin(), std::divides{});
			return *this;
		}

		// Scalar arithmetic operators (compound-assignment)
		template<Arithmetic T>
		PoolSizes& operator+=(T rhs) {
			for (auto& s : sizes_) s += rhs;
			return *this;
		}

		template<Arithmetic T>
		PoolSizes& operator-=(T rhs) {
			for (auto& s : sizes_) s -= rhs;
			return *this;
		}

		template<Arithmetic T>
		PoolSizes& operator*=(T rhs) {
			for (auto& s : sizes_) s *= rhs;
			return *this;
		}

		template<Arithmetic T>
		PoolSizes& operator/=(T rhs) {
			for (auto& s : sizes_) s /= rhs;
			return *this;
		}

		// Non-member arithmetic operators
		friend PoolSizes operator+(PoolSizes lhs, const PoolSizes& rhs) { return lhs += rhs; }
		friend PoolSizes operator-(PoolSizes lhs, const PoolSizes& rhs) { return lhs -= rhs; }
		friend PoolSizes operator*(PoolSizes lhs, const PoolSizes& rhs) { return lhs *= rhs; }
		friend PoolSizes operator/(PoolSizes lhs, const PoolSizes& rhs) { return lhs /= rhs; }

		template<Arithmetic T>
		friend PoolSizes operator+(PoolSizes lhs, T rhs) { return lhs += rhs; }
		template<Arithmetic T>
		friend PoolSizes operator-(PoolSizes lhs, T rhs) { return lhs -= rhs; }
		template<Arithmetic T>
		friend PoolSizes operator*(PoolSizes lhs, T rhs) { return lhs *= rhs; }
		template<Arithmetic T>
		friend PoolSizes operator/(PoolSizes lhs, T rhs) { return lhs /= rhs; }

		bool operator==(const PoolSizes&) const = default;

		auto operator<=>(const PoolSizes& rhs) const {
			return sizes_ <=> rhs.sizes_;
		}

		counting_type& operator[](size_t idx) { return sizes_[idx]; }
		const counting_type& operator[](size_t idx) const { return sizes_[idx]; }
	};

	class DescriptorSetLayoutBuilder : public NonCopyable {
	public:
		DescriptorSetLayoutBuilder() = default;

		auto add_binding(uint32_t binding, vk::DescriptorType descriptor_type, uint32_t descriptor_count, vk::ShaderStageFlags stage_flags, vk::DescriptorBindingFlagsEXT binding_flags = {}) -> DescriptorSetLayoutBuilder& {
			pool_sizes_[static_cast<uint32_t>(descriptor_type)] += descriptor_count;
			layout_bindings_.emplace_back(binding, descriptor_type, descriptor_count, stage_flags, nullptr);
			binding_flags_.emplace_back(binding_flags);
			return *this;
		}

		auto build(vk::DescriptorSetLayoutCreateFlags flags = {}) -> Result<DescriptorSetLayout>;
	private:
		mi::Vector<vk::DescriptorSetLayoutBinding> layout_bindings_{};
		mi::Vector<vk::DescriptorBindingFlagsEXT> binding_flags_{};
		PoolSizes pool_sizes_{};
	};

	class PipelineLayoutBuilder : public NonCopyable {
	public:
		PipelineLayoutBuilder() = default;

		auto add_set_layout(DescriptorSetLayout const& set_layout) -> PipelineLayoutBuilder&;

		inline auto add_constant_range(vk::PushConstantRange const& range) -> PipelineLayoutBuilder& {
			push_constant_ranges_.push_back(range);
			return *this;
		}

		inline auto add_constant_range(vk::ShaderStageFlags stage_flags, uint32_t offset, uint32_t size) -> PipelineLayoutBuilder& {
			push_constant_ranges_.emplace_back(stage_flags, offset, size);
			return *this;
		}

		auto build() -> Result<PipelineLayout>;
	private:
		mi::Vector<vk::DescriptorSetLayout> descriptor_set_layouts_{};
		mi::Vector<vk::PushConstantRange> push_constant_ranges_{};
	};

	class DescriptorSetLayout : public DeviceHandle<vk::DescriptorSetLayout> {
	public:
		DescriptorSetLayout(vk::DescriptorSetLayout handle = VK_NULL_HANDLE, PoolSizes pool_sizes = {}) noexcept
			: DeviceHandle{ handle }
			, pool_sizes_{ pool_sizes } {
		}

		auto get_pool_sizes() const -> PoolSizes const& {
			return pool_sizes_;
		}

	private:
		PoolSizes pool_sizes_{};
	};

	class DescriptorPool : public DeviceHandle<vk::DescriptorPool> {
	public:
		DescriptorPool(vk::DescriptorPool handle = VK_NULL_HANDLE) noexcept
			: DeviceHandle{ handle } {
		}

		static auto create(PoolSizes const& requested_sizes, uint32_t max_descriptor_sets, vk::DescriptorPoolCreateFlags flags = {}) -> Result<DescriptorPool>;

		auto allocate_descriptor_set(DescriptorSetLayout const& layout) const -> Result<DescriptorSet>;
		auto free_descriptor_set(DescriptorSet const& set) const -> void;
	};

	class DescriptorSet : public Handle<vk::DescriptorSet> {
	public:
		DescriptorSet(vk::DescriptorSet handle = VK_NULL_HANDLE, PoolSizes const& pool_sizes = {}) noexcept
			: Handle{ handle }
			, pool_sizes_{ pool_sizes } {
		}

		auto get_pool_sizes() const -> PoolSizes const& {
			return pool_sizes_;
		}

	private:
		PoolSizes pool_sizes_{};
	};

	auto initialize_graphics(const ContextInfo& info) -> vk::Result;
	auto shutdown_graphics() -> void;

	// TODO: Queue selection not finished
	auto get_queue(QueueType type) -> Result<Queue>;
}