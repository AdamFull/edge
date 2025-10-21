#pragma once

#include "gfx_base.h"
#include "gfx_enum.h"

namespace edge::platform {
	class IPlatformWindow;
}

namespace edge::gfx {
	class Context;
	class Instance;
	class Surface;
	class Adapter;
	class Device;
	class Queue;
	class MemoryAllocator;
	class Image;
	class Buffer;
	class CommandPool;
	class CommandBuffer;
	class QueryPool;
	class PipelineCache;
	class Pipeline;
	class PipelineLayout;
	class DescriptorSetLayout;
	class DescriptorPool;
	class DescriptorSet;

	template<typename T>
	class Handle : public NonCopyable {
	public:
		Handle(std::nullptr_t) noexcept {};

		Handle(T handle = VK_NULL_HANDLE, vk::AllocationCallbacks const* allocator = nullptr)
			: handle_{ handle }
			, allocator_{ allocator } {
		}

		Handle(Handle&& other)
			: handle_{ std::exchange(other.handle_, VK_NULL_HANDLE) }
			, allocator_{ std::exchange(other.allocator_, nullptr) } {
		}

		auto operator=(Handle&& other) -> Handle& {
			handle_ = std::exchange(other.handle_, VK_NULL_HANDLE);
			allocator_ = std::exchange(other.allocator_, nullptr);
			return *this;
		}

		auto operator*() const noexcept -> T const& { return handle_; }
		auto operator*() noexcept -> T& { return handle_; }

		auto operator->() const noexcept -> T const* { return &handle_; }
		auto operator->() noexcept -> T* { return &handle_; }

		operator bool() const noexcept { return handle_; }
		operator T() const noexcept { return handle_; }
		operator typename T::CType() const noexcept { return handle_; }
		auto get_handle() const noexcept -> T { return handle_; }

		auto get_allocator() const -> vk::AllocationCallbacks const* { return allocator_; }
	protected:
		T handle_{ VK_NULL_HANDLE };
		vk::AllocationCallbacks const* allocator_{ nullptr };
	};

	class Instance : public Handle<vk::Instance> {
	public:
		Instance(std::nullptr_t) noexcept {};
		Instance(vk::Instance handle, vk::DebugUtilsMessengerEXT debug_messenger, vk::AllocationCallbacks const* allocator, mi::Vector<const char*>&& enabled_extensions, mi::Vector<const char*> enabled_layers);
		~Instance();

		Instance(Instance&& other)
			: Handle(std::move(other))
			, debug_messenger_{ std::exchange(other.debug_messenger_, VK_NULL_HANDLE) }
			, enabled_extensions_{ std::exchange(other.enabled_extensions_, {}) }
			, enabled_layers_{ std::exchange(other.enabled_layers_, {}) } {
		}

		auto operator=(Instance&& other) -> Instance& {
			Handle::operator=(std::move(other));
			debug_messenger_ = std::exchange(other.debug_messenger_, VK_NULL_HANDLE);
			enabled_extensions_ = std::exchange(other.enabled_extensions_, {});
			enabled_layers_ = std::exchange(other.enabled_layers_, {});
			return *this;
		}

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
		auto create_surface(const vk::AndroidSurfaceCreateInfoKHR& create_info) const -> Result<Surface>;
#elif defined(VK_USE_PLATFORM_WIN32_KHR)
		auto create_surface(const vk::Win32SurfaceCreateInfoKHR& create_info) const -> Result<Surface>;
#endif

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
		InstanceHandle(Instance const* instance = nullptr, T handle = VK_NULL_HANDLE)
			: Handle<T>{ handle, instance ? instance->get_allocator() : nullptr }
			, instance_{ instance ? instance->get_handle() : VK_NULL_HANDLE } {
		}

		~InstanceHandle() {
			if (handle_ && instance_) {
				instance_.destroy(handle_, allocator_);
				handle_ = VK_NULL_HANDLE;
			}
		}

		InstanceHandle(InstanceHandle&& other)
			: Handle<T>{ std::move(other) }
			, instance_{ std::exchange(other.instance_, VK_NULL_HANDLE) } {
		}

