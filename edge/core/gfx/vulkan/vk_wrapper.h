#pragma once

#include "../../foundation/foundation.h"

#include <array>
#include <expected>
#include <string_view>
#include <vector>
#include <tuple>
#include <cstdlib>
#include <span>
#include <functional>

#include <vulkan/vulkan.hpp>
#include <volk.h>
#include <vk_mem_alloc.h>

#if defined(VKW_DEBUG) || defined(VKW_VALIDATION_LAYERS)
#ifndef USE_VALIDATION_LAYERS
#define USE_VALIDATION_LAYERS
#endif
#endif

#if defined(USE_VALIDATION_LAYERS) && (defined(VKW_VALIDATION_LAYERS_GPU_ASSISTED) || defined(VKW_VALIDATION_LAYERS_BEST_PRACTICES) || defined(VKW_VALIDATION_LAYERS_SYNCHRONIZATION))
#ifndef USE_VALIDATION_LAYER_FEATURES
#define USE_VALIDATION_LAYER_FEATURES
#endif
#endif

namespace edge::vkw {
	class Device;
	class Swapchain;
	class MemoryAllocator;

	template<typename T>
	using Result = std::expected<T, vk::Result>;

	template<typename T>
	struct FeatureTraits {
		static_assert(sizeof(T) == 0, "Unsupported feature.");
	};

	template<>
	struct FeatureTraits<vk::PhysicalDeviceBufferDeviceAddressFeatures> {
		static constexpr const char* extension_name{ VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME };
	};

	template<>
	struct FeatureTraits<vk::PhysicalDevicePerformanceQueryFeaturesKHR> {
		static constexpr const char* extension_name{ VK_KHR_PERFORMANCE_QUERY_EXTENSION_NAME };
	};

	template<>
	struct FeatureTraits<vk::PhysicalDeviceHostQueryResetFeatures> {
		static constexpr const char* extension_name{ VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME };
	};

	template<>
	struct FeatureTraits<vk::PhysicalDeviceSynchronization2Features> {
		static constexpr const char* extension_name{ VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME };
	};

	template<>
	struct FeatureTraits<vk::PhysicalDeviceDynamicRenderingFeatures> {
		static constexpr const char* extension_name{ VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME };
	};

	template<>
	struct FeatureTraits<vk::PhysicalDeviceShaderDemoteToHelperInvocationFeaturesEXT> {
		static constexpr const char* extension_name{ VK_EXT_SHADER_DEMOTE_TO_HELPER_INVOCATION_EXTENSION_NAME };
	};

	template<>
	struct FeatureTraits<vk::PhysicalDevice16BitStorageFeaturesKHR> {
		static constexpr const char* extension_name{ VK_KHR_16BIT_STORAGE_EXTENSION_NAME };
	};

	template<>
	struct FeatureTraits<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT> {
		static constexpr const char* extension_name{ VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME };
	};

	template<>
	struct FeatureTraits<vk::PhysicalDeviceRayQueryFeaturesKHR> {
		static constexpr const char* extension_name{ VK_KHR_RAY_QUERY_EXTENSION_NAME };
	};

	template<>
	struct FeatureTraits<vk::PhysicalDeviceAccelerationStructureFeaturesKHR> {
		static constexpr const char* extension_name{ VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME };
	};

	template<>
	struct FeatureTraits<vk::PhysicalDeviceRayTracingPipelineFeaturesKHR> {
		static constexpr const char* extension_name{ VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME };
	};

	template<>
	struct FeatureTraits<vk::PhysicalDeviceDiagnosticsConfigFeaturesNV> {
		static constexpr const char* extension_name{ VK_NV_DEVICE_DIAGNOSTICS_CONFIG_EXTENSION_NAME };
	};

	template<typename T>
	class Allocator {
	public:
		using value_type = T;
		using size_type = std::size_t;
		using difference_type = std::ptrdiff_t;
		using pointer = T*;
		using const_pointer = const T*;
		using reference = T&;
		using const_reference = const T&;

		Allocator(vk::AllocationCallbacks const* callbacks = nullptr) noexcept
			: callbacks_(callbacks) {
		}

