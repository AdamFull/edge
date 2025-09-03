#pragma once

#include <array>
#include <expected>
#include <string_view>
#include <vector>
#include <memory>
#include <cstdlib>
#include <span>
#include <functional>

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

namespace vkw {
	class Instance;
	class PhysicalDevice;
	class Device;

	template<typename T>
	using Result = std::expected<T, VkResult>;

	template<typename T>
	struct FeatureTraits {
		static_assert(sizeof(T) == 0, "Unsupported feature.");
	};

	template<>
	struct FeatureTraits<VkPhysicalDeviceBufferDeviceAddressFeatures> {
		static constexpr VkStructureType structure_type{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES };
		static constexpr const char* extension_name{ VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME };
	};

	template<>
	struct FeatureTraits<VkPhysicalDevicePerformanceQueryFeaturesKHR> {
		static constexpr VkStructureType structure_type{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PERFORMANCE_QUERY_FEATURES_KHR };
		static constexpr const char* extension_name{ VK_KHR_PERFORMANCE_QUERY_EXTENSION_NAME };
	};

	template<>
	struct FeatureTraits<VkPhysicalDeviceHostQueryResetFeatures> {
		static constexpr VkStructureType structure_type{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES };
		static constexpr const char* extension_name{ VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME };
	};

	template<>
	struct FeatureTraits<VkPhysicalDeviceSynchronization2Features> {
		static constexpr VkStructureType structure_type{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES };
		static constexpr const char* extension_name{ VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME };
	};

	template<>
	struct FeatureTraits<VkPhysicalDeviceDynamicRenderingFeatures> {
		static constexpr VkStructureType structure_type{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES };
		static constexpr const char* extension_name{ VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME };
	};

	template<>
	struct FeatureTraits<VkPhysicalDeviceShaderDemoteToHelperInvocationFeaturesEXT> {
		static constexpr VkStructureType structure_type{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DEMOTE_TO_HELPER_INVOCATION_FEATURES_EXT };
		static constexpr const char* extension_name{ VK_EXT_SHADER_DEMOTE_TO_HELPER_INVOCATION_EXTENSION_NAME };
	};

	template<>
	struct FeatureTraits<VkPhysicalDevice16BitStorageFeaturesKHR> {
		static constexpr VkStructureType structure_type{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES };
		static constexpr const char* extension_name{ VK_KHR_16BIT_STORAGE_EXTENSION_NAME };
	};

	template<>
	struct FeatureTraits<VkPhysicalDeviceExtendedDynamicStateFeaturesEXT> {
		static constexpr VkStructureType structure_type{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT };
		static constexpr const char* extension_name{ VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME };
	};

	template<>
	struct FeatureTraits<VkPhysicalDeviceRayQueryFeaturesKHR> {
		static constexpr VkStructureType structure_type{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR };
		static constexpr const char* extension_name{ VK_KHR_RAY_QUERY_EXTENSION_NAME };
	};

	template<>
	struct FeatureTraits<VkPhysicalDeviceAccelerationStructureFeaturesKHR> {
		static constexpr VkStructureType structure_type{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
		static constexpr const char* extension_name{ VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME };
	};

	template<>
	struct FeatureTraits<VkPhysicalDeviceRayTracingPipelineFeaturesKHR> {
		static constexpr VkStructureType structure_type{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR };
		static constexpr const char* extension_name{ VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME };
	};

	template<>
	struct FeatureTraits<VkPhysicalDeviceDiagnosticsConfigFeaturesNV> {
		static constexpr VkStructureType structure_type{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DIAGNOSTICS_CONFIG_FEATURES_NV };
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

		Allocator(VkAllocationCallbacks const* callbacks = nullptr) noexcept
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
					VK_SYSTEM_ALLOCATION_SCOPE_OBJECT
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

		auto get_callbacks() const noexcept -> VkAllocationCallbacks const* {
			return callbacks_;
		}
	private:
		VkAllocationCallbacks const* callbacks_;
	};

	template<typename T>
	using Vector = std::vector<T, Allocator<T>>;

