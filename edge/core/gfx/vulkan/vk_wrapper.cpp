#include "vk_wrapper.h"

#include <atomic>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include <spdlog/spdlog.h>

static bool g_volk_initialized{ false };
static std::atomic<uint64_t> g_volk_ref_counter{ 0ull };

#define VKW_TRY_INIT_VOLK() { if (!g_volk_initialized) { g_volk_initialized = volkInitialize() == VK_SUCCESS; } g_volk_ref_counter.fetch_add(1ull); }
#define VKW_TRY_DEINIT_VOLK() { if (g_volk_initialized && g_volk_ref_counter.fetch_sub(1ull) == 0ull) { volkFinalize(); g_volk_initialized = false; } }

#define VKW_CHECK_RESULT(expr) { auto result = expr; if(result != VK_SUCCESS) return std::unexpected(result); }

#define VKW_LOGI(...) spdlog::info("[{}]: {}", VKW_SCOPE, std::format(__VA_ARGS__))
#define VKW_LOGD(...) spdlog::debug("[{}]: {}", VKW_SCOPE, std::format(__VA_ARGS__))
#define VKW_LOGT(...) spdlog::trace("[{}]: {}", VKW_SCOPE, std::format(__VA_ARGS__))
#define VKW_LOGW(...) spdlog::warn("[{}]: {}", VKW_SCOPE, std::format(__VA_ARGS__))
#define VKW_LOGE(...) spdlog::error("[{}]: {}", VKW_SCOPE, std::format(__VA_ARGS__))

namespace vkw {
#ifdef USE_VALIDATION_LAYERS
	namespace
	{
#define VKW_SCOPE "Vulkan Validation"
		VKAPI_ATTR VkBool32 VKAPI_CALL debug_utils_messenger_callback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity, VkDebugUtilsMessageTypeFlagsEXT message_type, const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data) {
			// Log debug message
			if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
				VKW_LOGT("{} - {}: {}", callback_data->messageIdNumber, callback_data->pMessageIdName, callback_data->pMessage);
			}
			else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
				VKW_LOGI("{} - {}: {}", callback_data->messageIdNumber, callback_data->pMessageIdName, callback_data->pMessage); 
			}
			else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
				VKW_LOGW("{} - {}: {}", callback_data->messageIdNumber, callback_data->pMessageIdName, callback_data->pMessage);
			}
			else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
				VKW_LOGE("{} - {}: {}", callback_data->messageIdNumber, callback_data->pMessageIdName, callback_data->pMessage);
			}
			return VK_FALSE;
		}

		static VKAPI_ATTR VkBool32 VKAPI_CALL debug_report_callback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT /*type*/,
			uint64_t /*object*/, size_t /*location*/, int32_t /*message_code*/,
			const char* layer_prefix, const char* message, void* /*user_data*/) {
			if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
				VKW_LOGE("{}: {}", layer_prefix, message);
			}
			else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) {
				VKW_LOGW("{}: {}", layer_prefix, message);
			}
			else if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT) {
				VKW_LOGW("{}: {}", layer_prefix, message);
			}
			else if (flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT) {
				VKW_LOGD("{}: {}", layer_prefix, message);
			}
			else {
				VKW_LOGI("{}: {}", layer_prefix, message);
			}
			return VK_FALSE;
		}
#undef VKW_SCOPE // Vulkan validation
	}
#endif