		template<typename U>
		Allocator(const Allocator<U>& other) noexcept
			: callbacks_(other.get_callbacks()) {
		}

		auto allocate(size_type n) -> pointer {
			if (callbacks_ && callbacks_->pfnAllocation) {
				void* ptr = callbacks_->pfnAllocation(
					callbacks_->pUserData,
					n * sizeof(T),
					alignof(T), 
					vk::SystemAllocationScope::eObject
				);
				if (!ptr) {
					throw std::bad_alloc();
				}
				return static_cast<pointer>(ptr);
			}
			else {
#if defined(VK_USE_PLATFORM_WIN32_KHR)
				return static_cast<pointer>(_aligned_malloc(n * sizeof(T), alignof(T)));
#else
				return static_cast<pointer>(aligned_alloc(alignof(T), n * sizeof(T)));
#endif
			}
		}

		void deallocate(pointer p, size_type) noexcept {
			if (callbacks_ && callbacks_->pfnFree) {
				callbacks_->pfnFree(callbacks_->pUserData, p);
			}
			else {
#if defined(VK_USE_PLATFORM_WIN32_KHR)
				_aligned_free(p);
#else
				std::free(p);
#endif
			}
		}

		template<typename U>
		auto operator==(const Allocator<U>& other) const noexcept -> bool {
			return callbacks_ == other.get_callbacks();
		}

		template<typename U>
		auto operator!=(const Allocator<U>& other) const noexcept -> bool {
			return !(*this == other);
		}

		auto get_callbacks() const noexcept -> vk::AllocationCallbacks const* {
			return callbacks_;
		}
	private:
		vk::AllocationCallbacks const* callbacks_;
	};

	template<typename T>
	using Vector = std::vector<T, Allocator<T>>;

	template<typename K, typename T, typename Hasher = std::hash<K>, typename KeyEq = std::equal_to<K>, typename Alloc = Allocator<std::pair<K, T>>>
	using HashMap = std::unordered_map<K, T, Hasher, KeyEq, Alloc>;