		auto operator=(InstanceHandle&& other) -> InstanceHandle& {
			Handle<T>::operator=(std::move(other));
			instance_ = std::exchange(other.instance_, VK_NULL_HANDLE);
			return *this;
		}
	protected:
		using Handle<T>::handle_;
		using Handle<T>::allocator_;

		vk::Instance instance_{ VK_NULL_HANDLE };
	};

	class Surface : public InstanceHandle<vk::SurfaceKHR> {
	public:
		Surface(Instance const* instance = nullptr, vk::SurfaceKHR handle = VK_NULL_HANDLE)
			: InstanceHandle{ instance, handle } {

		}
	};

	class InstanceBuilder {
	public:
		InstanceBuilder(vk::AllocationCallbacks const* allocator);

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

		// Build the instance
		auto build() -> Result<Instance>;

		// Get current configuration for inspection
		auto get_app_info() const -> const vk::ApplicationInfo& { return app_info_; }

	private:
		vk::ApplicationInfo app_info_{};
		vk::InstanceCreateInfo create_info_{};
		mi::Vector<std::pair<const char*, bool>> requested_extensions_;
		mi::Vector<std::pair<const char*, bool>> requested_layers_;
		mi::Vector<vk::ValidationFeatureEnableEXT> validation_feature_enables_;
		mi::Vector<vk::ValidationFeatureDisableEXT> validation_feature_disables_;
		const vk::AllocationCallbacks* allocator_{ nullptr };

		bool enable_debug_utils_{ false };
		bool enable_surface_{ false };
		bool enable_portability_{ false };
	};

	class Adapter : public Handle<vk::PhysicalDevice> {
	public:
		Adapter(std::nullptr_t) noexcept {};
		Adapter(vk::PhysicalDevice handle, mi::Vector<vk::ExtensionProperties>&& device_extensions, vk::AllocationCallbacks const* allocator);
		~Adapter() = default;

		Adapter(Adapter&& other)
			: Handle(std::move(other))
			, supported_extensions_{ std::exchange(other.supported_extensions_, {}) } {
		}