	inline auto to_string(VkResult result) -> std::string_view {
		switch (result)
		{
		case VK_SUCCESS: return "Success";
		case VK_NOT_READY: return "NotReady";
		case VK_TIMEOUT: return "Timeout";
		case VK_EVENT_SET: return "EventSet";
		case VK_EVENT_RESET: return "EventReset";
		case VK_INCOMPLETE: return "Incomplete";
		case VK_ERROR_OUT_OF_HOST_MEMORY: return "OutOfHostMemory";
		case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "OutOfDeviceMemory";
		case VK_ERROR_INITIALIZATION_FAILED: return "InitializationFailed";
		case VK_ERROR_DEVICE_LOST: return "DeviceLost";
		case VK_ERROR_MEMORY_MAP_FAILED: return "MemoryMapFailed";
		case VK_ERROR_LAYER_NOT_PRESENT: return "LayerNotPresent";
		case VK_ERROR_EXTENSION_NOT_PRESENT: return "ExtensionNotPresent";
		case VK_ERROR_FEATURE_NOT_PRESENT: return "FeatureNotPresent";
		case VK_ERROR_INCOMPATIBLE_DRIVER: return "IncompatibleDriver";
		case VK_ERROR_TOO_MANY_OBJECTS: return "TooManyObjects";
		case VK_ERROR_FORMAT_NOT_SUPPORTED: return "FormatNotSupported";
		case VK_ERROR_FRAGMENTED_POOL: return "FragmentedPool";
		case VK_ERROR_UNKNOWN: return "Unknown";
		case VK_ERROR_OUT_OF_POOL_MEMORY: return "OutOfPoolMemory";
		case VK_ERROR_INVALID_EXTERNAL_HANDLE: return "InvalidExternalHandle";
		case VK_ERROR_FRAGMENTATION: return "Fragmentation";
		case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS: return "InvalidOpaqueCaptureAddress";
		case VK_PIPELINE_COMPILE_REQUIRED: return "PipelineCompileRequired";
		case VK_ERROR_NOT_PERMITTED: return "NotPermitted";
		case VK_ERROR_SURFACE_LOST_KHR: return "SurfaceLost";
		case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "NativeWindowInUse";
		case VK_SUBOPTIMAL_KHR: return "Suboptimal";
		case VK_ERROR_OUT_OF_DATE_KHR: return "OutOfDate";
		case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "IncompatibleDisplay";
		case VK_ERROR_VALIDATION_FAILED_EXT: return "ValidationFailed";
		case VK_ERROR_INVALID_SHADER_NV: return "InvalidShader";
		case VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR: return "ImageUsageNotSupported";
		case VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR: return "VideoPictureLayoutNotSupported";
		case VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR: return "VideoProfileOperationNotSupported";
		case VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR: return "VideoProfileFormatNotSupported";
		case VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR: return "VideoProfileCodecNotSupported";
		case VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR: return "VideoStdVersionNotSupported";
		case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT: return "InvalidDrmFormatModifierPlaneLayout";
		case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT: return "FullScreenExclusiveModeLost";
		case VK_THREAD_IDLE_KHR: return "ThreadIdle";
		case VK_THREAD_DONE_KHR: return "ThreadDone";
		case VK_OPERATION_DEFERRED_KHR: return "OperationDeferred";
		case VK_OPERATION_NOT_DEFERRED_KHR: return "OperationNotDeferred";
		case VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR: return "InvalidVideoStdParameters";
		case VK_ERROR_COMPRESSION_EXHAUSTED_EXT: return "CompressionExhausted";
		case VK_INCOMPATIBLE_SHADER_BINARY_EXT: return "IncompatibleShaderBinary";
		case VK_PIPELINE_BINARY_MISSING_KHR: return "PipelineBinaryMissing";
		case VK_ERROR_NOT_ENOUGH_SPACE_KHR: return "NotEnoughSpace";
		default: return "Undefined";
		}
	}