#define VKW_SCOPE "vkw::Instance"

	auto enumerate_instance_extension_properties(const char* layer_name, VkAllocationCallbacks const* allocator) -> Result<Vector<VkExtensionProperties>> {
		uint32_t count;
		VKW_CHECK_RESULT(vkEnumerateInstanceExtensionProperties(layer_name, &count, nullptr));

		Vector<VkExtensionProperties> output(count, allocator);
		VKW_CHECK_RESULT(vkEnumerateInstanceExtensionProperties(layer_name, &count, output.data()));

		return output;
	}

	auto enumerate_instance_layer_properties(VkAllocationCallbacks const* allocator) -> Result<Vector<VkLayerProperties>> {
		uint32_t count;
		VKW_CHECK_RESULT(vkEnumerateInstanceLayerProperties(&count, nullptr));

		Vector<VkLayerProperties> output(count, allocator);
		VKW_CHECK_RESULT(vkEnumerateInstanceLayerProperties(&count, output.data()));

		return output;
	}

	auto enumerate_instance_version() -> Result<uint32_t> {
		uint32_t version = VK_VERSION_1_0;
		if (auto result = vkEnumerateInstanceVersion(&version); result != VK_SUCCESS) {
			return std::unexpected(result);
		}

		return version;
	}

	Instance::~Instance() {
		if (handle_) {
			vkDestroyInstance(handle_, allocator_);
		}
	}

	auto Instance::enumerate_physical_devices() const -> Result<Vector<PhysicalDevice>> {
		uint32_t count = 0;
		if (auto result = vkEnumeratePhysicalDevices(handle_, &count, nullptr); result != VK_SUCCESS) {
			return std::unexpected(result);
		}
		Vector<VkPhysicalDevice> devices(count, allocator_);
		if (auto result = vkEnumeratePhysicalDevices(handle_, &count, devices.data()); result != VK_SUCCESS) {
			return std::unexpected(result);
		}

		Vector<PhysicalDevice> output(allocator_);
		for (auto& device : devices) {
			output.emplace_back(device, allocator_);
		}

		return output;
	}

	auto Instance::get_proc_addr(const char* name) const -> PFN_vkVoidFunction {
		return vkGetInstanceProcAddr(handle_, name);
	}

	auto Instance::enumerate_physical_device_groups() const -> Result<Vector<VkPhysicalDeviceGroupProperties>> {
		uint32_t count = 0;
		if (auto result = vkEnumeratePhysicalDeviceGroups(handle_, &count, nullptr); result != VK_SUCCESS) {
			return std::unexpected(result);
		}
		Vector<VkPhysicalDeviceGroupProperties> groups(count, allocator_);
		for (auto& group : groups) {
			group.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GROUP_PROPERTIES;
			group.pNext = nullptr;
		}
		if (auto result = vkEnumeratePhysicalDeviceGroups(handle_, &count, groups.data()); result != VK_SUCCESS) {
			return std::unexpected(result);
		}
		return groups;
	}

	auto Instance::create_debug_utils_messenger_ext(const VkDebugUtilsMessengerCreateInfoEXT& create_info) const -> Result<VkDebugUtilsMessengerEXT> {
		VkDebugUtilsMessengerEXT messenger;
		if (auto result = vkCreateDebugUtilsMessengerEXT(handle_, &create_info, allocator_, &messenger); result != VK_SUCCESS) {
			return std::unexpected(result);
		}
		return messenger;
	}

	auto Instance::destroy_debug_utils_messenger_ext(VkDebugUtilsMessengerEXT messenger) const -> void {
		vkDestroyDebugUtilsMessengerEXT(handle_, messenger, allocator_);
	}

	auto Instance::submit_debug_utils_message_ext(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
		VkDebugUtilsMessageTypeFlagsEXT message_types,
		const VkDebugUtilsMessengerCallbackDataEXT& callback_data) const -> void {
		vkSubmitDebugUtilsMessageEXT(handle_, message_severity, message_types, &callback_data);
	}

	auto Instance::create_debug_report_callback_ext(const VkDebugReportCallbackCreateInfoEXT& create_info) const -> Result<VkDebugReportCallbackEXT> {
		VkDebugReportCallbackEXT callback;
		if (auto result = vkCreateDebugReportCallbackEXT(handle_, &create_info, allocator_, &callback); result != VK_SUCCESS) {
			return std::unexpected(result);
		}
		return callback;
	}

	auto Instance::destroy_debug_report_callback_ext(VkDebugReportCallbackEXT callback) const -> void {
		vkDestroyDebugReportCallbackEXT(handle_, callback, allocator_);
	}

	auto Instance::debug_report_message_ext(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT object_type,
		uint64_t object, size_t location, int32_t message_code,
		const char* layer_prefix, const char* message) const -> void {
		vkDebugReportMessageEXT(handle_, flags, object_type, object, location, message_code, layer_prefix, message);
	}

	auto Instance::destroy_surface_khr(VkSurfaceKHR surface) const -> void {
		vkDestroySurfaceKHR(handle_, surface, allocator_);
	}

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
	auto Instance::create_surface_khr(const VkAndroidSurfaceCreateInfoKHR& create_info) const -> Result<VkSurfaceKHR> {
		VkSurfaceKHR surface;
		if (auto result = vkCreateAndroidSurfaceKHR(handle_, &create_info, allocator_, &surface); result != VK_SUCCESS) {
			return std::unexpected(result);
		}
		return surface;
	}
#elif defined(VK_USE_PLATFORM_WIN32_KHR)
	auto Instance::create_surface_khr(const VkWin32SurfaceCreateInfoKHR& create_info) const -> Result<VkSurfaceKHR> {
		VkSurfaceKHR surface;
		if (auto result = vkCreateWin32SurfaceKHR(handle_, &create_info, allocator_, &surface); result != VK_SUCCESS) {
			return std::unexpected(result);
		}
		return surface;
	}
#endif

#undef VKW_SCOPE // Instance

