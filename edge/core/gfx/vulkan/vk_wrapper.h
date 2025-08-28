#pragma once

#include <array>
#include <expected>
#include <string_view>
#include <vector>
#include <memory>

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
	class Device;

	template<typename T>
	using Result = std::expected<T, VkResult>;

	inline auto to_string(VkResult result) -> std::string_view {
		switch (result)
		{
		case VK_SUCCESS: return "VK_SUCCESS";
		case VK_NOT_READY: return "VK_NOT_READY";
		case VK_TIMEOUT: return "VK_TIMEOUT";
		case VK_EVENT_SET: return "VK_EVENT_SET";
		case VK_EVENT_RESET: return "VK_EVENT_RESET";
		case VK_INCOMPLETE: return "VK_INCOMPLETE";
		case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
		case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
		case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
		case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
		case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
		case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
		case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
		case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
		case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
		case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
		case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
		case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
		case VK_ERROR_UNKNOWN: return "VK_ERROR_UNKNOWN";
		case VK_ERROR_OUT_OF_POOL_MEMORY: return "VK_ERROR_OUT_OF_POOL_MEMORY";
		case VK_ERROR_INVALID_EXTERNAL_HANDLE: return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
		case VK_ERROR_FRAGMENTATION: return "VK_ERROR_FRAGMENTATION";
		case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS: return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
		case VK_PIPELINE_COMPILE_REQUIRED: return "VK_PIPELINE_COMPILE_REQUIRED";
		case VK_ERROR_NOT_PERMITTED: return "VK_ERROR_NOT_PERMITTED";
		case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
		case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
		case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
		case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
		case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
		case VK_ERROR_VALIDATION_FAILED_EXT: return "VK_ERROR_VALIDATION_FAILED_EXT";
		case VK_ERROR_INVALID_SHADER_NV: return "VK_ERROR_INVALID_SHADER_NV";
		case VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR: return "VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR";
		case VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR";
		case VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR";
		case VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR";
		case VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR";
		case VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR";
		case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT: return "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
		case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT: 		return "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
		case VK_THREAD_IDLE_KHR: return "VK_THREAD_IDLE_KHR";
		case VK_THREAD_DONE_KHR: return "VK_THREAD_DONE_KHR";
		case VK_OPERATION_DEFERRED_KHR: return "VK_OPERATION_DEFERRED_KHR";
		case VK_OPERATION_NOT_DEFERRED_KHR: return "VK_OPERATION_NOT_DEFERRED_KHR";
		case VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR: return "VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR";
		case VK_ERROR_COMPRESSION_EXHAUSTED_EXT: return "VK_ERROR_COMPRESSION_EXHAUSTED_EXT";
		case VK_INCOMPATIBLE_SHADER_BINARY_EXT: return "VK_INCOMPATIBLE_SHADER_BINARY_EXT";
		case VK_PIPELINE_BINARY_MISSING_KHR: return "VK_PIPELINE_BINARY_MISSING_KHR";
		case VK_ERROR_NOT_ENOUGH_SPACE_KHR: return "VK_ERROR_NOT_ENOUGH_SPACE_KHR";
		default: return "UNKNOWN_ENUMERATOR";
		}
	}

	inline auto to_string(VkObjectType object_type) -> std::string_view {
		switch (object_type)
		{
		case VK_OBJECT_TYPE_UNKNOWN:  return "UNKNOWN";
		case VK_OBJECT_TYPE_INSTANCE:  return "INSTANCE";
		case VK_OBJECT_TYPE_PHYSICAL_DEVICE:  return "PHYSICAL DEVICE";
		case VK_OBJECT_TYPE_DEVICE:  return "DEVICE";
		case VK_OBJECT_TYPE_QUEUE: return "QUEUE";
		case VK_OBJECT_TYPE_SEMAPHORE:  return "SEMAPHORE";
		case VK_OBJECT_TYPE_COMMAND_BUFFER: return "COMMAND BUFFER";
		case VK_OBJECT_TYPE_FENCE:  return "FENCE";
		case VK_OBJECT_TYPE_DEVICE_MEMORY: return "DEVICE MEMORY";
		case VK_OBJECT_TYPE_BUFFER: return "BUFFER";
		case VK_OBJECT_TYPE_IMAGE: return "IMAGE";
		case VK_OBJECT_TYPE_EVENT: return "EVENT";
		case VK_OBJECT_TYPE_QUERY_POOL: return "QUERY POOL";
		case VK_OBJECT_TYPE_BUFFER_VIEW: return "BUFFER VIEW";
		case VK_OBJECT_TYPE_IMAGE_VIEW: return "IMAGE VIEW";
		case VK_OBJECT_TYPE_SHADER_MODULE: return "SHADER MODULE";
		case VK_OBJECT_TYPE_PIPELINE_CACHE: return "PIPELINE CACHE";
		case VK_OBJECT_TYPE_PIPELINE_LAYOUT: return "PIPELINE LAYOUT";
		case VK_OBJECT_TYPE_RENDER_PASS: return "RENDER PASS";
		case VK_OBJECT_TYPE_PIPELINE: return "PIPELINE";
		case VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT: return "DESCRIPTOR SET LAYOUT";
		case VK_OBJECT_TYPE_SAMPLER: return "SAMPLER";
		case VK_OBJECT_TYPE_DESCRIPTOR_POOL: return "DESCRIPTOR POOL";
		case VK_OBJECT_TYPE_DESCRIPTOR_SET: return "DESCRIPTOR SET";
		case VK_OBJECT_TYPE_FRAMEBUFFER: return "FRAMEBUFFER";
		case VK_OBJECT_TYPE_COMMAND_POOL: return "COMMAND POOL";
		case VK_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION: return "SAMPLER YCBCR CONVERSION";
		case VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE: return "DESCRIPTOR UPDATE TEMPLATE";
		case VK_OBJECT_TYPE_PRIVATE_DATA_SLOT: return "PRIVATE DATA SLOT";
		case VK_OBJECT_TYPE_SURFACE_KHR: return "SURFACE KHR";
		case VK_OBJECT_TYPE_SWAPCHAIN_KHR: return "SWAPCHAIN";
		case VK_OBJECT_TYPE_DISPLAY_KHR: return "DISPLAY KHR";
		case VK_OBJECT_TYPE_DISPLAY_MODE_KHR: return "DISPLAY MODE KHR";
		case VK_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT: return "DEBUG REPORT CALLBACK EXT";
		case VK_OBJECT_TYPE_VIDEO_SESSION_KHR: return "VIDEO SESSION KHR";
		case VK_OBJECT_TYPE_VIDEO_SESSION_PARAMETERS_KHR: return "VIDEO SESSION PARAMETERS KHR";
		case VK_OBJECT_TYPE_CU_MODULE_NVX: return "CU MODULE NVX";
		case VK_OBJECT_TYPE_CU_FUNCTION_NVX: return "CU FUNCTION NVX";
		case VK_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT: return "DEBUG UTILS MESSENGER";
		case VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR: return "ACCELERATION STRUCTURE KHR";
		case VK_OBJECT_TYPE_VALIDATION_CACHE_EXT: return "VALIDATION CACKE EXT";
		case VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_NV: return "ACCELERATION STRUCTURE NV";
		case VK_OBJECT_TYPE_PERFORMANCE_CONFIGURATION_INTEL: return "PERFORMANCE CONFIGURATION INTEL";
		case VK_OBJECT_TYPE_DEFERRED_OPERATION_KHR: return "DEFERRED OPERATION KHR";
		case VK_OBJECT_TYPE_INDIRECT_COMMANDS_LAYOUT_NV: return "INDIRECT COMMANDS LAYOUT NV";
		case VK_OBJECT_TYPE_CUDA_MODULE_NV: return "CUDA MODULE NV";
		case VK_OBJECT_TYPE_CUDA_FUNCTION_NV: return "CUDA FUNCTION NV";
		case VK_OBJECT_TYPE_BUFFER_COLLECTION_FUCHSIA: return "BUFFER COLLECTION FUCHSIA";
		case VK_OBJECT_TYPE_MICROMAP_EXT: return "MICROMAP EXT";
		case VK_OBJECT_TYPE_OPTICAL_FLOW_SESSION_NV: return "OPTICAL FLOW SESSION NV";
		case VK_OBJECT_TYPE_SHADER_EXT: return "SHADER EXT";
		case VK_OBJECT_TYPE_PIPELINE_BINARY_KHR: return "PIPELINE BINARY KHR";
		case VK_OBJECT_TYPE_INDIRECT_COMMANDS_LAYOUT_EXT: return "INDIRECT COMMANDS LAYOUT EXT";
		case VK_OBJECT_TYPE_INDIRECT_EXECUTION_SET_EXT: return "INDIRECT EXECUTION SET EXT";
		default: return "UNKNOWN_ENUMERATOR";
		}
	}

	auto enumerate_instance_extension_properties(const char* layer_name = nullptr) -> std::expected<std::vector<VkExtensionProperties>, VkResult>;
	auto enumerate_instance_layer_properties() -> std::expected<std::vector<VkLayerProperties>, VkResult>;

	class Instance {
	public:
		static constexpr auto object_type{ VK_OBJECT_TYPE_INSTANCE };
		static constexpr std::string_view object_name{ "VkInstance" };

		Instance();
		~Instance();

		Instance(const Instance& other) = delete;
		Instance& operator=(const Instance& other) = delete;

		Instance(Instance&& other) :
			handle_{ std::exchange(other.handle_, VK_NULL_HANDLE) },
			allocator_{ std::exchange(other.allocator_, nullptr) } {}

		Instance& operator=(Instance&& other) {
			handle_ = std::exchange(other.handle_, VK_NULL_HANDLE);
			allocator_ = std::exchange(other.allocator_, nullptr);
			return *this;
		}

		auto get_handle() const noexcept -> VkInstance { return handle_; }
		explicit operator VkInstance() const noexcept { return handle_; }
		explicit operator uint64_t() const noexcept { return reinterpret_cast<uint64_t>(handle_); }

		auto get_allocator() const noexcept -> VkAllocationCallbacks const* { return allocator_; }

		auto create(const VkApplicationInfo& app_info, VkAllocationCallbacks const* allocator) -> VkResult;

		auto enumerate_devices() const -> Result<std::vector<Device>>;

		auto is_extension_enabled(const char* extension_name) const noexcept -> bool;
		auto is_extension_supported(const char* extension_name) const noexcept -> bool;
		auto try_enable_extension(const char* extension_name) -> bool;

		auto is_layer_enabled(const char* layer_name) const noexcept -> bool;
		auto try_enable_layer(const char* layer_name) -> bool;
	private:
		VkInstance handle_{ VK_NULL_HANDLE };
		VkAllocationCallbacks const* allocator_{ nullptr };

		std::vector<VkExtensionProperties> available_extensions_;
		std::vector<const char*> enabled_extensions_;

		std::vector<VkLayerProperties> available_layers_;
		std::vector<const char*> enabled_layers_;

		std::vector<VkValidationFeatureEnableEXT> enabled_validation_features_;
	};

	class Device {
	public:
		Device(Instance const& instance, VkPhysicalDevice physical);
		~Device();

		Device(const Device& other) = delete;
		Device& operator=(const Device& other) = delete;

		Device(Device&& other) :
			physical_{ std::exchange(other.physical_, VK_NULL_HANDLE) },
			logical_{ std::exchange(other.logical_, VK_NULL_HANDLE) },
			allocator_{ std::exchange(other.allocator_, nullptr) } {
		}

		Device& operator=(Device&& other) {
			physical_ = std::exchange(other.physical_, VK_NULL_HANDLE);
			logical_ = std::exchange(other.logical_, VK_NULL_HANDLE);
			allocator_ = std::exchange(other.allocator_, nullptr);
			return *this;
		}

		auto is_extension_enabled(const char* extension_name) const noexcept -> bool;
		auto is_extension_supported(const char* extension_name) const noexcept -> bool;
		auto try_enable_extension(const char* extension_name) -> bool;

		auto create() -> VkResult;
	private:
		VkPhysicalDevice physical_{ VK_NULL_HANDLE };
		VkDevice logical_{ VK_NULL_HANDLE };
		VkAllocationCallbacks const* allocator_{ nullptr };
	};

	class DebugInterface {
	public:
		virtual ~DebugInterface() = default;

		virtual auto set_name(VkObjectType object_type, uint64_t object_handle, std::string_view name) const -> void = 0;
		virtual auto set_tag(VkObjectType object_type, uint64_t object_handle, uint64_t tag_name, const void* tag_data, size_t tag_data_size) const -> void = 0;
		virtual auto push_label(VkCommandBuffer command_buffer, std::string_view name, std::array<float, 4ull> color) const -> void = 0;
		virtual auto pop_label(VkCommandBuffer command_buffer) const -> void = 0;
		virtual auto insert_label(VkCommandBuffer command_buffer, std::string_view name, std::array<float, 4ull> color) const -> void = 0;
	protected:
		VkDevice device_{ VK_NULL_HANDLE };
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
			allocator_{ std::exchange(other.allocator_, nullptr) },
			handle_{ std::exchange(other.handle_, VK_NULL_HANDLE) } {
		}

		DebugUtils& operator=(DebugUtils&& other) {
			instance_ = std::exchange(other.instance_, VK_NULL_HANDLE);
			allocator_ = std::exchange(other.allocator_, nullptr);
			handle_ = std::exchange(other.handle_, VK_NULL_HANDLE);
			return *this;
		}

		static auto create_unique(Instance const& instance) -> Result<std::unique_ptr<DebugUtils>>;

		auto set_name(VkObjectType object_type, uint64_t object_handle, std::string_view name) const -> void override;
		auto set_tag(VkObjectType object_type, uint64_t object_handle, uint64_t tag_name, const void* tag_data, size_t tag_data_size) const -> void override;
		auto push_label(VkCommandBuffer command_buffer, std::string_view name, std::array<float, 4ull> color) const -> void override;
		auto pop_label(VkCommandBuffer command_buffer) const -> void override;
		auto insert_label(VkCommandBuffer command_buffer, std::string_view name, std::array<float, 4ull> color) const -> void override;
	private:
		auto _create(Instance const& instance) -> VkResult;

		VkInstance instance_{ VK_NULL_HANDLE };
		VkAllocationCallbacks const* allocator_{ nullptr };
		VkDebugUtilsMessengerEXT handle_{ VK_NULL_HANDLE };
	};

	class DebugReport final : public DebugInterface {
	public:
		static constexpr auto object_type{ VK_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT };
		static constexpr std::string_view object_name{ "VkDebugReportCallbackEXT" };

		~DebugReport() override;

		static auto create_unique(Instance const& instance) -> Result< std::unique_ptr<DebugReport>>;

		auto set_name(VkObjectType object_type, uint64_t object_handle, std::string_view name) const -> void override;
		auto set_tag(VkObjectType object_type, uint64_t object_handle, uint64_t tag_name, const void* tag_data, size_t tag_data_size) const -> void override;
		auto push_label(VkCommandBuffer command_buffer, std::string_view name, std::array<float, 4ull> color) const -> void override;
		auto pop_label(VkCommandBuffer command_buffer) const -> void override;
		auto insert_label(VkCommandBuffer command_buffer, std::string_view name, std::array<float, 4ull> color) const -> void override;
	private:
		auto _create(Instance const& instance) -> VkResult;

		VkInstance instance_{ VK_NULL_HANDLE };
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