	inline auto to_string(VkObjectType object_type) -> std::string_view {
		switch (object_type)
		{
		case VK_OBJECT_TYPE_UNKNOWN:  return "Unknown";
		case VK_OBJECT_TYPE_INSTANCE:  return "Instance";
		case VK_OBJECT_TYPE_PHYSICAL_DEVICE:  return "PhysicalDevice";
		case VK_OBJECT_TYPE_DEVICE:  return "Device";
		case VK_OBJECT_TYPE_QUEUE: return "Queue";
		case VK_OBJECT_TYPE_SEMAPHORE:  return "Semaphore";
		case VK_OBJECT_TYPE_COMMAND_BUFFER: return "CommandBuffer";
		case VK_OBJECT_TYPE_FENCE:  return "Fence";
		case VK_OBJECT_TYPE_DEVICE_MEMORY: return "DeviceMemory";
		case VK_OBJECT_TYPE_BUFFER: return "Buffer";
		case VK_OBJECT_TYPE_IMAGE: return "Image";
		case VK_OBJECT_TYPE_EVENT: return "Event";
		case VK_OBJECT_TYPE_QUERY_POOL: return "QueryPool";
		case VK_OBJECT_TYPE_BUFFER_VIEW: return "BufferView";
		case VK_OBJECT_TYPE_IMAGE_VIEW: return "ImageView";
		case VK_OBJECT_TYPE_SHADER_MODULE: return "ShaderModule";
		case VK_OBJECT_TYPE_PIPELINE_CACHE: return "PipelineCache";
		case VK_OBJECT_TYPE_PIPELINE_LAYOUT: return "PipelineLayout";
		case VK_OBJECT_TYPE_RENDER_PASS: return "RenderPass";
		case VK_OBJECT_TYPE_PIPELINE: return "Pipeline";
		case VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT: return "DescriptorSetLayout";
		case VK_OBJECT_TYPE_SAMPLER: return "Sampler";
		case VK_OBJECT_TYPE_DESCRIPTOR_POOL: return "DescriptorPool";
		case VK_OBJECT_TYPE_DESCRIPTOR_SET: return "DescriptorSet";
		case VK_OBJECT_TYPE_FRAMEBUFFER: return "Framebuffer";
		case VK_OBJECT_TYPE_COMMAND_POOL: return "CommandPool";
		case VK_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION: return "SamplerYcbcrConversion";
		case VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE: return "DescriptorUpdateTemplate";
		case VK_OBJECT_TYPE_PRIVATE_DATA_SLOT: return "PrivateDataSlot";
		case VK_OBJECT_TYPE_SURFACE_KHR: return "SurfaceKHR";
		case VK_OBJECT_TYPE_SWAPCHAIN_KHR: return "SwapchainKHR";
		case VK_OBJECT_TYPE_DISPLAY_KHR: return "DisplayKHR";
		case VK_OBJECT_TYPE_DISPLAY_MODE_KHR: return "DisplayModeKHR";
		case VK_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT: return "DebugReportCallbackEXT";
		case VK_OBJECT_TYPE_VIDEO_SESSION_KHR: return "VideoSessionKHR";
		case VK_OBJECT_TYPE_VIDEO_SESSION_PARAMETERS_KHR: return "VideoSessionParametersKHR";
		case VK_OBJECT_TYPE_CU_MODULE_NVX: return "CuModuleNVX";
		case VK_OBJECT_TYPE_CU_FUNCTION_NVX: return "CuFunctionNVX";
		case VK_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT: return "DebugUtilsMessengerEXT";
		case VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR: return "AccelerationStructureKHR";
		case VK_OBJECT_TYPE_VALIDATION_CACHE_EXT: return "ValidationCacheEXT";
		case VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_NV: return "AccelerationStructureNV";
		case VK_OBJECT_TYPE_PERFORMANCE_CONFIGURATION_INTEL: return "PerformanceConfigurationINTEL";
		case VK_OBJECT_TYPE_DEFERRED_OPERATION_KHR: return "DeferredOperationKHR";
		case VK_OBJECT_TYPE_INDIRECT_COMMANDS_LAYOUT_NV: return "IndirectCommandsLayoutNV";
		case VK_OBJECT_TYPE_CUDA_MODULE_NV: return "CudaModuleNV";
		case VK_OBJECT_TYPE_CUDA_FUNCTION_NV: return "CudaFunctionNV";
		case VK_OBJECT_TYPE_BUFFER_COLLECTION_FUCHSIA: return "BufferCollectionFUCHSIA";
		case VK_OBJECT_TYPE_MICROMAP_EXT: return "MicromapEXT";
		case VK_OBJECT_TYPE_OPTICAL_FLOW_SESSION_NV: return "OpticalFlowSessionNV";
		case VK_OBJECT_TYPE_SHADER_EXT: return "ShaderEXT";
		case VK_OBJECT_TYPE_PIPELINE_BINARY_KHR: return "PipelineBinaryKHR";
		case VK_OBJECT_TYPE_INDIRECT_COMMANDS_LAYOUT_EXT: return "IndirectCommandsLayoutEXT";
		case VK_OBJECT_TYPE_INDIRECT_EXECUTION_SET_EXT: return "IndirectExecutionSetEXT";
		default: return "Undefined";
		}
	}

