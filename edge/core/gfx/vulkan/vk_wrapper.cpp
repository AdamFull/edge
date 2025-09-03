#include "vk_wrapper.h"

#include <atomic>
#include <ranges>

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
		allocator_{ allocator },
		enabled_extensions_{ allocator },
		enabled_layers_{ allocator },
		validation_feature_enables_{ allocator },
		validation_feature_disables_{ allocator } {

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

	auto InstanceBuilder::build() -> Result<Instance> {
		// Request all validation layers supported
		Vector<VkLayerProperties> all_layer_properties{ allocator_ };
		if (auto request = enumerate_instance_layer_properties(allocator_); request.has_value()) {
			all_layer_properties = std::move(request.value());
		}

		auto check_layer_support = [&all_layer_properties](const char* layer_name) {
			return std::find_if(all_layer_properties.begin(), all_layer_properties.end(),
				[layer_name](const VkLayerProperties& props) {
					return std::strcmp(props.layerName, layer_name) == 0;
				}) != all_layer_properties.end();
			};

		// Check enabled layers
		for (auto it = enabled_layers_.begin(); it != enabled_layers_.end();) {
			if (!check_layer_support(*it)) {
				VKW_LOGW("Unsupported layer \"{}\" removed.", *it);
				it = enabled_extensions_.erase(it);
				continue;
			}

			++it;
		}

		// Request all supported extensions including validation layers
		Vector<VkExtensionProperties> all_extension_properties{ allocator_ };
		for (int32_t layer_index = 0; layer_index < static_cast<int32_t>(enabled_layers_.size() + 1); ++layer_index) {
			const char* layer_name = (layer_index == 0 ? nullptr : enabled_layers_[layer_index - 1]);
			if (auto request = enumerate_instance_extension_properties(layer_name, allocator_); request.has_value()) {
				auto layer_ext_props = std::move(request.value());
				all_extension_properties.insert(all_extension_properties.end(), layer_ext_props.begin(), layer_ext_props.end());
			}
		}

		// Helper function
		auto check_ext_support = [&all_extension_properties](const char* extension_name) {
			return std::find_if(all_extension_properties.begin(), all_extension_properties.end(),
				[extension_name](const VkExtensionProperties& props) {
					return std::strcmp(props.extensionName, extension_name) == 0;
				}) != all_extension_properties.end();
			};

		// Check enabled extensions
		for (auto it = enabled_extensions_.begin(); it != enabled_extensions_.end();) {
			if (!check_ext_support(*it)) {
				VKW_LOGW("Unsupported extension \"{}\" removed.", *it);
				it = enabled_extensions_.erase(it);
				continue;
			}

			++it;
		}

#define TRY_ENABLE_EXTENSION(ext) if(!check_ext_support(ext)) { VKW_LOGE("Extension \"{}\" not supported.", ext); return std::unexpected(VK_ERROR_EXTENSION_NOT_PRESENT); } add_extension(ext);

		// Surface
		if (enable_surface_) {
			TRY_ENABLE_EXTENSION(VK_KHR_SURFACE_EXTENSION_NAME);
#if defined(VK_USE_PLATFORM_WIN32_KHR)
			TRY_ENABLE_EXTENSION(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
			TRY_ENABLE_EXTENSION(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_XCB_KHR)
			TRY_ENABLE_EXTENSION(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
			TRY_ENABLE_EXTENSION(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
			TRY_ENABLE_EXTENSION(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_MACOS_MVK)
			TRY_ENABLE_EXTENSION(VK_MVK_MACOS_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_IOS_MVK)
			TRY_ENABLE_EXTENSION(VK_MVK_IOS_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_METAL_EXT)
			TRY_ENABLE_EXTENSION(VK_EXT_METAL_SURFACE_EXTENSION_NAME);
#else
#error "Unsipported platform"
#endif
		}
		else {
			TRY_ENABLE_EXTENSION(VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME);
		}

		// Debug utils
		if (enable_debug_utils_) {
			TRY_ENABLE_EXTENSION(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

		// Portability
		if (enable_portability_) {
			TRY_ENABLE_EXTENSION(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
			add_flag(VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR);
		}

#undef TRY_ENABLE_EXTENSION

		// Enable validation features if possible
		VkValidationFeaturesEXT validation_features_info{ VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT };
		if ((!validation_feature_enables_.empty() || !validation_feature_disables_.empty()) && check_ext_support(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME)) {
			add_extension(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);

			validation_features_info.enabledValidationFeatureCount = static_cast<uint32_t>(validation_feature_enables_.size());
			validation_features_info.pEnabledValidationFeatures = validation_feature_enables_.data();
			validation_features_info.disabledValidationFeatureCount = static_cast<uint32_t>(validation_feature_disables_.size());
			validation_features_info.pDisabledValidationFeatures = validation_feature_disables_.data();
			validation_features_info.pNext = create_info_.pNext;
			create_info_.pNext = &validation_features_info;
		}

		create_info_.enabledExtensionCount = static_cast<uint32_t>(enabled_extensions_.size());
		create_info_.ppEnabledExtensionNames = enabled_extensions_.empty() ? nullptr : enabled_extensions_.data();
		create_info_.enabledLayerCount = static_cast<uint32_t>(enabled_layers_.size());
		create_info_.ppEnabledLayerNames = enabled_layers_.empty() ? nullptr : enabled_layers_.data();

		VkInstance instance_handle;
		if (auto result = vkCreateInstance(&create_info_, allocator_, &instance_handle); result != VK_SUCCESS) {
			return std::unexpected(result);
		}

		volkLoadInstance(instance_handle);

		return Instance{ instance_handle, allocator_ };
	}

#undef VKW_SCOPE // InstanceBuilder

#define VKW_SCOPE "PhysicalDevice"
	auto PhysicalDevice::create_device(const VkDeviceCreateInfo& create_info) const -> Result<Device> {
		VkDevice device;
		if (auto result = vkCreateDevice(handle_, &create_info, allocator_, &device); result != VK_SUCCESS) {
			return std::unexpected(result);
		}

		volkLoadDevice(device);

		return Device{ device, allocator_ };
	}

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

	auto PhysicalDevice::get_features2(void* chain) const -> VkPhysicalDeviceFeatures2 {
		VkPhysicalDeviceFeatures2 result{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
		result.pNext = chain;
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
	Device::~Device() {
		if (handle_) {
			vkDestroyDevice(handle_, allocator_);
		}
	}
#undef VKW_SCOPE // Device

#define VKW_SCOPE "DeviceSelector"

	DeviceSelector::DeviceSelector(Instance const& instance) :
		allocator_{ instance.get_allocator() },
		requested_extensions_{ allocator_ },
		requested_features_{ allocator_ } {

		if (auto result = instance.enumerate_physical_devices(); result.has_value()) {
			physical_devices_ = std::move(result.value());
		}
	}

	auto DeviceSelector::select() -> Result<Device> {
		int32_t best_candidate_index{ -1 };
		int32_t fallback_index{ -1 };

		Vector<Vector<const char*>> per_device_extensions(physical_devices_.size(), Vector<const char*>(allocator_), allocator_);
		for (int32_t device_idx = 0; device_idx < static_cast<int32_t>(physical_devices_.size()); ++device_idx) {
			auto& physical_device = physical_devices_[device_idx];

			auto properties = physical_device.get_properties();

			Vector<VkExtensionProperties> available_extensions{ allocator_ };
			if (auto result = physical_device.enumerate_device_extension_properties(); result.has_value()) {
				available_extensions = std::move(result.value());
			}

			// No extensions found. Bug?
			if (available_extensions.empty()) {
				VKW_LOGE("Device \"{}\" have no supported extensions. Check driver.", properties.deviceName);
				continue;
			}

			auto check_ext_support = [&available_extensions](const char* extension_name) {
				return std::find_if(available_extensions.begin(), available_extensions.end(),
					[extension_name](const VkExtensionProperties& props) {
						return std::strcmp(props.extensionName, extension_name) == 0;
					}) != available_extensions.end();
				};

			// Check for all required extension support
			bool all_extension_supported{ true };
			auto& requested_extensions = per_device_extensions[device_idx];
			for (auto& ext : requested_extensions_) {
				if (!check_ext_support(ext.first)) {
					if (ext.second) {
						VKW_LOGE("Device \"{}\" is not support required extension \"{}\"", properties.deviceName, ext.first);
						all_extension_supported = false;
					}

					VKW_LOGW("Device \"{}\" is not support optional extension \"{}\"", properties.deviceName, ext.first);
					continue;
				}

				requested_extensions.push_back(ext.first);
			}

			// Can't use this device, because some extensions is not supported
			if (!all_extension_supported) {
				continue;
			}

			// Check that device have queue with present support
			if (surface_) {
				auto queue_family_props = physical_device.get_queue_family_properties();

				bool surface_supported{ false };
				for (uint32_t queue_family_index = 0; queue_family_index < static_cast<uint32_t>(queue_family_props.size()); ++queue_family_index) {
					if (physical_device.get_surface_support_khr(queue_family_index, surface_) == VK_TRUE) {
						surface_supported = true;
						break;
					}
				}

				// We requested surface check, but device is not support it
				if (!surface_supported) {
					continue;
				}
			}

			// Not critical requirements, save as fallback
			if (properties.apiVersion < minimal_api_ver || properties.deviceType != preferred_type_) {
				fallback_index = device_idx;
				continue;
			}

			best_candidate_index = device_idx;
			break;
		}

		int32_t selected_device_index = best_candidate_index;
		if (selected_device_index == -1) {
			if (fallback_index == -1) {
				return std::unexpected(VK_ERROR_INCOMPATIBLE_DRIVER);
			}
			selected_device_index = fallback_index;
		}

		auto& selected_device = physical_devices_[selected_device_index];
		auto& enabled_extensions = per_device_extensions[selected_device_index];

		auto properties = selected_device.get_properties();
		auto queue_family_properties = selected_device.get_queue_family_properties();

		VKW_LOGD("{} device \"{}\" selected.", to_string(properties.deviceType), properties.deviceName);

		Vector<VkDeviceQueueCreateInfo> queue_create_infos(queue_family_properties.size(), allocator_);
		Vector<Vector<float>> family_queue_priorities(queue_family_properties.size(), Vector<float>(allocator_), allocator_);
		for (uint32_t family_index = 0u; family_index < static_cast<uint32_t>(queue_create_infos.size()); ++family_index) {
			auto& family_props = queue_family_properties[family_index];

			auto& queue_priorities = family_queue_priorities[family_index];
			queue_priorities.resize(family_props.queueCount, 0.5f);

			auto& queue_create_info = queue_create_infos[family_index];
			queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queue_create_info.queueFamilyIndex = family_index;
			queue_create_info.queueCount = family_props.queueCount;
			queue_create_info.pQueuePriorities = queue_priorities.data();
		}

		VkDeviceCreateInfo create_info{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
		create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
		create_info.pQueueCreateInfos = queue_create_infos.data();
		create_info.enabledExtensionCount = static_cast<uint32_t>(enabled_extensions.size());
		create_info.ppEnabledExtensionNames = enabled_extensions.data();
		create_info.enabledLayerCount = 0u;
		create_info.ppEnabledLayerNames = nullptr;
		create_info.pNext = nullptr;

		// Enable all possible features
		VkPhysicalDeviceVulkan11Features features11{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES };
		features11.pNext = nullptr;
		VkPhysicalDeviceVulkan12Features features12{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
		features12.pNext = nullptr;
		VkPhysicalDeviceVulkan13Features features13{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
		features13.pNext = nullptr;
		VkPhysicalDeviceVulkan14Features features14{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES };
		features14.pNext = nullptr;

		void* feature_chain{ nullptr };
		if (properties.apiVersion >= VK_API_VERSION_1_4) {
			feature_chain = &features14;
			features14.pNext = &features13;
			features13.pNext = &features12;
			features12.pNext = &features11;
			features11.pNext = last_feature_ptr_;
		}
		else if (properties.apiVersion >= VK_API_VERSION_1_3) {
			feature_chain = &features13;
			features13.pNext = &features12;
			features12.pNext = &features11;
			features11.pNext = last_feature_ptr_;
		}
		else if (properties.apiVersion >= VK_API_VERSION_1_2) {
			feature_chain = &features12;
			features12.pNext = &features11;
			features11.pNext = last_feature_ptr_;
		}
		else if (properties.apiVersion >= VK_API_VERSION_1_1) {
			feature_chain = &features11;
			features11.pNext = last_feature_ptr_;
		}
		else {
			feature_chain = last_feature_ptr_;
		}

		auto features2 = selected_device.get_features2(feature_chain);
		create_info.pEnabledFeatures = &features2.features;
		create_info.pNext = feature_chain;

		return selected_device.create_device(create_info);
	}

#undef VKW_SCOPE // DeviceSelector

#define VKW_SCOPE "DebugUtils"
	DebugUtils::~DebugUtils() {
		if (handle_) {
			vkDestroyDebugUtilsMessengerEXT(instance_, handle_, allocator_);
		}
	}

	auto DebugUtils::create_unique(Instance const& instance, Device const& device) -> Result<std::unique_ptr<DebugUtils>> {
		auto self = std::make_unique<DebugUtils>();
		if (auto result = self->_create(instance, device); result != VK_SUCCESS) {
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

	auto DebugUtils::_create(Instance const& instance, Device const& device) -> VkResult {
		instance_ = instance.get_handle();
		device_ = device.get_handle();
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
	
	auto DebugReport::create_unique(Instance const& instance, Device const& device) -> Result< std::unique_ptr<DebugReport>> {
		auto self = std::make_unique<DebugReport>();
		if (auto result = self->_create(instance, device); result != VK_SUCCESS) {
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
	
	auto DebugReport::_create(Instance const& instance, Device const& device) -> VkResult {
		instance_ = instance.get_handle();
		device_ = device.get_handle();
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