		auto operator=(Adapter&& other) -> Adapter& {
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

		Device(std::nullptr_t) noexcept {};
		Device(vk::Device handle, mi::Vector<const char*>&& enabled_extensions, vk::AllocationCallbacks const* allocator, Array<mi::Vector<QueueFamilyInfo>, 3ull>&& queue_family_map);
		~Device();

		Device(Device&& other)
			: Handle(std::move(other))
			, enabled_extensions_{ std::exchange(other.enabled_extensions_, {}) }
			, queue_family_map_{ std::exchange(other.queue_family_map_, {}) } {
		}

		auto operator=(Device&& other) -> Device& {
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
		DeviceSelector(const Instance& instance) 
			: instance_{ &instance }
			, requested_extensions_{}
			, requested_features_{} {
		}

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
		Instance const* instance_{ nullptr };

		vk::SurfaceKHR surface_{ VK_NULL_HANDLE };
		uint32_t minimal_api_ver{ VK_VERSION_1_0 };
		vk::PhysicalDeviceType preferred_type_{ vk::PhysicalDeviceType::eDiscreteGpu };

		mi::Vector<std::pair<const char*, bool>> requested_extensions_;

		mi::Vector<Shared<void>> requested_features_;
		void* last_feature_ptr_{ nullptr };
	};

	template<typename T>
	class DeviceHandle : public Handle<T> {
	public:
		DeviceHandle(Device const* device = nullptr, T handle = VK_NULL_HANDLE)
			: Handle<T>{handle, device ? device->get_allocator() : nullptr }
			, device_{ device } {
		}

		~DeviceHandle() {
			if (handle_ && device_) {
				auto device_handle = device_->get_handle();
				device_handle.destroy(handle_, allocator_);
			}
		}

		DeviceHandle(DeviceHandle&& other)
			: Handle<T>{ std::move(other) }
			, device_{ std::exchange(other.device_, nullptr) } {
		}

		auto operator=(DeviceHandle&& other) -> DeviceHandle& {
			Handle<T>::operator=(std::move(other));
			device_ = std::exchange(other.device_, nullptr);
			return *this;
		}

		auto set_name(std::string_view name) const -> vk::Result {
			auto device_handle = device_->get_handle();

			vk::DebugUtilsObjectNameInfoEXT name_info{ T::objectType, reinterpret_cast<uint64_t>(static_cast<T::CType>(handle_)), name.data() };
			return device_handle.setDebugUtilsObjectNameEXT(&name_info);
		}

		auto set_tag(uint64_t tag_name, const void* tag_data, size_t tag_size) const -> vk::Result {
			auto device_handle = device_->get_handle();

			vk::DebugUtilsObjectTagInfoEXT tag_info{ T::objectType, reinterpret_cast<uint64_t>(static_cast<T::CType>(handle_)), tag_name, tag_size, tag_data };
			return device_handle.setDebugUtilsObjectTagEXT(&tag_info);
		}

		auto get_device() const -> Device const* { return device_; }
	protected:
		using Handle<T>::handle_;
		using Handle<T>::allocator_;

		Device const* device_{ nullptr };
	};

	class Queue : public Handle<vk::Queue> {
	public:
		Queue(Device const* device = nullptr, vk::Queue handle = VK_NULL_HANDLE, uint32_t family_index = ~0u, uint32_t queue_index = ~0u) 
			: Handle{ handle, device ? device->get_allocator() : nullptr }
			, device_{ device }
			, family_index_{ family_index_ }
			, queue_index_{ queue_index } {
		}

		auto create_command_pool() const -> Result<CommandPool>;

		// create command pool
		auto get_family_index() const -> uint32_t { return family_index_; }
		auto get_queue_index() const -> uint32_t { return queue_index_; }
		auto get_device() const -> Device const* { return device_; }
	private:
		Device const* device_{ nullptr };
		uint32_t family_index_{ ~0u };
		uint32_t queue_index_{ ~0u };
	};

	class Fence : public DeviceHandle<vk::Fence> {
	public:
		Fence(Device const* device = nullptr, vk::Fence handle = VK_NULL_HANDLE)
			: DeviceHandle{ device, handle } {
		}

		auto wait(uint64_t timeout = std::numeric_limits<uint64_t>::max()) const -> vk::Result;
		auto reset() const -> vk::Result;
	};

	class Semaphore : public DeviceHandle<vk::Semaphore> {
	public:
		Semaphore(Device const* device = nullptr, vk::Semaphore handle = VK_NULL_HANDLE) 
			: DeviceHandle{ device, handle } {
		}

		auto signal(uint64_t value) const -> vk::Result;
	};

	class MemoryAllocator : public NonCopyable {
	public:
		MemoryAllocator(std::nullptr_t) noexcept {};
		MemoryAllocator(Device const* device = nullptr, VmaAllocator handle = VK_NULL_HANDLE)
			: allocator_{ device ? device->get_allocator() : nullptr }
			, handle_{ handle } {

		}

		~MemoryAllocator();

		MemoryAllocator(MemoryAllocator&& other)
			: allocator_{ std::exchange(other.allocator_, nullptr) }
			, handle_{ std::exchange(other.handle_, VK_NULL_HANDLE) } {
		}

		auto operator=(MemoryAllocator&& other) -> MemoryAllocator& {
			allocator_ = std::exchange(other.allocator_, nullptr);
			handle_ = std::exchange(other.handle_, VK_NULL_HANDLE);
			return *this;
		}

		auto allocate_image(const vk::ImageCreateInfo& create_info, const VmaAllocationCreateInfo& allocation_create_info) const -> Result<Image>;
		auto allocate_buffer(const vk::BufferCreateInfo& create_info, const VmaAllocationCreateInfo& allocation_create_info) const -> Result<Buffer>;

		operator VmaAllocator() const noexcept { return handle_; }
		auto get_handle() const noexcept -> VmaAllocator { return handle_; }
		auto get_allocator() const -> vk::AllocationCallbacks const* { return allocator_; }
	private:
		vk::AllocationCallbacks const* allocator_{ nullptr };
		VmaAllocator handle_{ VK_NULL_HANDLE };
	};

	template<typename T>
	class MemoryAllocation : public Handle<T> {
	public:
		MemoryAllocation(MemoryAllocator const* memory_allocator = nullptr, T handle = VK_NULL_HANDLE, VmaAllocation allocation = VK_NULL_HANDLE, VmaAllocationInfo allocation_info = {})
			: Handle<T>{ handle, memory_allocator ? memory_allocator->get_allocator() : nullptr }
			, allocation_{ allocation }
			, allocation_info_{ allocation_info }
			, memory_allocator_{ memory_allocator } {

			if (memory_allocator_) {
				auto vma_handle = memory_allocator_->get_handle();

				VkMemoryPropertyFlags memory_properties;
				vmaGetAllocationMemoryProperties(vma_handle, allocation_, &memory_properties);

				coherent_ = (memory_properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
				persistent_ = allocation_info_.pMappedData != nullptr;
				mapped_memory_ = static_cast<uint8_t*>(allocation_info.pMappedData);
			}
		}

		~MemoryAllocation() {
			if (handle_ && allocation_ && memory_allocator_) {
				unmap();

				auto vma_handle = memory_allocator_->get_handle();
				if constexpr (std::is_same_v<T, vk::Image>) {
					vmaDestroyImage(vma_handle, static_cast<VkImage>(handle_), allocation_);
				}
				else if constexpr (std::is_same_v<T, vk::Buffer>) {
					vmaDestroyBuffer(vma_handle, static_cast<VkBuffer>(handle_), allocation_);
				}

				handle_ = VK_NULL_HANDLE;
				allocation_ = VK_NULL_HANDLE;
			}
		}

		MemoryAllocation(MemoryAllocation&& other)
			: Handle<T>{ std::move(other) }
			, allocation_{ std::exchange(other.allocation_, VK_NULL_HANDLE) }
			, allocation_info_{ std::exchange(other.allocation_info_, {}) }
			, coherent_{ std::exchange(other.coherent_, {}) }
			, persistent_{ std::exchange(other.persistent_, {}) }
			, mapped_memory_{ std::exchange(other.mapped_memory_, nullptr) }
			, memory_allocator_{ std::exchange(other.memory_allocator_, nullptr) } {
		}

		auto operator=(MemoryAllocation&& other) -> MemoryAllocation& {
			Handle<T>::operator=(std::move(other));
			allocation_ = std::exchange(other.allocation_, VK_NULL_HANDLE);
			allocation_info_ = std::exchange(other.allocation_info_, {});
			coherent_ = std::exchange(other.coherent_, {});
			persistent_ = std::exchange(other.persistent_, {});
			mapped_memory_ = std::exchange(other.mapped_memory_, nullptr);
			memory_allocator_ = std::exchange(other.memory_allocator_, nullptr);
			return *this;
		}

		auto map() const -> Result<Span<uint8_t>> {
			if (!persistent_ && !is_mapped()) {
				auto vma_handle = memory_allocator_->get_handle();
				if (auto result = vmaMapMemory(vma_handle, allocation_, reinterpret_cast<void**>(&mapped_memory_)); result != VK_SUCCESS) {
					return std::unexpected(static_cast<vk::Result>(result));
				}
			}
			return Span<uint8_t>(mapped_memory_, allocation_info_.size);
		}

		auto unmap() const -> void {
			if (!persistent_ && is_mapped()) {
				auto vma_handle = memory_allocator_->get_handle();
				vmaUnmapMemory(vma_handle, allocation_);
				mapped_memory_ = nullptr;
			}
		}

		auto flush(vk::DeviceSize offset = 0, vk::DeviceSize size = VK_WHOLE_SIZE) const -> vk::Result {
			if (!coherent_) {
				auto vma_handle = memory_allocator_->get_handle();
				return static_cast<vk::Result>(vmaFlushAllocation(vma_handle, allocation_, offset, size));
			}
			return vk::Result::eSuccess;
		}

		auto update(std::span<const uint8_t> data, uint64_t offset) const -> vk::Result {
			return update(data.data(), data.size(), offset);
		}

		auto update(const void* data, uint64_t size, uint64_t offset) const -> vk::Result {
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
		auto get_memory_allocator() const -> MemoryAllocator const* { return memory_allocator_; }
	protected:
		using Handle<T>::handle_;
		using Handle<T>::allocator_;

		VmaAllocation allocation_{ VK_NULL_HANDLE };
		VmaAllocationInfo allocation_info_{};
		bool coherent_{ false };
		bool persistent_{ false };
		mutable uint8_t* mapped_memory_{ nullptr };

		MemoryAllocator const* memory_allocator_{ nullptr };
	};

	class Image : public MemoryAllocation<vk::Image> {
	public:
		Image() = default;
		Image(std::nullptr_t) noexcept {};

		Image(vk::Image handle, const vk::ImageCreateInfo& create_info)
			: MemoryAllocation{ nullptr, handle, VK_NULL_HANDLE, {} }
			, create_info_{ create_info } {

		}

		Image(MemoryAllocator const* allocator, vk::Image handle, VmaAllocation allocation, VmaAllocationInfo allocation_info, const vk::ImageCreateInfo& create_info)
			: MemoryAllocation{ allocator, handle, allocation, allocation_info }
			, create_info_{ create_info } {
		}

		Image(Image&& other)
			: MemoryAllocation{ std::move(other) }
			, create_info_{ std::exchange(other.create_info_, {}) } {
		}

		auto operator=(Image&& other) -> Image& {
			MemoryAllocation::operator=(std::move(other));
			create_info_ = std::exchange(other.create_info_, {});
			return *this;
		}

		auto get_extent() const -> vk::Extent3D { return create_info_.extent; }
		auto get_layer_count() const -> uint32_t { return create_info_.arrayLayers; }
		auto get_level_count() const -> uint32_t { return create_info_.mipLevels; }
		auto get_format() const -> vk::Format { return create_info_.format; }
	private:
		vk::ImageCreateInfo create_info_;
	};

	class ImageView : public DeviceHandle<vk::ImageView> {
	public:
		ImageView(Device const* device = nullptr, vk::ImageView handle = VK_NULL_HANDLE, const vk::ImageSubresourceRange& subresource_range = {})
			: DeviceHandle{ device, handle }
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

	class Buffer : public MemoryAllocation<vk::Buffer> {
	public:
		Buffer(MemoryAllocator const* allocator = nullptr, vk::Buffer handle = VK_NULL_HANDLE, VmaAllocation allocation = VK_NULL_HANDLE, VmaAllocationInfo allocation_info = {}, const vk::BufferCreateInfo& create_info = {})
			: MemoryAllocation{ allocator, handle, allocation, allocation_info }
			, create_info_{ create_info } {
		}

		Buffer(Buffer&& other)
			: MemoryAllocation{ std::move(other) }
			, create_info_{ std::exchange(other.create_info_, {}) } {
		}

		auto operator=(Buffer&& other) -> Buffer& {
			MemoryAllocation::operator=(std::move(other));
			create_info_ = std::exchange(other.create_info_, {});
			return *this;
		}
	private:
		vk::BufferCreateInfo create_info_;
	};

	class BufferRange {
	public:
		BufferRange(vk::Buffer buffer = VK_NULL_HANDLE, vk::DeviceSize offset = 0ull)
			: buffer_{ buffer }
			, offset_{ offset } {

		}

		BufferRange(const BufferRange&) = delete;
		auto operator=(const BufferRange&) -> BufferRange & = delete;

		BufferRange(BufferRange&& other)
			: buffer_{ std::exchange(other.buffer_, VK_NULL_HANDLE) }
			, range_{ std::exchange(other.range_, {}) }
			, offset_{ std::exchange(other.offset_, {}) } {
		}

		auto operator=(BufferRange&& other) -> BufferRange& {
			buffer_ = std::exchange(other.buffer_, VK_NULL_HANDLE);
			range_ = std::exchange(other.range_, {});
			offset_ = std::exchange(other.offset_, {});
			return *this;
		}

		static auto construct(Buffer const* buffer = nullptr, vk::DeviceSize offset = 0ull, vk::DeviceSize size = 0ull) -> Result<BufferRange>;

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
		BufferView(Device const* device = nullptr, vk::BufferView handle = VK_NULL_HANDLE, vk::DeviceSize size = 0ull, vk::DeviceSize offset = 0ull, vk::Format format = vk::Format::eUndefined)
			: DeviceHandle{ device, handle }
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

	struct ImageCreateInfo {
		vk::Extent3D extent{ 1u, 1u, 1u };
		uint32_t layer_count{ 1u };
		uint32_t level_count{ 1u };
		ImageFlags flags{};
		vk::Format format{ vk::Format::eUndefined };
	};

	struct BufferCreateInfo {
		vk::DeviceSize size{ 1u };;
		vk::DeviceSize count{ 1u };
		vk::DeviceSize minimal_alignment{ 1u };
		BufferFlags flags{};
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

		Swapchain(Device const* device = nullptr, vk::SwapchainKHR handle = VK_NULL_HANDLE, const State& new_state = {})
			: DeviceHandle{ device, handle }
			, state_{ new_state } {

		}

		Swapchain(Swapchain&& other)
			: DeviceHandle{ std::move(other) }
			, state_{ std::exchange(other.state_, {}) } {
		}

		auto operator=(Swapchain&& other) -> Swapchain& {
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
		SwapchainBuilder(Adapter const& adapter, Device const& device, Surface const& surface)
			: adapter_{ &adapter }
			, device_{ &device }
			, surface_{ &surface } {
		}

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

		Swapchain::State requested_state_;
		vk::SwapchainKHR old_swapchain_{ VK_NULL_HANDLE };

		Adapter const* adapter_{ nullptr };
		Device const* device_{ nullptr };
		Surface const* surface_{ nullptr };
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
		CommandBuffer(Device const* device = nullptr, vk::CommandPool command_pool = VK_NULL_HANDLE, vk::CommandBuffer handle = VK_NULL_HANDLE)
			: Handle{ handle, device ? device->get_allocator() : nullptr }
			, device_{ device }
			, command_pool_{ command_pool } {

		}

		~CommandBuffer() {
			if (handle_ && command_pool_) {
				auto device_handle = device_->get_handle();
				device_handle.freeCommandBuffers(command_pool_, 1u, &handle_);
				handle_ = VK_NULL_HANDLE;
				command_pool_ = VK_NULL_HANDLE;
			}
		}

		CommandBuffer(CommandBuffer&& other)
			: Handle(std::move(other))
			, device_{ std::exchange(other.device_, nullptr) }
			, command_pool_{ std::exchange(other.command_pool_, VK_NULL_HANDLE) } {
		}

		auto operator=(CommandBuffer&& other) -> CommandBuffer& {
			Handle::operator=(std::move(other));
			device_ = std::exchange(other.device_, nullptr);
			command_pool_ = std::exchange(other.command_pool_, VK_NULL_HANDLE);
			return *this;
		}

		auto begin() const -> vk::Result;
		auto end() const -> vk::Result;

		auto push_barrier(const Barrier& barrier) const -> void;
		auto push_barrier(const ImageBarrier& barrier) const -> void;
	private:
		Device const* device_{ nullptr };
		vk::CommandPool command_pool_{ VK_NULL_HANDLE };
	};

	class CommandPool : public DeviceHandle<vk::CommandPool> {
	public:
		CommandPool(Device const* device = nullptr, vk::CommandPool handle = VK_NULL_HANDLE)
			: DeviceHandle{ device, handle } {

		}

		auto allocate_command_buffer(vk::CommandBufferLevel level = vk::CommandBufferLevel::ePrimary) const -> Result<CommandBuffer>;
	};

	class Sampler : public DeviceHandle<vk::Sampler> {
	public:
		Sampler(Device const* device = nullptr, vk::Sampler handle = VK_NULL_HANDLE, const vk::SamplerCreateInfo& create_info = {})
			: DeviceHandle{ device, handle }
			, create_info_{ create_info } {
		}

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
		QueryPool(Device const* device = nullptr, vk::QueryPool handle = VK_NULL_HANDLE, vk::QueryType type = {}, uint32_t max_query = 0u)
			: DeviceHandle{ device, handle }
			, type_{ type }
			, max_query_{ max_query } {
		}

		QueryPool(QueryPool&& other)
			: DeviceHandle(std::move(other))
			, type_{ std::exchange(other.type_, {}) }
			, max_query_{ std::exchange(other.max_query_, {}) } {
		}

		auto operator=(QueryPool&& other) -> QueryPool& {
			DeviceHandle::operator=(std::move(other));
			type_ = std::exchange(other.type_, {});
			max_query_ = std::exchange(other.max_query_, {});
			return *this;
		}

		auto get_data(uint32_t query_index, void* data) const -> vk::Result;
		auto get_data(uint32_t first_query, uint32_t query_count, void* data) const -> vk::Result;

		auto reset(uint32_t start_query, uint32_t query_count = 0u) const -> void;
	private:
		vk::QueryType type_;
		uint32_t max_query_;
	};

	class PipelineCache : public DeviceHandle<vk::PipelineCache> {
	public:
		PipelineCache(Device const* device = nullptr, vk::PipelineCache handle = VK_NULL_HANDLE) 
			: DeviceHandle{ device, handle } {
		}

		auto get_data(mi::Vector<uint8_t>& data) const -> vk::Result;
		auto get_data(void*& data, size_t& size) const -> vk::Result;
	};

	class Pipeline : public DeviceHandle<vk::Pipeline> {
	public:
		Pipeline(Device const* device = nullptr, vk::Pipeline handle = VK_NULL_HANDLE)
			: DeviceHandle{ device, handle } {
		}
	};

	class ShaderModule : public DeviceHandle<vk::ShaderModule> {
	public:
		ShaderModule(Device const* device = nullptr, vk::ShaderModule handle = VK_NULL_HANDLE)
			: DeviceHandle{ device, handle } {
		}
	private:
		vk::PipelineShaderStageCreateInfo shader_stage_create_info_;
	};

	class PipelineLayout : public DeviceHandle<vk::PipelineLayout> {
	public:
		PipelineLayout(Device const* device = nullptr, vk::PipelineLayout handle = VK_NULL_HANDLE)
			: DeviceHandle{ device, handle } {
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
		DescriptorSetLayoutBuilder(Context const& ctx);

		auto add_binding(uint32_t binding, vk::DescriptorType descriptor_type, uint32_t descriptor_count, vk::ShaderStageFlags stage_flags, vk::DescriptorBindingFlagsEXT binding_flags = {}) -> DescriptorSetLayoutBuilder& {
			pool_sizes_[static_cast<uint32_t>(descriptor_type)] += descriptor_count;
			layout_bindings_.emplace_back(binding, descriptor_type, descriptor_count, stage_flags, nullptr);
			binding_flags_.emplace_back(binding_flags);
			return *this;
		}

		auto build(vk::DescriptorSetLayoutCreateFlags flags = {}) -> Result<DescriptorSetLayout>;
	private:
		Context const* ctx_{ nullptr };

		mi::Vector<vk::DescriptorSetLayoutBinding> layout_bindings_;
		mi::Vector<vk::DescriptorBindingFlagsEXT> binding_flags_;
		PoolSizes pool_sizes_;
	};

	class PipelineLayoutBuilder : public NonCopyable {
	public:
		PipelineLayoutBuilder(Context const& ctx);

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
		Context const* ctx_{ nullptr };

		mi::Vector<vk::DescriptorSetLayout> descriptor_set_layouts_;
		mi::Vector<vk::PushConstantRange> push_constant_ranges_;
	};

	class DescriptorSetLayout : public DeviceHandle<vk::DescriptorSetLayout> {
	public:
		DescriptorSetLayout(Device const* device = nullptr, vk::DescriptorSetLayout handle = VK_NULL_HANDLE, PoolSizes pool_sizes = {})
			: DeviceHandle{ device, handle }
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
		DescriptorPool(Device const* device = nullptr, vk::DescriptorPool handle = VK_NULL_HANDLE)
			: DeviceHandle{ device, handle } {
		}

		auto allocate_descriptor_set(DescriptorSetLayout const& layout) const -> Result<DescriptorSet>;
		auto free_descriptor_set(DescriptorSet const& set) const -> void;
	};

	class DescriptorSet : public Handle<vk::DescriptorSet> {
	public:
		DescriptorSet(Device const* device = nullptr, vk::DescriptorSet handle = VK_NULL_HANDLE, PoolSizes const& pool_sizes = {})
			: Handle{ handle, device ? device->get_allocator() : nullptr }
			, pool_sizes_{ pool_sizes } {
		}

		auto get_pool_sizes() const -> PoolSizes const& {
			return pool_sizes_;
		}

	private:
		PoolSizes pool_sizes_{};
	};

	class Context : public NonCopyable {
	public:
		Context();
		Context(std::nullptr_t) noexcept
			: instance_{ nullptr }
			, surface_{ nullptr }
			, adapter_{ nullptr }
			, device_{ nullptr }
			, memory_allocator_{ nullptr } {
		}
		~Context();

		Context(Context&& other)
			: allocator_{ std::exchange(other.allocator_, VK_NULL_HANDLE) }
			, instance_{ std::exchange(other.instance_, VK_NULL_HANDLE) }
			, surface_{ std::exchange(other.surface_, VK_NULL_HANDLE) }
			, adapter_{ std::exchange(other.adapter_, VK_NULL_HANDLE) }
			, device_{ std::exchange(other.device_, VK_NULL_HANDLE) }
			, memory_allocator_{ std::exchange(other.memory_allocator_, VK_NULL_HANDLE) } {
		}

		auto operator=(Context&& other) -> Context& {
			allocator_ = std::exchange(other.allocator_, VK_NULL_HANDLE);
			instance_ = std::exchange(other.instance_, VK_NULL_HANDLE);
			surface_ = std::exchange(other.surface_, VK_NULL_HANDLE);
			adapter_ = std::exchange(other.adapter_, VK_NULL_HANDLE);
			device_ = std::exchange(other.device_, VK_NULL_HANDLE);
			memory_allocator_ = std::exchange(other.memory_allocator_, VK_NULL_HANDLE);
			return *this;
		}

		static auto construct(const ContextInfo& info) -> Result<Context>;

		auto create_fence(vk::FenceCreateFlags flags = {}) const -> Result<Fence>;
		auto create_semaphore(vk::SemaphoreType type = vk::SemaphoreType::eBinary, uint64_t initial_value = 0ull) const -> Result<Semaphore>;
		// TODO: Queue selection not finished
		auto get_queue(QueueType type) const -> Result<Queue>;

		auto create_image(const ImageCreateInfo& create_info) const -> Result<Image>;
		auto create_image_view(const Image& image, const vk::ImageSubresourceRange& range, vk::ImageViewType type) const -> Result<ImageView>;
		auto create_buffer(const BufferCreateInfo& create_info) const -> Result<Buffer>;
		auto create_buffer_view(const Buffer& buffer, vk::DeviceSize size, vk::DeviceSize offset = 0ull, vk::Format format = vk::Format::eUndefined) const -> Result<BufferView>;

		// TODO: Shader
		auto create_sampler(const vk::SamplerCreateInfo& create_info) const -> Result<Sampler>;
		// TODO: RootSignature
		// TODO: Pipeline

		auto create_shader_module(Span<const uint8_t> code) const -> Result<ShaderModule>;

		auto create_pipeline_cache(Span<const uint8_t> data) const -> Result<PipelineCache>;
		auto create_query_pool(vk::QueryType type, uint32_t query_count) const -> Result<QueryPool>;
		auto create_descriptor_pool(PoolSizes const& pool_sizes, uint32_t max_descriptor_sets, vk::DescriptorPoolCreateFlags flags = {}) const -> Result<DescriptorPool>;

		auto get_allocator() const -> vk::AllocationCallbacks const* { return allocator_; }
		auto get_instance() const -> Instance const& { return instance_; }
		auto get_surface() const -> Surface const& { return surface_; }
		auto get_adapter() const -> Adapter const& { return adapter_; }
		auto get_device() const -> Device const& { return device_; }
		auto get_memory_allocator() const -> MemoryAllocator const& { return memory_allocator_; }
	private:
		auto _construct(const ContextInfo& info) -> vk::Result;

		vk::AllocationCallbacks const* allocator_{ nullptr };
		vk::detail::DynamicLoader loader_;
		Instance instance_;
		Surface surface_;
		Adapter adapter_;
		Device device_;
		MemoryAllocator memory_allocator_;
	};
}