	inline auto to_string(VkPhysicalDeviceType type) -> std::string_view {
		switch (type)
		{
		case VK_PHYSICAL_DEVICE_TYPE_OTHER: return "Other";
		case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "Integrated";
		case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return "Discrete";
		case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return "Virtual";
		case VK_PHYSICAL_DEVICE_TYPE_CPU: return "CPU";
		default: return "Undefined";
		}
	}

	auto enumerate_instance_extension_properties(const char* layer_name = nullptr, VkAllocationCallbacks const* allocator = nullptr) -> Result<Vector<VkExtensionProperties>>;
	auto enumerate_instance_layer_properties(VkAllocationCallbacks const* allocator = nullptr) -> Result<Vector<VkLayerProperties>>;
	auto enumerate_instance_version() -> Result<uint32_t>;

	template<typename T>
	class BaseHandle {
	public:
		BaseHandle(T handle = VK_NULL_HANDLE, VkAllocationCallbacks const* allocator = nullptr) :
			handle_{ handle },
			allocator_{ allocator } {
		}

		BaseHandle(const BaseHandle& other) = delete;
		BaseHandle& operator=(const BaseHandle& other) = delete;

		BaseHandle(BaseHandle&& other) :
			handle_{ std::exchange(other.handle_, VK_NULL_HANDLE) },
			allocator_{ std::exchange(other.allocator_, nullptr) }
		{
		}

		BaseHandle& operator=(BaseHandle&& other) {
			if (this != &other) {
				handle_ = std::exchange(other.handle_, VK_NULL_HANDLE);
				allocator_ = std::exchange(other.allocator_, nullptr);
			}
			return *this;
		}

		auto get_handle() const noexcept -> T { return handle_; }
		auto get_allocator() const noexcept -> const VkAllocationCallbacks* { return allocator_; }

		explicit operator bool() const noexcept { return is_valid(); }
		explicit operator T() const noexcept { return handle_; }
		explicit operator uint64_t() const noexcept { return reinterpret_cast<uint64_t>(handle_); }

		auto is_valid() const noexcept -> bool { return handle_ != VK_NULL_HANDLE; }
	protected:
		T handle_{ VK_NULL_HANDLE };
		VkAllocationCallbacks const* allocator_{ nullptr };
	};

	class Instance : public BaseHandle<VkInstance> {
	public:
		static constexpr auto object_type{ VK_OBJECT_TYPE_INSTANCE };
		static constexpr std::string_view object_name{ "VkInstance" };

		Instance(VkInstance instance = VK_NULL_HANDLE, VkAllocationCallbacks const* allocator = nullptr) :
			BaseHandle(instance, allocator) {}

		~Instance();

		Instance(const Instance& other) = delete;
		Instance& operator=(const Instance& other) = delete;

		Instance(Instance&& other) :
			BaseHandle(std::move(other)) {}

		Instance& operator=(Instance&& other) {
			BaseHandle::operator=(std::move(other));
			return *this;
		}

		// Core Vulkan 1.0 functions
		auto enumerate_physical_devices() const -> Result<Vector<PhysicalDevice>>;
		auto get_proc_addr(const char* name) const -> PFN_vkVoidFunction;

		// Vulkan 1.1 functions
		auto enumerate_physical_device_groups() const -> Result<Vector<VkPhysicalDeviceGroupProperties>>;

		// Extension functions
		auto create_debug_utils_messenger_ext(const VkDebugUtilsMessengerCreateInfoEXT& create_info) const -> Result<VkDebugUtilsMessengerEXT>;
		auto destroy_debug_utils_messenger_ext(VkDebugUtilsMessengerEXT messenger) const -> void;
		auto submit_debug_utils_message_ext(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
			VkDebugUtilsMessageTypeFlagsEXT message_types,
			const VkDebugUtilsMessengerCallbackDataEXT& callback_data) const -> void;

		// Debug report (legacy)
		auto create_debug_report_callback_ext(const VkDebugReportCallbackCreateInfoEXT& create_info) const -> Result<VkDebugReportCallbackEXT>;
		auto destroy_debug_report_callback_ext(VkDebugReportCallbackEXT callback) const -> void;
		auto debug_report_message_ext(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT object_type,
			uint64_t object, size_t location, int32_t message_code,
			const char* layer_prefix, const char* message) const -> void;