#define VKW_SCOPE "InstanceBuilder"

	InstanceBuilder::InstanceBuilder(VkAllocationCallbacks const* allocator) :
		allocator_{ allocator } {
		app_info_.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		app_info_.pNext = nullptr;
		app_info_.pApplicationName = nullptr;
		app_info_.applicationVersion = 0;
		app_info_.pEngineName = nullptr;
		app_info_.engineVersion = 0;
		app_info_.apiVersion = VK_API_VERSION_1_0;

		create_info_.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		create_info_.pNext = nullptr;
		create_info_.flags = 0;
		create_info_.pApplicationInfo = &app_info_;
		create_info_.enabledLayerCount = 0;
		create_info_.ppEnabledLayerNames = nullptr;
		create_info_.enabledExtensionCount = 0;
		create_info_.ppEnabledExtensionNames = nullptr;
	}

	auto InstanceBuilder::enable_validation_layers() -> InstanceBuilder& {
		// Try enable validation layers
#ifdef USE_VALIDATION_LAYERS
		add_layer("VK_LAYER_KHRONOS_validation");
#if defined(VKW_VALIDATION_LAYERS_SYNCHRONIZATION)
		add_layer("VK_LAYER_KHRONOS_synchronization2");
#endif // VULKAN_VALIDATION_LAYERS_SYNCHRONIZATION
#endif // USE_VALIDATION_LAYERS
		return *this;
	}

	auto InstanceBuilder::check_extensions_supported() const -> Result<bool> {
		auto available_extensions = enumerate_instance_extension_properties(nullptr, allocator_);
		if (!available_extensions) {
			return std::unexpected(available_extensions.error());
		}

		for (const char* requested_ext : enabled_extensions_) {
			bool found = false;
			for (const auto& available_ext : available_extensions.value()) {
				if (std::strcmp(requested_ext, available_ext.extensionName) == 0) {
					found = true;
					break;
				}
			}
			if (!found) {
				VKW_LOGW("Extension \"{}\" is not supported by instance.", requested_ext);
				return false;
			}
		}
		return true;
	}

	auto InstanceBuilder::check_layers_supported() const -> Result<bool> {
		auto available_layers = enumerate_instance_layer_properties(allocator_);
		if (!available_layers) {
			return std::unexpected(available_layers.error());
		}

		for (const char* requested_layer : enabled_layers_) {
			bool found = false;
			for (const auto& available_layer : available_layers.value()) {
				if (std::strcmp(requested_layer, available_layer.layerName) == 0) {
					found = true;
					break;
				}
			}
			if (!found) {
				VKW_LOGW("Layer \"{}\" is not supported by instance.", requested_layer);
				return false;
			}
		}
		return true;
	}

	auto InstanceBuilder::build() -> Result<Instance> {
		// Check extension support
		if (auto result = check_extensions_supported(); result.has_value() && !result.value()) {
			return std::unexpected(VK_ERROR_EXTENSION_NOT_PRESENT);
		}

		if (auto result = check_layers_supported(); result.has_value() && !result.value()) {
			return std::unexpected(VK_ERROR_LAYER_NOT_PRESENT);
		}


#ifdef USE_VALIDATION_LAYER_FEATURES
		bool validation_features{ false };
		{
			if (auto request = enumerate_instance_extension_properties("VK_LAYER_KHRONOS_validation"); request.has_value()) {
				for (auto& available_extension : *request) {
					if (strcmp(available_extension.extensionName, VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME) == 0) {
						validation_features = true;
						enabled_extensions_.push_back(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);
					}
				}
			}
		}

		VkValidationFeaturesEXT validation_features_info{ VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT };
		Vector<VkValidationFeatureEnableEXT> validation_feature_lists(allocator_);
		validation_feature_lists.push_back(VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT);

		if (validation_features) {
			VKW_LOGD("Enabling validation features");
#if defined(VKW_VALIDATION_LAYERS_GPU_ASSISTED)
			validation_feature_lists.push_back(VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_RESERVE_BINDING_SLOT_EXT);
			validation_feature_lists.push_back(VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT);
#endif
#if defined(VKW_VALIDATION_LAYERS_BEST_PRACTICES)
			validation_feature_lists.push_back(VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT);
#endif
#if defined(VKW_VALIDATION_LAYERS_SYNCHRONIZATION)
			validation_feature_lists.push_back(VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT);
#endif
			validation_features_info.pEnabledValidationFeatures = validation_feature_lists.data();
			validation_features_info.enabledValidationFeatureCount = static_cast<uint32_t>(validation_feature_lists.size());
			validation_features_info.pNext = create_info_.pNext;
			create_info_.pNext = &validation_features_info;
		}
#endif

		update_create_info();

#if defined(VKW_ENABLE_PORTABILITY)
		create_info_.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

		VkInstance instance_handle;
		if (auto result = vkCreateInstance(&create_info_, allocator_, &instance_handle); result != VK_SUCCESS) {
			return std::unexpected(result);
		}

		volkLoadInstance(instance_handle);

		return Instance{ instance_handle, allocator_ };
	}

#undef VKW_SCOPE // InstanceBuilder