	inline auto make_color_array(uint32_t in_color, Span<float> out_color) -> void {
		out_color[3] = static_cast<float>((in_color >> 24) & 0xFF) / 255.0f;
		out_color[0] = static_cast<float>((in_color >> 16) & 0xFF) / 255.0f;
		out_color[1] = static_cast<float>((in_color >> 8) & 0xFF) / 255.0f;
		out_color[2] = static_cast<float>(in_color & 0xFF) / 255.0f;
	}

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
			enabled_extensions_.emplace_back(extension_name);
			return *this;
		}

		auto add_extensions(std::span<const char* const> extensions) -> InstanceBuilder& {
			enabled_extensions_.insert(enabled_extensions_.end(), extensions.begin(), extensions.end());
			return *this;
		}

		auto remove_extension(const char* extension_name) -> InstanceBuilder& {
			enabled_extensions_.erase(
				std::remove_if(enabled_extensions_.begin(), enabled_extensions_.end(),
					[extension_name](const char* ext) { return std::strcmp(ext, extension_name) == 0; }),
				enabled_extensions_.end()
			);
			return *this;
		}

		auto clear_extensions() -> InstanceBuilder& {
			enabled_extensions_.clear();
			return *this;
		}

		// Layer management
		auto add_layer(const char* layer_name) -> InstanceBuilder& {
			enabled_layers_.emplace_back(layer_name);
			return *this;
		}

		auto add_layers(std::span<const char* const> layers) -> InstanceBuilder& {
			enabled_layers_.insert(enabled_layers_.end(), layers.begin(), layers.end());
			return *this;
		}

		auto remove_layer(const char* layer_name) -> InstanceBuilder& {
			enabled_layers_.erase(
				std::remove_if(enabled_layers_.begin(), enabled_layers_.end(),
					[layer_name](const char* layer) { return std::strcmp(layer, layer_name) == 0; }),
				enabled_layers_.end()
			);
			return *this;
		}

		auto clear_layers() -> InstanceBuilder& {
			enabled_layers_.clear();
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
		auto build() -> Result<vk::Instance>;

		// Get current configuration for inspection
		auto get_enabled_extensions() const -> const Vector<const char*>& { return enabled_extensions_; }
		auto get_enabled_layers() const -> const Vector<const char*>& { return enabled_layers_; }
		auto get_app_info() const -> const vk::ApplicationInfo& { return app_info_; }

	private:
		vk::ApplicationInfo app_info_{};
		vk::InstanceCreateInfo create_info_{};
		Vector<const char*> enabled_extensions_;
		Vector<const char*> enabled_layers_;
		Vector<vk::ValidationFeatureEnableEXT> validation_feature_enables_;
		Vector<vk::ValidationFeatureDisableEXT> validation_feature_disables_;
		const vk::AllocationCallbacks* allocator_{ nullptr };

		bool enable_debug_utils_{ false };
		bool enable_surface_{ false };
		bool enable_portability_{ false };
	};

	class DeviceSelector {
	public:
		DeviceSelector(vk::Instance instance, vk::AllocationCallbacks const* allocator);

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

		auto select() -> Result<Device>;

	private:
		vk::Instance instance_{ VK_NULL_HANDLE };

		vk::AllocationCallbacks const* allocator_{ nullptr };
		vk::SurfaceKHR surface_{ VK_NULL_HANDLE };
		uint32_t minimal_api_ver{ VK_VERSION_1_0 };
		vk::PhysicalDeviceType preferred_type_{ vk::PhysicalDeviceType::eDiscreteGpu };

		Vector<std::pair<const char*, bool>> requested_extensions_;

		Vector<Shared<void>> requested_features_;
		void* last_feature_ptr_{ nullptr };
	};

	class Queue {
	public:
		Queue(Device& device, uint32_t queue_family_index, uint32_t queue_index);
		Queue(std::nullptr_t) noexcept {}
		~Queue();

		Queue(const Queue&) = delete;
		auto operator=(const Queue&) -> Queue & = delete;

		Queue(Queue&& other) 
			: handle_{ std::exchange(other.handle_, VK_NULL_HANDLE) }
			, queue_family_index_{ std::exchange(other.queue_family_index_, ~0u) }
			, queue_index_{ std::exchange(other.queue_index_, ~0u) }
			, device_{ std::exchange(other.device_, nullptr) } {
		}

		auto operator=(Queue&& other) -> Queue& {
			handle_ = std::exchange(other.handle_, VK_NULL_HANDLE);
			queue_family_index_ = std::exchange(other.queue_family_index_, ~0u);
			queue_index_ = std::exchange(other.queue_index_, ~0u);
			device_ = std::exchange(other.device_, nullptr);
			return *this;
		}

		auto submit(const vk::SubmitInfo2& submit_info, vk::Fence fence = VK_NULL_HANDLE) const -> vk::Result;
		auto wait_idle() const -> void;

		auto get_family_index() const noexcept -> uint32_t { return queue_family_index_; }
		auto get_queue_index() const noexcept -> uint32_t { return queue_index_; }

		operator vk::Queue() const noexcept { return handle_; }
		operator VkQueue() const noexcept { return handle_; }
		auto get_handle() const noexcept -> vk::Queue { return handle_; }
	private:
		vk::Queue handle_{ VK_NULL_HANDLE };
		uint32_t queue_family_index_{ ~0u };
		uint32_t queue_index_{ ~0u };
		Device* device_{ nullptr };
	};

	class Device {
	public:
		struct QueueFamilyInfo {
			uint32_t index;
			Vector<uint32_t> queue_indices;
		};

		Device(vk::PhysicalDevice physical = VK_NULL_HANDLE, vk::Device logical = VK_NULL_HANDLE, vk::AllocationCallbacks const* allocator = nullptr, Vector<const char*>&& enabled_extensions = {});
		~Device();

		Device(const Device&) = delete;
		auto operator=(const Device&) -> Device& = delete;

		Device(Device&& other) 
			: logical_{ std::exchange(other.logical_, VK_NULL_HANDLE) }
			, physical_{ std::exchange(other.physical_, VK_NULL_HANDLE) }
			, allocator_{ std::exchange(other.allocator_, nullptr) }
			, enabled_extensions_{ std::exchange(other.enabled_extensions_, {}) }
			, supported_extensions_{ std::exchange(other.supported_extensions_, {}) } {

		}

		auto operator=(Device&& other) -> Device& {
			logical_ = std::exchange(other.logical_, VK_NULL_HANDLE);
			physical_ = std::exchange(other.physical_, VK_NULL_HANDLE);
			allocator_ = std::exchange(other.allocator_, nullptr);
			enabled_extensions_ = std::exchange(other.enabled_extensions_, {});
			supported_extensions_ = std::exchange(other.supported_extensions_, {});
			return *this;
		}

		auto create_allocator(vk::Instance instance) const -> Result<MemoryAllocator>;

		auto get_queue_family_properties() const -> Vector<vk::QueueFamilyProperties>;

		auto get_queue(vk::QueueFlagBits queue_type) -> Result<Queue>;
		auto release_queue(Queue& queue) -> void;

		auto create_handle(const vk::CommandPoolCreateInfo& create_info, vk::CommandPool& handle) const -> vk::Result;
		auto destroy_handle(vk::CommandPool handle) const -> void;
		auto allocate_command_buffer(const vk::CommandBufferAllocateInfo& allocate_info, vk::CommandBuffer& handle) const -> vk::Result;
		auto free_command_buffer(vk::CommandPool command_pool, vk::CommandBuffer handle) const -> void;

		auto create_handle(const vk::SemaphoreCreateInfo& create_info, vk::Semaphore& handle) const -> vk::Result;
		auto destroy_handle(vk::Semaphore handle) const -> void;
		auto signal_semaphore(const vk::SemaphoreSignalInfo& signal_info) const -> vk::Result;
		auto wait_semaphore(const vk::SemaphoreWaitInfo& wait_info, uint64_t timeout = std::numeric_limits<uint64_t>::max()) const -> vk::Result;
		auto get_semaphore_counter_value(vk::Semaphore handle) const -> Result<uint64_t>;

		auto create_handle(const vk::FenceCreateInfo& create_info, vk::Fence& handle) const -> vk::Result;
		auto destroy_handle(vk::Fence handle) const -> void;

		auto create_handle(const vk::SwapchainCreateInfoKHR& create_info, vk::SwapchainKHR& handle) const -> vk::Result;
		auto destroy_handle(vk::SwapchainKHR handle) const -> void;

		auto create_handle(const vk::BufferViewCreateInfo& create_info, vk::BufferView& handle) const -> vk::Result;
		auto destroy_handle(vk::BufferView handle) const -> void;

		auto create_handle(const vk::ImageViewCreateInfo& create_info, vk::ImageView& handle) const -> vk::Result;
		auto destroy_handle(vk::ImageView handle) const -> void;

		auto get_buffer_device_address(const vk::BufferDeviceAddressInfo& address_info) const -> uint64_t;

		template<typename T>
		auto set_object_name(T object, std::string_view name) const -> void {
			set_object_name(T::objectType, reinterpret_cast<uint64_t>(static_cast<T::CType>(object)), name);
		}

		auto set_object_name(vk::ObjectType object_type, uint64_t object_handle, std::string_view name) const -> void;

		auto is_enabled(const char* extension_name) const -> bool;
		auto is_supported(const char* extension_name) const -> bool;

		operator vk::Device() const { return logical_; }
		operator VkDevice() const { return logical_; }
		auto get_logical() const -> vk::Device { return logical_; }

		operator vk::PhysicalDevice() const { return physical_; }
		operator VkPhysicalDevice() const { return physical_; }
		auto get_physical() const -> vk::PhysicalDevice { return physical_; }

		auto get_allocator() const -> vk::AllocationCallbacks const* { return allocator_; }
	private:
		vk::Device logical_{ VK_NULL_HANDLE };
		vk::PhysicalDevice physical_{ VK_NULL_HANDLE };
		vk::AllocationCallbacks const* allocator_{ nullptr };

		Vector<const char*> enabled_extensions_;
		Vector<vk::ExtensionProperties> supported_extensions_;

		Array<Vector<QueueFamilyInfo>, 3ull> queue_family_map_;
	};

	class Swapchain {
	public:
		struct State {
			uint32_t image_count;
			vk::SurfaceFormatKHR format;
			vk::Extent2D extent;
			vk::SurfaceTransformFlagBitsKHR transform;
			bool vsync;
			bool hdr;
		};

		Swapchain() = default;
		Swapchain(Device const& device, vk::SwapchainKHR swapchain, const State& new_state);
		Swapchain(std::nullptr_t) noexcept {}
		~Swapchain();

		Swapchain(const Swapchain&) = delete;
		auto operator=(const Swapchain&) -> Swapchain & = delete;

		Swapchain(Swapchain&& other) 
			: handle_{ std::exchange(other.handle_, VK_NULL_HANDLE) }
			, device_{ std::exchange(other.device_, nullptr) } {
		}

		auto operator=(Swapchain&& other) -> Swapchain& {
			handle_ = std::exchange(other.handle_, VK_NULL_HANDLE);
			device_ = std::exchange(other.device_, nullptr);
			return *this;
		}

		auto reset() -> void;

		auto get_image_count() const noexcept -> uint32_t { return state_.image_count; }
		auto get_format() const noexcept -> vk::Format { return state_.format.format; }
		auto get_color_space() const noexcept -> vk::ColorSpaceKHR { return state_.format.colorSpace; }
		auto get_extent() const noexcept -> vk::Extent2D { return state_.extent; }
		auto get_width() const noexcept -> uint32_t { return state_.extent.width; }
		auto get_height() const noexcept -> uint32_t { return state_.extent.height; }
		auto get_transform() const noexcept -> vk::SurfaceTransformFlagBitsKHR { return state_.transform; }
		auto is_vsync_enabled() const noexcept -> bool { return state_.vsync; }
		auto is_hdr_enabled() const noexcept -> bool { return state_.hdr; }

		operator vk::SwapchainKHR() const noexcept { return handle_; }
		operator VkSwapchainKHR() const noexcept { return handle_; }
		auto get_handle() const noexcept -> vk::SwapchainKHR { return handle_; }
	private:
		vk::SwapchainKHR handle_{ VK_NULL_HANDLE };
		Device const* device_{ nullptr };

		State state_;
	};

	class SwapchainBuilder {
	public:
		SwapchainBuilder(Device const& device, vk::SurfaceKHR surface);

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
		static auto choose_surface_format(const vk::SurfaceFormatKHR requested_surface_format, const Vector<vk::SurfaceFormatKHR>& available_surface_formats, bool prefer_hdr = false) -> vk::SurfaceFormatKHR;
		static auto choose_suitable_composite_alpha(vk::CompositeAlphaFlagBitsKHR request_composite_alpha, vk::CompositeAlphaFlagsKHR supported_composite_alpha) -> vk::CompositeAlphaFlagBitsKHR;
		static auto choose_suitable_present_mode(vk::PresentModeKHR request_present_mode, std::span<const vk::PresentModeKHR> available_present_modes, std::span<const vk::PresentModeKHR> present_mode_priority_list) -> vk::PresentModeKHR;

		Swapchain::State requested_state_;
		vk::SwapchainKHR old_swapchain_{ VK_NULL_HANDLE };

		Device const* device_{ nullptr };
		vk::SurfaceKHR surface_{ VK_NULL_HANDLE };
	};
	
	// TODO: Add aliasing support
	template<typename T>
	class MemoryAllocation {
	public:
		MemoryAllocation() = default;
		MemoryAllocation(std::nullptr_t) noexcept {}

		MemoryAllocation(MemoryAllocator const& allocator, T handle, VmaAllocation allocation, VmaAllocationInfo allocation_info)
			: allocator_{ &allocator }
			, handle_{ handle }
			, allocation_{ allocation }
			, allocation_info_{ allocation_info } {

			auto memory_properties = allocator_->get_memory_propersies(allocation_);
			coherent_ = (memory_properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
			persistent_ = allocation_info_.pMappedData != nullptr;
			mapped_memory_ = static_cast<uint8_t*>(allocation_info.pMappedData);
		}

		~MemoryAllocation() {
			if (handle_ && allocation_) {
				unmap();
				allocator_->deallocate(handle_, allocation_);
				handle_ = VK_NULL_HANDLE;
				allocation_ = VK_NULL_HANDLE;
			}
		}

		MemoryAllocation(const MemoryAllocation&) = delete;
		auto operator=(const MemoryAllocation&) -> MemoryAllocation & = delete;

		MemoryAllocation(MemoryAllocation&& other)
			: allocator_{ std::exchange(other.allocator_, nullptr) }
			, handle_{ std::exchange(other.handle_, VK_NULL_HANDLE) }
			, allocation_{ std::exchange(other.allocation_, VK_NULL_HANDLE) }
			, allocation_info_{ std::exchange(other.allocation_info_, {}) }
			, mapped_memory_{ std::exchange(other.mapped_memory_, {}) } {}

		auto operator=(MemoryAllocation&& other) -> MemoryAllocation& {
			allocator_ = std::exchange(other.allocator_, nullptr);
			handle_ = std::exchange(other.handle_, VK_NULL_HANDLE);
			allocation_ = std::exchange(other.allocation_, VK_NULL_HANDLE);
			allocation_info_ = std::exchange(other.allocation_info_, {});
			mapped_memory_ = std::exchange(other.mapped_memory_, nullptr);
			return *this;
		}

		auto map() const -> Result<Span<uint8_t>> {
			if (!persistent_ && !is_mapped()) {
				if (auto result = allocator_->map_memory(allocation_, reinterpret_cast<void**>(&mapped_memory_)); result != vk::Result::eSuccess) {
					return std::unexpected(result);
				}
			}
			return Span<uint8_t>(mapped_memory_, allocation_info_.size);
		}

		auto unmap() const -> void {
			if (!persistent_ && is_mapped()) {
				allocator_->unmap_memory(allocation_);
				mapped_memory_ = nullptr;
			}
		}

		auto flush(vk::DeviceSize offset, vk::DeviceSize size) const -> vk::Result {
			if (!coherent_) {
				return allocator_->flush_memory(allocation_, offset, size);
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

				if (auto result = flush(offset, size); result != vk::Result::eSuccess) {
					return result;
				}
			}

			return vk::Result::eSuccess;
		}

		auto get_size() const -> uint64_t { return allocation_info_.size; }

		auto is_coherent() const -> bool { return coherent_; }
		auto is_persistent() const -> bool { return persistent_; }
		auto is_mapped() const -> bool { return mapped_memory_ != nullptr; }

		auto get_memory() const -> vk::DeviceMemory {
			return (vk::DeviceMemory)allocation_info_.deviceMemory;
		}

		operator T() const noexcept { return handle_; }
		operator T::CType() const noexcept { return handle_; }
		auto get_handle() const noexcept -> T { return handle_; }
		auto get_allocation() const -> VmaAllocation { return allocation_; }
		auto get_allocator() const -> MemoryAllocator const& { return *allocator_; }
	protected:
		MemoryAllocator const* allocator_{ nullptr };
		T handle_{ VK_NULL_HANDLE };
		VmaAllocation allocation_{ VK_NULL_HANDLE };
		VmaAllocationInfo allocation_info_{};

		bool persistent_{ false };
		bool coherent_{ false };

		mutable uint8_t* mapped_memory_{ nullptr };
	};

	class Image : public MemoryAllocation<vk::Image> {
	public:
		Image() = default;
		Image(std::nullptr_t) noexcept {}

		Image(MemoryAllocator const& allocator, vk::Image handle, VmaAllocation allocation, VmaAllocationInfo allocation_info)
			: MemoryAllocation(allocator, handle, allocation, allocation_info) {
		}

		auto get_extent() const -> vk::Extent3D;
		auto get_layer_count() const -> uint32_t;
		auto gat_level_count() const -> uint32_t;
	private:

	};

	class ImageView {
	public:
	private:
		Device const* device_{ nullptr };
		vk::ImageView handle_{ VK_NULL_HANDLE };
	};

	class Buffer : public MemoryAllocation<vk::Buffer> {
	public:
		Buffer() = default;
		Buffer(std::nullptr_t) noexcept {}

		Buffer(MemoryAllocator const& allocator, vk::Buffer handle, VmaAllocation allocation, VmaAllocationInfo allocation_info)
			: MemoryAllocation(allocator, handle, allocation, allocation_info) {
		}

		auto create_view(uint64_t offset, uint64_t size, vk::Format format) const -> Result<BufferView>;

		auto get_gpu_virtual_address() const -> uint64_t;
	};

	class BufferView {
	public:
		BufferView() = default;
		BufferView(std::nullptr_t) noexcept {}
		BufferView(Device const& device, vk::BufferView handle, uint64_t offset, uint64_t size, vk::Format format);
		~BufferView();

		BufferView(const BufferView&) = delete;
		auto operator=(const BufferView&) -> BufferView & = delete;

		BufferView(BufferView&& other)
			: device_{ std::exchange(other.device_, nullptr) }
			, handle_{ std::exchange(other.handle_, VK_NULL_HANDLE) }
			, offset_{ std::exchange(other.offset_, ~0ull) }
			, size_{ std::exchange(other.size_, ~0ull) }
			, format_{ std::exchange(other.format_, vk::Format::eUndefined) } {
		}

		auto operator=(BufferView&& other) -> BufferView& {
			device_ = std::exchange(other.device_, nullptr);
			handle_ = std::exchange(other.handle_, VK_NULL_HANDLE);
			offset_ = std::exchange(other.offset_, ~0ull);
			size_ = std::exchange(other.size_, ~0ull);
			format_ = std::exchange(other.format_, vk::Format::eUndefined);
			return *this;
		}

		auto get_offset() const -> uint64_t { return offset_; }
		auto get_size() const -> uint64_t { return size_; }
		auto get_format() const -> vk::Format { return format_; }

		operator vk::BufferView() const noexcept { return handle_; }
		operator VkBufferView() const noexcept { return handle_; }
		auto get_handle() const noexcept -> vk::BufferView { return handle_; }
		auto get_device() const -> Device const& { return *device_; }
	private:
		Device const* device_{ nullptr };
		vk::BufferView handle_{ VK_NULL_HANDLE };
		uint64_t offset_, size_;
		vk::Format format_;
	};

	class MemoryAllocator {
	public:
		MemoryAllocator() = default;
		MemoryAllocator(Device const& device, VmaAllocator handle);
		MemoryAllocator(std::nullptr_t) noexcept {}
		~MemoryAllocator();

		MemoryAllocator(const MemoryAllocator&) = delete;
		auto operator=(const MemoryAllocator&) -> MemoryAllocator& = delete;

		MemoryAllocator(MemoryAllocator&& other) :
			handle_{ std::exchange(other.handle_, VK_NULL_HANDLE) },
			device_{ std::exchange(other.device_, nullptr) } {
		}

		auto operator=(MemoryAllocator&& other) -> MemoryAllocator& {
			handle_ = std::exchange(other.handle_, VK_NULL_HANDLE);
			device_ = std::exchange(other.device_, nullptr);
			return *this;
		}

		auto get_memory_propersies(VmaAllocation allocation) const -> VkMemoryPropertyFlags;

		auto map_memory(VmaAllocation allocation, void** mapped_memory) const -> vk::Result;
		auto unmap_memory(VmaAllocation allocation) const -> void;
		auto flush_memory(VmaAllocation allocation, vk::DeviceSize offset, vk::DeviceSize size) const -> vk::Result;

		auto allocate_image(const vk::ImageCreateInfo& create_info, const VmaAllocationCreateInfo& allocation_create_info) const -> Result<Image>;
		auto allocate_buffer(const vk::BufferCreateInfo& create_info, const VmaAllocationCreateInfo& allocation_create_info) const -> Result<Buffer>;
		auto deallocate(vk::Image handle, VmaAllocation allocation) const -> void;
		auto deallocate(vk::Buffer handle, VmaAllocation allocation) const -> void;

		auto get_device() const -> Device const* { return device_; }
		operator VmaAllocator() const noexcept { return handle_; }
		auto get_handle() const noexcept -> VmaAllocator { return handle_; }
	private:
		Device const* device_{ nullptr };
		VmaAllocator handle_{ VK_NULL_HANDLE };
	};
}