		// Surface creation functions
		auto destroy_surface_khr(VkSurfaceKHR surface) const -> void;

#if defined(VK_USE_PLATFORM_WIN32_KHR)
		auto create_surface_khr(const VkWin32SurfaceCreateInfoKHR& create_info) const -> Result<VkSurfaceKHR>;
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
		auto create_surface_khr(const VkXlibSurfaceCreateInfoKHR& create_info) const -> Result<VkSurfaceKHR>;
#elif defined(VK_USE_PLATFORM_XCB_KHR)
		auto create_surface_khr(const VkXcbSurfaceCreateInfoKHR& create_info) const -> Result<VkSurfaceKHR>;
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
		auto create_surface_khr(const VkWaylandSurfaceCreateInfoKHR& create_info) const -> Result<VkSurfaceKHR>;
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
		auto create_surface_khr(const VkAndroidSurfaceCreateInfoKHR& create_info) const -> Result<VkSurfaceKHR>;
#elif defined(VK_USE_PLATFORM_MACOS_MVK)
		auto create_surface_mvk(const VkMacOSSurfaceCreateInfoMVK& create_info) const -> Result<VkSurfaceKHR>;
#elif defined(VK_USE_PLATFORM_IOS_MVK)
		auto create_surface_mvk(const VkIOSSurfaceCreateInfoMVK& create_info) const -> Result<VkSurfaceKHR>;
#elif defined(VK_USE_PLATFORM_METAL_EXT)
		auto create_surface_ext(const VkMetalSurfaceCreateInfoEXT& create_info) const -> Result<VkSurfaceKHR>;
#endif

		// Display functions
		auto create_display_mode_khr(VkDisplayKHR display, const VkDisplayModeCreateInfoKHR& create_info) const -> Result<VkDisplayModeKHR>;
		auto get_display_plane_capabilities_khr(VkDisplayModeKHR mode, uint32_t plane_index) const -> Result<VkDisplayPlaneCapabilitiesKHR>;
		auto create_display_plane_surface_khr(const VkDisplaySurfaceCreateInfoKHR& create_info) const -> Result<VkSurfaceKHR>;

		// Headless surface
		auto create_headless_surface_ext(const VkHeadlessSurfaceCreateInfoEXT& create_info) const -> Result<VkSurfaceKHR>;
	};

	class InstanceBuilder {
	public:
		InstanceBuilder(VkAllocationCallbacks const* allocator);

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

		auto add_validation_feature_enable(VkValidationFeatureEnableEXT enables) -> InstanceBuilder& {
			validation_feature_enables_.push_back(enables);
			return *this;
		}