#define VKW_SCOPE "PhysicalDevice"
	auto PhysicalDevice::get_features() const -> VkPhysicalDeviceFeatures {
		VkPhysicalDeviceFeatures features{};
		vkGetPhysicalDeviceFeatures(handle_, &features);
		return features;
	}

	auto PhysicalDevice::get_format_properties(VkFormat format) const -> VkFormatProperties {
		VkFormatProperties props{};
		vkGetPhysicalDeviceFormatProperties(handle_, format, &props);
		return props;
	}

	auto PhysicalDevice::get_image_format_properties(VkFormat format, VkImageType type, VkImageTiling tiling,
		VkImageUsageFlags usage, VkImageCreateFlags flags) const -> Result<VkImageFormatProperties> {
		VkImageFormatProperties props{};
		if (auto result = vkGetPhysicalDeviceImageFormatProperties(handle_, format, type, tiling, usage, flags, &props); result != VK_SUCCESS) {
			return std::unexpected(result);
		}
		return props;
	}

	auto PhysicalDevice::get_queue_family_properties() const -> Vector<VkQueueFamilyProperties> {
		uint32_t count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(handle_, &count, nullptr);
		Vector<VkQueueFamilyProperties> output(count, allocator_);
		vkGetPhysicalDeviceQueueFamilyProperties(handle_, &count, output.data());
		return output;
	}

	auto PhysicalDevice::get_memory_properties() const -> VkPhysicalDeviceMemoryProperties {
		VkPhysicalDeviceMemoryProperties props{};
		vkGetPhysicalDeviceMemoryProperties(handle_, &props);
		return props;
	}

	auto PhysicalDevice::get_properties() const -> VkPhysicalDeviceProperties {
		VkPhysicalDeviceProperties props{};
		vkGetPhysicalDeviceProperties(handle_, &props);
		return props;
	}

	auto PhysicalDevice::get_sparse_image_format_properties(VkFormat format, VkImageType type, VkSampleCountFlagBits samples,
		VkImageUsageFlags usage, VkImageTiling tiling) const -> Vector<VkSparseImageFormatProperties> {
		uint32_t count = 0;
		vkGetPhysicalDeviceSparseImageFormatProperties(handle_, format, type, samples, usage, tiling, &count, nullptr);
		Vector<VkSparseImageFormatProperties> output(count, allocator_);
		vkGetPhysicalDeviceSparseImageFormatProperties(handle_, format, type, samples, usage, tiling, &count, output.data());
		return output;
	}

	auto PhysicalDevice::get_features2() const -> VkPhysicalDeviceFeatures2 {
		VkPhysicalDeviceFeatures2 result{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
		vkGetPhysicalDeviceFeatures2(handle_, &result);
		return result;
	}

	auto PhysicalDevice::get_properties2() const -> VkPhysicalDeviceProperties2 {
		VkPhysicalDeviceProperties2 result{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
		vkGetPhysicalDeviceProperties2(handle_, &result);
		return result;
	}

	auto PhysicalDevice::get_format_properties2(VkFormat format) const -> VkFormatProperties2 {
		VkFormatProperties2 result{ VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2 };
		vkGetPhysicalDeviceFormatProperties2(handle_, format, &result);
		return result;
	}

	auto PhysicalDevice::get_image_format_properties2(const VkPhysicalDeviceImageFormatInfo2& image_format_info) const -> Result<VkImageFormatProperties2> {
		VkImageFormatProperties2 output{ VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2 };
		if (auto result = vkGetPhysicalDeviceImageFormatProperties2(handle_, &image_format_info, &output); result != VK_SUCCESS) {
			return std::unexpected(result);
		}
		return output;
	}

	auto PhysicalDevice::get_queue_family_properties2() const -> Vector<VkQueueFamilyProperties2> {
		uint32_t count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties2(handle_, &count, nullptr);
		Vector<VkQueueFamilyProperties2> output(count, allocator_);
		for (auto& props : output) {
			props.sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
			props.pNext = nullptr;
		}
		vkGetPhysicalDeviceQueueFamilyProperties2(handle_, &count, output.data());
		return output;
	}

	auto PhysicalDevice::get_memory_properties2() const -> VkPhysicalDeviceMemoryProperties2 {
		VkPhysicalDeviceMemoryProperties2 props{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2 };
		vkGetPhysicalDeviceMemoryProperties2(handle_, &props);
		return props;
	}

	auto PhysicalDevice::get_sparse_image_format_properties2(const VkPhysicalDeviceSparseImageFormatInfo2& format_info) const 
		-> Vector<VkSparseImageFormatProperties2> {
		uint32_t count = 0;
		vkGetPhysicalDeviceSparseImageFormatProperties2(handle_, &format_info, &count, nullptr);
		Vector<VkSparseImageFormatProperties2> output(count, allocator_);
		for (auto& props : output) {
			props.sType = VK_STRUCTURE_TYPE_SPARSE_IMAGE_FORMAT_PROPERTIES_2;
			props.pNext = nullptr;
		}
		vkGetPhysicalDeviceSparseImageFormatProperties2(handle_, &format_info, &count, output.data());
		return output;
	}

	auto PhysicalDevice::enumerate_device_extension_properties(const char* layer_name) const -> Result<Vector<VkExtensionProperties>> {
		uint32_t count = 0;
		if (auto result = vkEnumerateDeviceExtensionProperties(handle_, layer_name, &count, nullptr); result != VK_SUCCESS) {
			return std::unexpected(result);
		}
		Vector<VkExtensionProperties> extensions(count, allocator_);
		if (auto result = vkEnumerateDeviceExtensionProperties(handle_, layer_name, &count, extensions.data()); result != VK_SUCCESS) {
			return std::unexpected(result);
		}
		return extensions;
	}

	auto PhysicalDevice::enumerate_device_layer_properties() const -> Result<Vector<VkLayerProperties>> {
		uint32_t count = 0;
		if (auto result = vkEnumerateDeviceLayerProperties(handle_, &count, nullptr); result != VK_SUCCESS) {
			return std::unexpected(result);
		}
		Vector<VkLayerProperties> layers(count, allocator_);
		if (auto result = vkEnumerateDeviceLayerProperties(handle_, &count, layers.data()); result != VK_SUCCESS) {
			return std::unexpected(result);
		}
		return layers;
	}

	auto PhysicalDevice::get_surface_support_khr(uint32_t queue_family_index, VkSurfaceKHR surface) const -> Result<VkBool32> {
		VkBool32 supported = VK_FALSE;
		if (auto result = vkGetPhysicalDeviceSurfaceSupportKHR(handle_, queue_family_index, surface, &supported); result != VK_SUCCESS) {
			return std::unexpected(result);
		}
		return supported;
	}

	auto PhysicalDevice::get_surface_capabilities_khr(VkSurfaceKHR surface) const -> Result<VkSurfaceCapabilitiesKHR> {
		VkSurfaceCapabilitiesKHR capabilities{};
		if (auto result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(handle_, surface, &capabilities); result != VK_SUCCESS) {
			return std::unexpected(result);
		}
		return capabilities;
	}

	auto PhysicalDevice::get_surface_formats_khr(VkSurfaceKHR surface) const -> Result<Vector<VkSurfaceFormatKHR>> {
		uint32_t count = 0;
		if (auto result = vkGetPhysicalDeviceSurfaceFormatsKHR(handle_, surface, &count, nullptr); result != VK_SUCCESS) {
			return std::unexpected(result);
		}
		Vector<VkSurfaceFormatKHR> formats(count, allocator_);
		if (auto result = vkGetPhysicalDeviceSurfaceFormatsKHR(handle_, surface, &count, formats.data()); result != VK_SUCCESS) {
			return std::unexpected(result);
		}
		return formats;
	}

	auto PhysicalDevice::get_surface_present_modes_khr(VkSurfaceKHR surface) const -> Result<Vector<VkPresentModeKHR>> {
		uint32_t count = 0;
		if (auto result = vkGetPhysicalDeviceSurfacePresentModesKHR(handle_, surface, &count, nullptr); result != VK_SUCCESS) {
			return std::unexpected(result);
		}
		Vector<VkPresentModeKHR> modes(count, allocator_);
		if (auto result = vkGetPhysicalDeviceSurfacePresentModesKHR(handle_, surface, &count, modes.data()); result != VK_SUCCESS) {
			return std::unexpected(result);
		}
		return modes;
	}

	auto PhysicalDevice::get_display_properties_khr() const -> Result<Vector<VkDisplayPropertiesKHR>> {
		uint32_t count = 0;
		if (auto result = vkGetPhysicalDeviceDisplayPropertiesKHR(handle_, &count, nullptr); result != VK_SUCCESS) {
			return std::unexpected(result);
		}
		Vector<VkDisplayPropertiesKHR> properties(count, allocator_);
		if (auto result = vkGetPhysicalDeviceDisplayPropertiesKHR(handle_, &count, properties.data()); result != VK_SUCCESS) {
			return std::unexpected(result);
		}
		return properties;
	}

	auto PhysicalDevice::get_display_plane_properties_khr() const -> Result<Vector<VkDisplayPlanePropertiesKHR>> {
		uint32_t count = 0;
		if (auto result = vkGetPhysicalDeviceDisplayPlanePropertiesKHR(handle_, &count, nullptr); result != VK_SUCCESS) {
			return std::unexpected(result);
		}
		Vector<VkDisplayPlanePropertiesKHR> properties(count, allocator_);
		if (auto result = vkGetPhysicalDeviceDisplayPlanePropertiesKHR(handle_, &count, properties.data()); result != VK_SUCCESS) {
			return std::unexpected(result);
		}
		return properties;
	}

	auto PhysicalDevice::get_display_planes_supported_displays_khr(uint32_t plane_index) const -> Result<Vector<VkDisplayKHR>> {
		uint32_t count = 0;
		if (auto result = vkGetDisplayPlaneSupportedDisplaysKHR(handle_, plane_index, &count, nullptr); result != VK_SUCCESS) {
			return std::unexpected(result);
		}
		Vector<VkDisplayKHR> displays(count, allocator_);
		if (auto result = vkGetDisplayPlaneSupportedDisplaysKHR(handle_, plane_index, &count, displays.data()); result != VK_SUCCESS) {
			return std::unexpected(result);
		}
		return displays;
	}

	auto PhysicalDevice::get_tool_properties() const -> Result<Vector<VkPhysicalDeviceToolProperties>> {
		uint32_t count = 0;
		if (auto result = vkGetPhysicalDeviceToolProperties(handle_, &count, nullptr); result != VK_SUCCESS) {
			return std::unexpected(result);
		}
		Vector<VkPhysicalDeviceToolProperties> tools(count, allocator_);
		for (auto& tool : tools) {
			tool.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TOOL_PROPERTIES;
			tool.pNext = nullptr;
		}
		if (auto result = vkGetPhysicalDeviceToolProperties(handle_, &count, tools.data()); result != VK_SUCCESS) {
			return std::unexpected(result);
		}
		return tools;
	}

	auto PhysicalDevice::get_external_buffer_properties(const VkPhysicalDeviceExternalBufferInfo& external_buffer_info) const -> VkExternalBufferProperties {
		VkExternalBufferProperties properties{ VK_STRUCTURE_TYPE_EXTERNAL_BUFFER_PROPERTIES };
		vkGetPhysicalDeviceExternalBufferProperties(handle_, &external_buffer_info, &properties);
		return properties;
	}

	auto PhysicalDevice::get_external_fence_properties(const VkPhysicalDeviceExternalFenceInfo& external_fence_info) const -> VkExternalFenceProperties {
		VkExternalFenceProperties properties{ VK_STRUCTURE_TYPE_EXTERNAL_FENCE_PROPERTIES };
		vkGetPhysicalDeviceExternalFenceProperties(handle_, &external_fence_info, &properties);
		return properties;
	}

	auto PhysicalDevice::get_external_semaphore_properties(const VkPhysicalDeviceExternalSemaphoreInfo& external_semaphore_info) const -> VkExternalSemaphoreProperties {
		VkExternalSemaphoreProperties properties{ VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES };
		vkGetPhysicalDeviceExternalSemaphoreProperties(handle_, &external_semaphore_info, &properties);
		return properties;
	}

	auto PhysicalDevice::get_multisample_properties_ext(VkSampleCountFlagBits samples) const -> VkMultisamplePropertiesEXT {
		VkMultisamplePropertiesEXT properties{ VK_STRUCTURE_TYPE_MULTISAMPLE_PROPERTIES_EXT };
		vkGetPhysicalDeviceMultisamplePropertiesEXT(handle_, samples, &properties);
		return properties;
	}

	auto PhysicalDevice::get_calibrateable_time_domains_ext() const -> Result<Vector<VkTimeDomainEXT>> {
		uint32_t count = 0;
		if (auto result = vkGetPhysicalDeviceCalibrateableTimeDomainsEXT(handle_, &count, nullptr); result != VK_SUCCESS) {
			return std::unexpected(result);
		}
		Vector<VkTimeDomainEXT> domains(count, allocator_);
		if (auto result = vkGetPhysicalDeviceCalibrateableTimeDomainsEXT(handle_, &count, domains.data()); result != VK_SUCCESS) {
			return std::unexpected(result);
		}
		return domains;
	}

	auto PhysicalDevice::get_fragment_shading_rates_khr() const -> Result<Vector<VkPhysicalDeviceFragmentShadingRateKHR>> {
		uint32_t count = 0;
		if (auto result = vkGetPhysicalDeviceFragmentShadingRatesKHR(handle_, &count, nullptr); result != VK_SUCCESS) {
			return std::unexpected(result);
		}
		Vector<VkPhysicalDeviceFragmentShadingRateKHR> rates(count, allocator_);
		for (auto& rate : rates) {
			rate.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_KHR;
			rate.pNext = nullptr;
		}
		if (auto result = vkGetPhysicalDeviceFragmentShadingRatesKHR(handle_, &count, rates.data()); result != VK_SUCCESS) {
			return std::unexpected(result);
		}
		return rates;
	}

	auto PhysicalDevice::get_cooperative_matrix_properties_nv() const -> Result<Vector<VkCooperativeMatrixPropertiesNV>> {
		uint32_t count = 0;
		if (auto result = vkGetPhysicalDeviceCooperativeMatrixPropertiesNV(handle_, &count, nullptr); result != VK_SUCCESS) {
			return std::unexpected(result);
		}
		Vector<VkCooperativeMatrixPropertiesNV> properties(count, allocator_);
		for (auto& prop : properties) {
			prop.sType = VK_STRUCTURE_TYPE_COOPERATIVE_MATRIX_PROPERTIES_NV;
			prop.pNext = nullptr;
		}
		if (auto result = vkGetPhysicalDeviceCooperativeMatrixPropertiesNV(handle_, &count, properties.data()); result != VK_SUCCESS) {
			return std::unexpected(result);
		}
		return properties;
	}

	auto PhysicalDevice::get_supported_framebuffer_mixed_samples_combinations_nv() const -> Result<std::vector<VkFramebufferMixedSamplesCombinationNV>> {
		uint32_t count = 0;
		if (auto result = vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV(handle_, &count, nullptr); result != VK_SUCCESS) {
			return std::unexpected(result);
		}
		std::vector<VkFramebufferMixedSamplesCombinationNV> combinations(count);
		for (auto& combo : combinations) {
			combo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_MIXED_SAMPLES_COMBINATION_NV;
			combo.pNext = nullptr;
		}
		if (auto result = vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV(handle_, &count, combinations.data()); result != VK_SUCCESS) {
			return std::unexpected(result);
		}
		return combinations;
	}

#undef VKW_SCOPE // PhysicalDevice

#define VKW_SCOPE "Device"
	Device::Device(Instance const& instance, VkPhysicalDevice physical) {

		physical_ = physical;
		allocator_ = instance.get_allocator();

		VkPhysicalDeviceFeatures2 features2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
		features2.pNext = &device_features_11_;
		device_features_11_.pNext = &device_features_12_;

		vkGetPhysicalDeviceFeatures2(physical_, &features2);

		// Copy base device features
		memcpy(&device_features_, &features2.features, sizeof(device_features_));

		vkGetPhysicalDeviceProperties(physical_, &device_properties_);
		vkGetPhysicalDeviceMemoryProperties(physical_, &device_memory_properties_);

		uint32_t family_count{ 0u };
		vkGetPhysicalDeviceQueueFamilyProperties(physical_, &family_count, nullptr);

		queue_family_properties_.resize(family_count);
		vkGetPhysicalDeviceQueueFamilyProperties(physical_, &family_count, queue_family_properties_.data());

		uint32_t extension_property_count_{ 0u };
		vkEnumerateDeviceExtensionProperties(physical_, nullptr, &extension_property_count_, nullptr);

		available_extensions_.resize(extension_property_count_);
		vkEnumerateDeviceExtensionProperties(physical_, nullptr, &extension_property_count_, available_extensions_.data());

		VKW_LOGD("{} device \"{}\" with Vulkan {}.{}.{} support found.",
			to_string(device_properties_.deviceType),
			device_properties_.deviceName,
			VK_VERSION_MAJOR(device_properties_.apiVersion),
			VK_VERSION_MINOR(device_properties_.apiVersion),
			VK_VERSION_PATCH(device_properties_.apiVersion));
	}

	Device::~Device() {
		if (logical_) {
			vkDestroyDevice(logical_, allocator_);
		}
	}

	auto Device::is_extension_enabled(const char* extension_name) const noexcept -> bool {
		return std::find_if(enabled_extensions_.begin(), enabled_extensions_.end(),
			[extension_name](auto& instance_extension) {
				return std::strcmp(instance_extension, extension_name) == 0;
			}) != enabled_extensions_.end();
	}

	auto Device::is_extension_supported(const char* extension_name) const noexcept -> bool {
		return std::find_if(available_extensions_.begin(), available_extensions_.end(),
			[extension_name](auto& extension_props) {
				return std::strcmp(extension_props.extensionName, extension_name) == 0;
			}) != available_extensions_.end();
	}

	auto Device::try_enable_extension(const char* extension_name) -> bool {
		for (const auto& extension_props : available_extensions_) {
			if (strcmp(extension_props.extensionName, extension_name) == 0) {
				if (is_extension_enabled(extension_name)) {
					VKW_LOGW("Extension \"{}\" is already enabled.", extension_name);
				}
				else {
					VKW_LOGD("Extension \"{}\" is supported and enabled.", extension_name);
					enabled_extensions_.push_back(extension_name);
				}

				return true;
			}
		}

		VKW_LOGE("Extension \"{}\" is not supported and can't be enabled.", extension_name);
		return false;
	}

	auto Device::check_api_version(uint32_t version) const noexcept -> bool {
		return device_properties_.apiVersion >= version;
	}

	auto Device::get_name() const noexcept -> std::string_view {
		return device_properties_.deviceName;
	}
#undef VKW_SCOPE // Device

#define VKW_SCOPE "DebugUtils"
	DebugUtils::~DebugUtils() {
		if (handle_) {
			vkDestroyDebugUtilsMessengerEXT(instance_, handle_, allocator_);
		}
	}

	auto DebugUtils::create_unique(Instance const& instance) -> Result<std::unique_ptr<DebugUtils>> {
		auto self = std::make_unique<DebugUtils>();
		if (auto result = self->_create(instance); result != VK_SUCCESS) {
			return std::unexpected(result);
		}
		return self;
	}

	auto DebugUtils::set_name(VkObjectType object_type, uint64_t object_handle, std::string_view name) const -> void {
		VkDebugUtilsObjectNameInfoEXT object_name_info{ VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
		object_name_info.objectType = object_type;
		object_name_info.objectHandle = object_handle;
		object_name_info.pObjectName = name.data();
		auto result = vkSetDebugUtilsObjectNameEXT(device_, &object_name_info);
		if (result != VK_SUCCESS) {
			VKW_LOGW("Failed to set name \"{}\" for object handle {:#x} with type \"{}\". Reason: {}.", 
				name, object_handle, to_string(object_type), to_string(result));
		}
	}

	auto DebugUtils::set_tag(VkObjectType object_type, uint64_t object_handle, uint64_t tag_name, const void* tag_data, size_t tag_data_size) const -> void {
		VkDebugUtilsObjectTagInfoEXT object_tag_info{ VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_TAG_INFO_EXT };
		object_tag_info.objectHandle = object_handle;
		object_tag_info.tagName = tag_name;
		object_tag_info.pTag = tag_data;
		object_tag_info.tagSize = tag_data_size;
		auto result = vkSetDebugUtilsObjectTagEXT(device_, &object_tag_info);
		if (result != VK_SUCCESS) {
			VKW_LOGW("Failed to set tag {:#x} with data {:#x} for object handle {:#x} with type \"{}\". Reason: {}.",
				tag_name, reinterpret_cast<uintptr_t>(tag_data), object_handle, to_string(object_type), to_string(result));
		}
	}

	auto DebugUtils::push_label(VkCommandBuffer command_buffer, std::string_view name, std::array<float, 4ull> color) const -> void {
		VkDebugUtilsLabelEXT debug_label{ VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT };
		debug_label.pLabelName = name.data();
		memcpy(debug_label.color, color.data(), sizeof(float) * color.size());
		vkCmdBeginDebugUtilsLabelEXT(command_buffer, &debug_label);
	}

	auto DebugUtils::pop_label(VkCommandBuffer command_buffer) const -> void {
		vkCmdEndDebugUtilsLabelEXT(command_buffer);
	}

	auto DebugUtils::insert_label(VkCommandBuffer command_buffer, std::string_view name, std::array<float, 4ull> color) const -> void {
		VkDebugUtilsLabelEXT debug_label{ VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT };
		debug_label.pLabelName = name.data();
		memcpy(debug_label.color, color.data(), sizeof(float) * color.size());
		vkCmdInsertDebugUtilsLabelEXT(command_buffer, &debug_label);
	}

	auto DebugUtils::_create(Instance const& instance) -> VkResult {
		instance_ = instance.get_handle();
		allocator_ = instance.get_allocator();

		VkDebugUtilsMessengerCreateInfoEXT create_info{ VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
		create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
		create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		create_info.pfnUserCallback = debug_utils_messenger_callback;

		return vkCreateDebugUtilsMessengerEXT(instance_, &create_info, allocator_, &handle_);
	}
#undef VKW_SCOPE // DebugUtils

#define VKW_SCOPE "DebugReport"
	DebugReport::~DebugReport() {
		if (handle_) {
			vkDestroyDebugReportCallbackEXT(instance_, handle_, allocator_);
		}
	}

	auto DebugReport::create_unique(Instance const& instance) -> Result< std::unique_ptr<DebugReport>> {
		auto self = std::make_unique<DebugReport>();
		if (auto result = self->_create(instance); result != VK_SUCCESS) {
			return std::unexpected(result);
		}
		return self;
	}

	auto DebugReport::set_name(VkObjectType object_type, uint64_t object_handle, std::string_view name) const -> void {
		VkDebugMarkerObjectNameInfoEXT object_name_info{ VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT };
		object_name_info.objectType = (VkDebugReportObjectTypeEXT)object_type;
		object_name_info.object = object_handle;
		object_name_info.pObjectName = name.data();
		auto result = vkDebugMarkerSetObjectNameEXT(device_, &object_name_info);
		if (result != VK_SUCCESS) {
			VKW_LOGW("Failed to set name \"{}\" for object handle {:#x} with type \"{}\". Reason: {}.",
				name, object_handle, to_string(object_type), to_string(result));
		}
	}

	auto DebugReport::set_tag(VkObjectType object_type, uint64_t object_handle, uint64_t tag_name, const void* tag_data, size_t tag_data_size) const -> void {
		VkDebugMarkerObjectTagInfoEXT object_tag_info{ VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_TAG_INFO_EXT };
		object_tag_info.objectType = (VkDebugReportObjectTypeEXT)object_type;
		object_tag_info.object = object_handle;
		object_tag_info.tagName = tag_name;
		object_tag_info.pTag = tag_data;
		object_tag_info.tagSize = tag_data_size;
		auto result = vkDebugMarkerSetObjectTagEXT(device_, &object_tag_info);
		if (result != VK_SUCCESS) {
			VKW_LOGW("Failed to set tag {:#x} with data {:#x} for object handle {:#x} with type \"{}\". Reason: {}.",
				tag_name, reinterpret_cast<uintptr_t>(tag_data), object_handle, to_string(object_type), to_string(result));
		}
	}

	auto DebugReport::push_label(VkCommandBuffer command_buffer, std::string_view name, std::array<float, 4ull> color) const -> void {
		VkDebugMarkerMarkerInfoEXT debug_marker{ VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT };
		debug_marker.pMarkerName = name.data();
		memcpy(debug_marker.color, color.data(), sizeof(float) * color.size());
		vkCmdDebugMarkerBeginEXT(command_buffer, &debug_marker);
	}

	auto DebugReport::pop_label(VkCommandBuffer command_buffer) const -> void {
		vkCmdDebugMarkerEndEXT(command_buffer);
	}

	auto DebugReport::insert_label(VkCommandBuffer command_buffer, std::string_view name, std::array<float, 4ull> color) const -> void {
		VkDebugMarkerMarkerInfoEXT debug_marker{ VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT };
		debug_marker.pMarkerName = name.data();
		memcpy(debug_marker.color, color.data(), sizeof(float) * color.size());
		vkCmdDebugMarkerInsertEXT(command_buffer, &debug_marker);
	}

	auto DebugReport::_create(Instance const& instance) -> VkResult {
		instance_ = instance.get_handle();
		allocator_ = instance.get_allocator();

		VkDebugReportCallbackCreateInfoEXT create_info{ VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT };
		create_info.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
		create_info.pfnCallback = debug_report_callback;

		return vkCreateDebugReportCallbackEXT(instance_, &create_info, allocator_, &handle_);
	}

#undef VKW_SCOPE // DebugReport

	// DebugNone
	auto DebugNone::set_name(VkObjectType object_type, uint64_t object_handle, std::string_view name) const -> void {
		(void)object_type;
		(void)object_handle;
		(void)name;
	}

	auto DebugNone::set_tag(VkObjectType object_type, uint64_t object_handle, uint64_t tag_name, const void* tag_data, size_t tag_data_size) const -> void {
		(void)object_type;
		(void)object_handle;
		(void)tag_name;
		(void)tag_data;
		(void)tag_data_size;
	}

	auto DebugNone::push_label(VkCommandBuffer command_buffer, std::string_view name, std::array<float, 4ull> color) const -> void {
		(void)command_buffer;
		(void)name;
		(void)color;
	}

	auto DebugNone::pop_label(VkCommandBuffer command_buffer) const -> void {
		(void)command_buffer;
	}

	auto DebugNone::insert_label(VkCommandBuffer command_buffer, std::string_view name, std::array<float, 4ull> color) const -> void {
		(void)command_buffer;
		(void)name;
		(void)color;
	}
}