		auto add_validation_feature_disable(VkValidationFeatureDisableEXT disables) -> InstanceBuilder& {
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
		auto add_flag(VkInstanceCreateFlagBits flags) -> InstanceBuilder& {
			create_info_.flags |= flags;
			return *this;
		}

		auto set_flags(VkInstanceCreateFlags flags) -> InstanceBuilder& {
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
		auto get_enabled_extensions() const -> const Vector<const char*>& { return enabled_extensions_; }
		auto get_enabled_layers() const -> const Vector<const char*>& { return enabled_layers_; }
		auto get_app_info() const -> const VkApplicationInfo& { return app_info_; }

	private:
		VkApplicationInfo app_info_{};
		VkInstanceCreateInfo create_info_{};
		Vector<const char*> enabled_extensions_;
		Vector<const char*> enabled_layers_;
		Vector<VkValidationFeatureEnableEXT> validation_feature_enables_;
		Vector<VkValidationFeatureDisableEXT> validation_feature_disables_;
		const VkAllocationCallbacks* allocator_{ nullptr };

		bool enable_debug_utils_{ false };
		bool enable_surface_{ false };
		bool enable_portability_{ false };
	};

	class PhysicalDevice : public BaseHandle<VkPhysicalDevice> {
	public:
		static constexpr auto object_type{ VK_OBJECT_TYPE_PHYSICAL_DEVICE };
		static constexpr std::string_view object_name{ "VkPhysicalDevice" };

		PhysicalDevice(VkPhysicalDevice handle = VK_NULL_HANDLE, const VkAllocationCallbacks* allocator = nullptr) :
			BaseHandle(handle, allocator) {}

		~PhysicalDevice() = default;

		PhysicalDevice(const PhysicalDevice& other) = delete;
		PhysicalDevice& operator=(const PhysicalDevice& other) = delete;

		PhysicalDevice(PhysicalDevice&& other) :
			BaseHandle(std::move(other)) {}

		PhysicalDevice& operator=(PhysicalDevice&& other) {
			BaseHandle::operator=(std::move(other));
			return *this;
		}

		auto create_device(const VkDeviceCreateInfo& create_info) const -> Result<Device>;

		// Core Vulkan 1.0 functions
		auto get_features() const -> VkPhysicalDeviceFeatures;
		auto get_format_properties(VkFormat format) const -> VkFormatProperties;
		auto get_image_format_properties(VkFormat format, VkImageType type, VkImageTiling tiling,
			VkImageUsageFlags usage, VkImageCreateFlags flags) const -> Result<VkImageFormatProperties>;
		auto get_queue_family_properties() const -> Vector<VkQueueFamilyProperties>;
		auto get_memory_properties() const -> VkPhysicalDeviceMemoryProperties;
		auto get_properties() const -> VkPhysicalDeviceProperties;
		auto get_sparse_image_format_properties(VkFormat format, VkImageType type, VkSampleCountFlagBits samples,
			VkImageUsageFlags usage, VkImageTiling tiling) const -> Vector<VkSparseImageFormatProperties>;

		// Vulkan 1.1 functions
		auto get_features2(void* chain = nullptr) const -> VkPhysicalDeviceFeatures2;
		auto get_properties2() const -> VkPhysicalDeviceProperties2;
		auto get_format_properties2(VkFormat format) const -> VkFormatProperties2;
		auto get_image_format_properties2(const VkPhysicalDeviceImageFormatInfo2& image_format_info) const -> Result<VkImageFormatProperties2>;
		auto get_queue_family_properties2() const -> Vector<VkQueueFamilyProperties2>;
		auto get_memory_properties2() const -> VkPhysicalDeviceMemoryProperties2;
		auto get_sparse_image_format_properties2(const VkPhysicalDeviceSparseImageFormatInfo2& format_info) const -> Vector<VkSparseImageFormatProperties2>;

		// Extension query functions
		auto enumerate_device_extension_properties(const char* layer_name = nullptr) const -> Result<Vector<VkExtensionProperties>>;
		auto enumerate_device_layer_properties() const -> Result<Vector<VkLayerProperties>>;

		// Surface support functions (VK_KHR_surface)
		auto get_surface_support_khr(uint32_t queue_family_index, VkSurfaceKHR surface) const -> Result<VkBool32>;
		auto get_surface_capabilities_khr(VkSurfaceKHR surface) const -> Result<VkSurfaceCapabilitiesKHR>;
		auto get_surface_formats_khr(VkSurfaceKHR surface) const -> Result<Vector<VkSurfaceFormatKHR>>;
		auto get_surface_present_modes_khr(VkSurfaceKHR surface) const -> Result<Vector<VkPresentModeKHR>>;

		// Display functions (VK_KHR_display)
		auto get_display_properties_khr() const -> Result<Vector<VkDisplayPropertiesKHR>>;
		auto get_display_plane_properties_khr() const -> Result<Vector<VkDisplayPlanePropertiesKHR>>;
		auto get_display_planes_supported_displays_khr(uint32_t plane_index) const -> Result<Vector<VkDisplayKHR>>;

		// Tool properties (Vulkan 1.3)
		auto get_tool_properties() const -> Result<Vector<VkPhysicalDeviceToolProperties>>;

		// External memory/fence/semaphore properties
		auto get_external_buffer_properties(const VkPhysicalDeviceExternalBufferInfo& external_buffer_info) const -> VkExternalBufferProperties;
		auto get_external_fence_properties(const VkPhysicalDeviceExternalFenceInfo& external_fence_info) const -> VkExternalFenceProperties;
		auto get_external_semaphore_properties(const VkPhysicalDeviceExternalSemaphoreInfo& external_semaphore_info) const -> VkExternalSemaphoreProperties;

		// Multisample properties
		auto get_multisample_properties_ext(VkSampleCountFlagBits samples) const -> VkMultisamplePropertiesEXT;

		// Calibrateable time domains (VK_EXT_calibrated_timestamps)
		auto get_calibrateable_time_domains_ext() const -> Result<Vector<VkTimeDomainEXT>>;

		// Fragment shading rate properties (VK_KHR_fragment_shading_rate)
		auto get_fragment_shading_rates_khr() const -> Result<Vector<VkPhysicalDeviceFragmentShadingRateKHR>>;

		// Cooperative matrix properties (VK_NV_cooperative_matrix)
		auto get_cooperative_matrix_properties_nv() const -> Result<Vector<VkCooperativeMatrixPropertiesNV>>;

		// Supported framebuffer mixed samples combinations (VK_NV_coverage_reduction_mode)
		auto get_supported_framebuffer_mixed_samples_combinations_nv() const -> Result<std::vector<VkFramebufferMixedSamplesCombinationNV>>;
	};

	class Device : public BaseHandle<VkDevice> {
	public:
		static constexpr auto object_type{ VK_OBJECT_TYPE_DEVICE };
		static constexpr std::string_view object_name{ "VkDevice" };

		Device(VkDevice device = VK_NULL_HANDLE, VkAllocationCallbacks const* allocator = nullptr) :
			BaseHandle(device, allocator) {}

		~Device();

		Device(const Device& other) = delete;
		Device& operator=(const Device& other) = delete;

		Device(Device&& other) :
			BaseHandle(std::move(other)) {}

		Device& operator=(Device&& other) {
			BaseHandle::operator=(std::move(other));
			return *this;
		}
	};

	class DeviceSelector {
	public:
		DeviceSelector(Instance const& instance);

		auto set_surface(VkSurfaceKHR surface) -> DeviceSelector& {
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

		auto set_preferred_device_type(VkPhysicalDeviceType type) -> DeviceSelector& {
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
			StructureType feature{ FeatureTraits<StructureType>::structure_type };
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
		Vector<PhysicalDevice> physical_devices_;

		VkAllocationCallbacks const* allocator_{ nullptr };
		VkSurfaceKHR surface_{ VK_NULL_HANDLE };
		uint32_t minimal_api_ver{ VK_VERSION_1_0 };
		VkPhysicalDeviceType preferred_type_{ VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU };

		Vector<std::pair<const char*, bool>> requested_extensions_;

		Vector<std::shared_ptr<void>> requested_features_;
		void* last_feature_ptr_{ nullptr };
	};

	template<typename T>
	class DeviceHandle : public BaseHandle<T> {
	public:
		DeviceHandle(Device const& device, T handle) :
			BaseHandle(handle, device.get_allocator()),
			device_{ device } {}

		~DeviceHandle() {
			if (handle_) {
				device_.destroy(handle);
			}
		}

		DeviceHandle(const DeviceHandle& other) = delete;
		DeviceHandle& operator=(const DeviceHandle& other) = delete;

		DeviceHandle(DeviceHandle&& other) :
			BaseHandle(std::move(other)),
			device_{other.device_} {}

		DeviceHandle& operator=(DeviceHandle&& other) {
			BaseHandle::operator=(std::move(other));
			device_ = other.device_;

			return *this;
		}

		auto get_device() const -> Device const& {
			return device_;
		}
	protected:
		Device const& device_;
	};

	class DebugInterface {
	public:
		virtual ~DebugInterface() = default;

		virtual auto set_name(VkObjectType object_type, uint64_t object_handle, std::string_view name) const -> void = 0;
		virtual auto set_tag(VkObjectType object_type, uint64_t object_handle, uint64_t tag_name, const void* tag_data, size_t tag_data_size) const -> void = 0;
		virtual auto push_label(VkCommandBuffer command_buffer, std::string_view name, std::array<float, 4ull> color) const -> void = 0;
		virtual auto pop_label(VkCommandBuffer command_buffer) const -> void = 0;
		virtual auto insert_label(VkCommandBuffer command_buffer, std::string_view name, std::array<float, 4ull> color) const -> void = 0;
	};

	class DebugUtils final : public DebugInterface {
	public:
		static constexpr auto object_type{ VK_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT };
		static constexpr std::string_view object_name{ "VkDebugUtilsMessengerEXT" };

		~DebugUtils() override;

		DebugUtils() = default;
		DebugUtils(const DebugUtils& other) = delete;
		DebugUtils& operator=(const DebugUtils& other) = delete;

		DebugUtils(DebugUtils&& other) :
			instance_{ std::exchange(other.instance_, VK_NULL_HANDLE) },
			device_{ std::exchange(other.device_, VK_NULL_HANDLE) },
			allocator_{ std::exchange(other.allocator_, nullptr) },
			handle_{ std::exchange(other.handle_, VK_NULL_HANDLE) } {
		}

		DebugUtils& operator=(DebugUtils&& other) {
			instance_ = std::exchange(other.instance_, VK_NULL_HANDLE);
			device_ = std::exchange(other.device_, VK_NULL_HANDLE);
			allocator_ = std::exchange(other.allocator_, nullptr);
			handle_ = std::exchange(other.handle_, VK_NULL_HANDLE);
			return *this;
		}

		static auto create_unique(Instance const& instance, Device const& device) -> Result<std::unique_ptr<DebugUtils>>;

		auto set_name(VkObjectType object_type, uint64_t object_handle, std::string_view name) const -> void override;
		auto set_tag(VkObjectType object_type, uint64_t object_handle, uint64_t tag_name, const void* tag_data, size_t tag_data_size) const -> void override;
		auto push_label(VkCommandBuffer command_buffer, std::string_view name, std::array<float, 4ull> color) const -> void override;
		auto pop_label(VkCommandBuffer command_buffer) const -> void override;
		auto insert_label(VkCommandBuffer command_buffer, std::string_view name, std::array<float, 4ull> color) const -> void override;
	private:
		auto _create(Instance const& instance, Device const& device) -> VkResult;

		VkInstance instance_{ VK_NULL_HANDLE };
		VkDevice device_{ VK_NULL_HANDLE };
		VkAllocationCallbacks const* allocator_{ nullptr };
		VkDebugUtilsMessengerEXT handle_{ VK_NULL_HANDLE };
	};

	class DebugReport final : public DebugInterface {
	public:
		static constexpr auto object_type{ VK_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT };
		static constexpr std::string_view object_name{ "VkDebugReportCallbackEXT" };
	
		~DebugReport() override;

		DebugReport() = default;
		DebugReport(const DebugReport& other) = delete;
		DebugReport& operator=(const DebugReport& other) = delete;

		DebugReport(DebugReport&& other) :
			instance_{ std::exchange(other.instance_, VK_NULL_HANDLE) },
			device_{ std::exchange(other.device_, VK_NULL_HANDLE) },
			allocator_{ std::exchange(other.allocator_, nullptr) },
			handle_{ std::exchange(other.handle_, VK_NULL_HANDLE) } {
		}

		DebugReport& operator=(DebugReport&& other) {
			instance_ = std::exchange(other.instance_, VK_NULL_HANDLE);
			device_ = std::exchange(other.device_, VK_NULL_HANDLE);
			allocator_ = std::exchange(other.allocator_, nullptr);
			handle_ = std::exchange(other.handle_, VK_NULL_HANDLE);
			return *this;
		}
	
		static auto create_unique(Instance const& instance, Device const& device) -> Result< std::unique_ptr<DebugReport>>;
	
		auto set_name(VkObjectType object_type, uint64_t object_handle, std::string_view name) const -> void override;
		auto set_tag(VkObjectType object_type, uint64_t object_handle, uint64_t tag_name, const void* tag_data, size_t tag_data_size) const -> void override;
		auto push_label(VkCommandBuffer command_buffer, std::string_view name, std::array<float, 4ull> color) const -> void override;
		auto pop_label(VkCommandBuffer command_buffer) const -> void override;
		auto insert_label(VkCommandBuffer command_buffer, std::string_view name, std::array<float, 4ull> color) const -> void override;
	private:
		auto _create(Instance const& instance, Device const& device) -> VkResult;
	
		VkInstance instance_{ VK_NULL_HANDLE };
		VkDevice device_{ VK_NULL_HANDLE };
		VkAllocationCallbacks const* allocator_{ nullptr };
		VkDebugReportCallbackEXT handle_{ VK_NULL_HANDLE };
	};

	class DebugNone final : public DebugInterface {
	public:
		~DebugNone() override = default;

		static auto create_unique() -> Result<std::unique_ptr<DebugNone>> { return std::make_unique<DebugNone>(); }

		auto set_name(VkObjectType object_type, uint64_t object_handle, std::string_view name) const -> void override;
		auto set_tag(VkObjectType object_type, uint64_t object_handle, uint64_t tag_name, const void* tag_data, size_t tag_data_size) const -> void override;
		auto push_label(VkCommandBuffer command_buffer, std::string_view name, std::array<float, 4ull> color) const -> void override;
		auto pop_label(VkCommandBuffer command_buffer) const -> void override;
		auto insert_label(VkCommandBuffer command_buffer, std::string_view name, std::array<float, 4ull> color) const -> void override;
	};
}