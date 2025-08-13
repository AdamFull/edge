#include "vk_context.h"

#include <spdlog/spdlog.h>

#include <volk.h>
#include <vulkan/vulkan.h>

#include <cstdlib>

#define VK_CHECK_RESULT(result, error_text) \
if(result != VK_SUCCESS) { \
	spdlog::error("[Vulkan Graphics Context]: {}", error_text); \
	return false; \
}

#if defined(ENGINE_DEBUG) || defined(VULKAN_VALIDATION_LAYERS)
#define USE_VALIDATION_LAYERS
#endif

#if defined(USE_VALIDATION_LAYERS) && (defined(VULKAN_VALIDATION_LAYERS_GPU_ASSISTED) || defined(VULKAN_VALIDATION_LAYERS_BEST_PRACTICES) || defined(VULKAN_VALIDATION_LAYERS_SYNCHRONIZATION))
#define USE_VALIDATION_LAYER_FEATURES
#endif

namespace edge::gfx {
	namespace
	{
#ifdef USE_VALIDATION_LAYERS
		VKAPI_ATTR VkBool32 VKAPI_CALL debug_utils_messenger_callback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity, VkDebugUtilsMessageTypeFlagsEXT message_type, const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data) {
			// Log debug message
			if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
				spdlog::info("{} - {}: {}", callback_data->messageIdNumber, callback_data->pMessageIdName, callback_data->pMessage);
			}
			else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
				spdlog::info("{} - {}: {}", callback_data->messageIdNumber, callback_data->pMessageIdName, callback_data->pMessage);
			}
			else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
				spdlog::warn("{} - {}: {}", callback_data->messageIdNumber, callback_data->pMessageIdName, callback_data->pMessage);
			}
			else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
				spdlog::error("{} - {}: {}", callback_data->messageIdNumber, callback_data->pMessageIdName, callback_data->pMessage);
			}
			return VK_FALSE; 
		}

		static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT /*type*/,
			uint64_t /*object*/, size_t /*location*/, int32_t /*message_code*/,
			const char* layer_prefix, const char* message, void* /*user_data*/) {
			if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
				spdlog::error("{}: {}", layer_prefix, message);
			}
			else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) {
				spdlog::warn("{}: {}", layer_prefix, message);
			}
			else if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT) {
				spdlog::warn("{}: {}", layer_prefix, message);
			}
			else if (flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT) {
				spdlog::debug("{}: {}", layer_prefix, message);
			}
			else {
				spdlog::info("{}: {}", layer_prefix, message);
			}
			return VK_FALSE;
		}
#endif
	}

	auto validate_layers(const std::vector<const char*>& required, const std::vector<VkLayerProperties>& available) -> bool {
		for (auto layer : required) {
			bool found = false;
			for (auto& available_layer : available) {
				if (strcmp(available_layer.layerName, layer) == 0) {
					found = true;
					break;
				}
			}

			if (!found) {
				spdlog::error("[Vulkan Graphics Context]: Validation Layer {} not found", layer);
				return false;
			}
		}

		return true;
	}

	auto get_optimal_validation_layers(const std::vector<VkLayerProperties>& supported_instance_layers) -> std::vector<const char*> {
		std::vector<std::vector<const char*>> validation_layer_priority_list = {
			// The preferred validation layer is "VK_LAYER_KHRONOS_validation"
			{"VK_LAYER_KHRONOS_validation"},

			// Otherwise we fallback to using the LunarG meta layer
			{"VK_LAYER_LUNARG_standard_validation"},

			// Otherwise we attempt to enable the individual layers that compose the LunarG meta layer since it doesn't exist
			{
				"VK_LAYER_GOOGLE_threading",
				"VK_LAYER_LUNARG_parameter_validation",
				"VK_LAYER_LUNARG_object_tracker",
				"VK_LAYER_LUNARG_core_validation",
				"VK_LAYER_GOOGLE_unique_objects",
			},

			// Otherwise as a last resort we fallback to attempting to enable the LunarG core layer
			{"VK_LAYER_LUNARG_core_validation"}
		};

		for (auto& validation_layers : validation_layer_priority_list) {
			if (validate_layers(validation_layers, supported_instance_layers)) {
				return validation_layers;
			}

			spdlog::warn("[Vulkan Graphics Context]: Couldn't enable validation layers (see log for error) - falling back");
		}

		// Else return nothing
		return {};
	}

	inline auto get_allocation_scope_str(VkSystemAllocationScope scope) -> const char* {
		switch (scope) {
		case VK_SYSTEM_ALLOCATION_SCOPE_COMMAND: return "command"; break;
		case VK_SYSTEM_ALLOCATION_SCOPE_OBJECT: return "object"; break;
		case VK_SYSTEM_ALLOCATION_SCOPE_CACHE: return "cache"; break;
		case VK_SYSTEM_ALLOCATION_SCOPE_DEVICE: return "device"; break;
		case VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE: return "instance"; break;
		default: return "unknown"; break;
		};
	}

	inline auto vkmemalloc(void* user_data, size_t size, size_t alignment, VkSystemAllocationScope allocation_scope) -> void* {
		auto* stats = static_cast<VkMemoryAllocationStats*>(user_data);

		void* ptr = nullptr;

#ifdef EDGE_PLATFORM_WINDOWS
		ptr = _aligned_malloc(size, alignment);
#else
		// POSIX aligned allocation
		if (posix_memalign(&ptr, alignment, size) != 0) {
			ptr = nullptr;
		}
#endif
		if (stats && ptr) {
			stats->total_bytes_allocated += size;
			stats->allocation_count += 1ull;
			stats->allocation_map[ptr] = { size, alignment, allocation_scope };

//#if VULKAN_DEBUG
//			spdlog::trace("[Vulkan Graphics Context]: Allocation({:#010x}, {} bytes, {} byte alignment, scope - {})",
//				reinterpret_cast<intptr_t>(ptr), size, alignment, get_allocation_scope_str(allocation_scope));
//#endif
		}

		return ptr;
	}

	inline auto vkmemfree(void* user_data, void* mem) -> void {
		if (!mem) {
			return;
		}

		auto* stats = static_cast<VkMemoryAllocationStats*>(user_data);
		if (stats) {
			if (auto found = stats->allocation_map.find(mem); found != stats->allocation_map.end()) {
				stats->total_bytes_allocated -= found->second.size;
				stats->deallocation_count += 1ull;

//#if VULKAN_DEBUG
//				spdlog::trace("[Vulkan Graphics Context]: Deallocation({:#010x}, {} bytes, {} byte alignment, scope - {})",
//					reinterpret_cast<intptr_t>(mem), found->second.size, found->second.align, get_allocation_scope_str(found->second.scope));
//#endif
				stats->allocation_map.erase(found);
			}
			else {
				spdlog::error("[Vulkan Graphics Context]: Found invalid memory allocation: {:#010x}.", reinterpret_cast<intptr_t>(mem));
			}
		}

#ifdef EDGE_PLATFORM_WINDOWS
		_aligned_free(mem);
#else
		free(mem);
#endif
	}

	inline auto vkmemrealloc(void* user_data, void* old, size_t size, size_t alignment, VkSystemAllocationScope allocation_scope) -> void* {
		if (!old) {
			return vkmemalloc(user_data, size, alignment, allocation_scope);
		}

		if (size == 0ull) {
			// Behave like free
			vkmemfree(user_data, old);
			return nullptr;
		}

		size_t old_size{ size };

		auto* stats = static_cast<VkMemoryAllocationStats*>(user_data);
		if (stats) {
			if (auto found = stats->allocation_map.find(old); found != stats->allocation_map.end()) {
				old_size = found->second.size;

//#if VULKAN_DEBUG
//				spdlog::trace("[Vulkan Graphics Context]: Reallocation({:#010x}, {} bytes, {} byte alignment, scope - {})",
//					reinterpret_cast<intptr_t>(old), found->second.size, found->second.align, get_allocation_scope_str(found->second.scope));
//#endif
			}
			else {
				spdlog::error("[Vulkan Graphics Context]: Found invalid memory allocation: {:#010x}.", reinterpret_cast<intptr_t>(old));
			}
		}

		void* new_ptr = vkmemalloc(user_data, size, alignment, allocation_scope);
		if (new_ptr && old) {
			memcpy(new_ptr, old, std::min(size, old_size));
			vkmemfree(user_data, old);
		}
		return new_ptr;
	}


	VulkanGraphicsContext::~VulkanGraphicsContext() {
		if (vk_debug_utils_messenger_) {
			vkDestroyDebugUtilsMessengerEXT(vk_instance_, vk_debug_utils_messenger_, &vk_alloc_callbacks_);
		}

		if (vk_debug_report_callback_) {
			vkDestroyDebugReportCallbackEXT(vk_instance_, vk_debug_report_callback_, &vk_alloc_callbacks_);
		}

		if (vk_instance_) {
			vkDestroyInstance(vk_instance_, &vk_alloc_callbacks_);
		}

		if (volk_initialized_) {
			volkFinalize();
		}

		// Check that all allocated vulkan objects was deallocated
		if (memalloc_stats_.allocation_count != memalloc_stats_.deallocation_count) {
			spdlog::error("[Vulkan Graphics Context]: Memory leaks detected!\n Allocated: {}, Deallocated: {} objects. Leaked {} bytes.",
				memalloc_stats_.allocation_count, memalloc_stats_.deallocation_count, memalloc_stats_.total_bytes_allocated);

			for (const auto& allocation : memalloc_stats_.allocation_map) {
				spdlog::warn("{:#010x} : {} bytes, {} byte alignment, {} scope", 
					reinterpret_cast<intptr_t>(allocation.first), allocation.second.size, allocation.second.align, get_allocation_scope_str(allocation.second.scope));
			}
		}
	}

	auto VulkanGraphicsContext::construct(const GraphicsContextCreateInfo create_info) -> std::unique_ptr<VulkanGraphicsContext> {
		auto self = std::make_unique<VulkanGraphicsContext>();
		self->_construct(create_info);
		return self;
	}

	auto VulkanGraphicsContext::initialize() -> bool {
		return true;
	}

	auto VulkanGraphicsContext::shutdown() -> void {

	}

	auto VulkanGraphicsContext::_construct(const GraphicsContextCreateInfo create_info) -> bool {
		memalloc_stats_.total_bytes_allocated = 0ull;
		memalloc_stats_.allocation_count = 0ull;
		memalloc_stats_.deallocation_count = 0ull;

		// Create allocation callbacks
		vk_alloc_callbacks_.pfnAllocation = vkmemalloc;
		vk_alloc_callbacks_.pfnFree = vkmemfree;
		vk_alloc_callbacks_.pfnReallocation = vkmemrealloc;
		vk_alloc_callbacks_.pfnInternalAllocation = [](void* user_data, size_t size, VkInternalAllocationType allocation_type, VkSystemAllocationScope allocation_scope) -> void {};
		vk_alloc_callbacks_.pfnInternalFree = [](void* user_data, size_t size, VkInternalAllocationType allocation_type, VkSystemAllocationScope allocation_scope) -> void {};
		vk_alloc_callbacks_.pUserData = &memalloc_stats_;

		VK_CHECK_RESULT(volkInitialize(), "Failed to initialize volk.");

		// Enumerate instance extensions
		uint32_t instance_property_count{ 0u };
		VK_CHECK_RESULT(vkEnumerateInstanceExtensionProperties(nullptr, &instance_property_count, nullptr),
			"Failed to request instance extension properties count.");

		std::vector<VkExtensionProperties> ext_props(instance_property_count);
		VK_CHECK_RESULT(vkEnumerateInstanceExtensionProperties(nullptr, &instance_property_count, ext_props.data()),
			"Failed to request instance extension properties.");

		// Collect all required instance extensions
		std::vector<const char*> instance_required_extensions{};
		instance_required_extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);

		// Add platform dependent surface extensions
#if EDGE_PLATFORM_ANDROID
		instance_required_extensions.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#elif EDGE_PLATFORM_WINDOWS
		instance_required_extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#endif

#if defined(VULKAN_DEBUG) && defined(USE_VALIDATION_LAYERS)
		bool VK_EXT_debug_utils_enabled{ false };
		auto debug_extension_it = std::find_if(ext_props.begin(), ext_props.end(), [](VkExtensionProperties const& ep) {
			return strcmp(ep.extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0;
			});

		if (debug_extension_it != ext_props.end()) {
			spdlog::info("[Vulkan Graphics Context]: Vulkan debug utils enabled ({})", VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
			instance_required_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
			VK_EXT_debug_utils_enabled = true;
		}

		bool VK_EXT_debug_report_enabled{ false };
		debug_extension_it = std::find_if(ext_props.begin(), ext_props.end(), [](VkExtensionProperties const& ep) {
			return strcmp(ep.extensionName, VK_EXT_DEBUG_REPORT_EXTENSION_NAME) == 0;
			});

		if (!VK_EXT_debug_utils_enabled && debug_extension_it != ext_props.end()) {
			spdlog::info("[Vulkan Graphics Context]: Vulkan debug report enabled ({})", VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
			instance_required_extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
			VK_EXT_debug_report_enabled = true;
		}
#endif

#if defined(VULKAN_ENABLE_PORTABILITY)
		instance_required_extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
		instance_required_extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
		enable_extension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME, available_instance_extensions, enabled_extensions);
		bool portability_enumeration_available = enable_extension(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME, available_instance_extensions, enabled_extensions);
#endif

#ifdef USE_VALIDATION_LAYER_FEATURES
		bool validation_features = false;
		{
			uint32_t available_layer_instance_extension_count{ 0u };
			VK_CHECK_RESULT(vkEnumerateInstanceExtensionProperties("VK_LAYER_KHRONOS_validation", &available_layer_instance_extension_count, nullptr),
				"Failed to request VK_LAYER_KHRONOS_validation instance extension properties count.");

			std::vector<VkExtensionProperties> available_layer_instance_extensions(available_layer_instance_extension_count);
			VK_CHECK_RESULT(vkEnumerateInstanceExtensionProperties("VK_LAYER_KHRONOS_validation", &available_layer_instance_extension_count, available_layer_instance_extensions.data()),
				"Failed to request instance extension properties.");

			for (auto& available_extension : available_layer_instance_extensions) {
				if (strcmp(available_extension.extensionName, VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME) == 0) {
					validation_features = true;
					spdlog::info("[Vulkan Graphics Context]: {} is available, enabling it", VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);
					instance_required_extensions.push_back(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);
				}
			}
		}
#endif

		// VK_KHR_get_physical_device_properties2 is a prerequisite of VK_KHR_performance_query
		// which will be used for stats gathering where available.
		instance_required_extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

		uint32_t supported_validation_layer_count{ 0u };
		vkEnumerateInstanceLayerProperties(&supported_validation_layer_count, nullptr);

		std::vector<VkLayerProperties> supported_validation_layers(supported_validation_layer_count);
		vkEnumerateInstanceLayerProperties(&supported_validation_layer_count, supported_validation_layers.data());

		std::vector<const char*> requested_validation_layers{};

#ifdef USE_VALIDATION_LAYERS
		// Determine the optimal validation layers to enable that are necessary for useful debugging
		auto optimal_validation_layers = get_optimal_validation_layers(supported_validation_layers);
		requested_validation_layers.insert(requested_validation_layers.end(), optimal_validation_layers.begin(), optimal_validation_layers.end());

#if defined(VULKAN_VALIDATION_LAYERS_SYNCHRONIZATION)
		requested_validation_layers.emplace_back("VK_LAYER_KHRONOS_synchronization2");
#endif // VULKAN_VALIDATION_LAYERS_SYNCHRONIZATION
#endif // USE_VALIDATION_LAYERS

		if (validate_layers(requested_validation_layers, supported_validation_layers)) {
			spdlog::info("[Vulkan Graphics Context]: Enabled Validation Layers:");
			for (const auto& layer : requested_validation_layers) {
				spdlog::info("	\t{}", layer);
			}
		}
		else {
			spdlog::error("[Vulkan Graphics Context]: Required validation layers are missing.");
			return false;
		}

		VkApplicationInfo application_info{};
		application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		application_info.applicationVersion = 1;
		application_info.engineVersion = 1;
		application_info.apiVersion = VK_API_VERSION_1_2;
		application_info.pApplicationName = "EdgeVulkanApp";
		application_info.pEngineName = "EdgeGameEngine";

		VkInstanceCreateInfo instance_create_info{};
		instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		instance_create_info.pApplicationInfo = &application_info;
		instance_create_info.enabledExtensionCount = static_cast<uint32_t>(instance_required_extensions.size());
		instance_create_info.ppEnabledExtensionNames = instance_required_extensions.data();
		instance_create_info.enabledLayerCount = static_cast<uint32_t>(requested_validation_layers.size());
		instance_create_info.ppEnabledLayerNames = requested_validation_layers.data();

#ifdef USE_VALIDATION_LAYERS
		VkDebugUtilsMessengerCreateInfoEXT debug_utils_create_info{};
		debug_utils_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

		VkDebugReportCallbackCreateInfoEXT debug_report_create_info{};
		debug_report_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;

		if (VK_EXT_debug_utils_enabled) {
			debug_utils_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
			debug_utils_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT; // VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT
			debug_utils_create_info.pfnUserCallback = debug_utils_messenger_callback;

			instance_create_info.pNext = &debug_utils_create_info;
		}
		else if (VK_EXT_debug_report_enabled)
		{
			debug_report_create_info.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
			debug_report_create_info.pfnCallback = debug_callback;

			instance_create_info.pNext = &debug_report_create_info;
		}
#endif

#if (defined(VULKAN_ENABLE_PORTABILITY))
		if (portability_enumeration_available) {
			instance_info.flags |= vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
		}
#endif

#ifdef USE_VALIDATION_LAYER_FEATURES
		VkValidationFeaturesEXT validation_features_info{};
		validation_features_info.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;

		std::vector<VkValidationFeatureEnableEXT> enable_features{};
		enable_features.emplace_back(VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT);

		if (validation_features) {
#if defined(VULKAN_VALIDATION_LAYERS_GPU_ASSISTED)
			enable_features.emplace_back(VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_RESERVE_BINDING_SLOT_EXT);
			enable_features.emplace_back(VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT);
#endif
#if defined(VULKAN_VALIDATION_LAYERS_BEST_PRACTICES)
			enable_features.emplace_back(VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT);
#endif
#if defined(VULKAN_VALIDATION_LAYERS_SYNCHRONIZATION)
			enable_features.emplace_back(VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT);
#endif
			validation_features_info.enabledValidationFeatureCount = static_cast<uint32_t>(enable_features.size());
			validation_features_info.pEnabledValidationFeatures = enable_features.data();
			validation_features_info.pNext = instance_create_info.pNext;
			instance_create_info.pNext = &validation_features_info;
		}
#endif

		// TODO: Not needed for now
		//VkLayerSettingsCreateInfoEXT layer_settings_create_info{};
		//layer_settings_create_info.sType = VK_STRUCTURE_TYPE_LAYER_SETTINGS_CREATE_INFO_EXT;
		//
		//// If layer settings extension enabled by sample, then activate layer settings during instance creation
		//if (is_enabled(VK_EXT_LAYER_SETTINGS_EXTENSION_NAME)) {
		//	layer_settings_create_info.settingCount = static_cast<uint32_t>(builder.layer_settings.size());
		//	layer_settings_create_info.pSettings = builder.layer_settings.data();
		//	layer_settings_create_info.pNext = instance_create_info.pNext;
		//	instance_create_info.pNext = &layer_settings_create_info;
		//}

		VK_CHECK_RESULT(vkCreateInstance(&instance_create_info, &vk_alloc_callbacks_, &vk_instance_),
			"Failed to create VkInstance.");

		// Need to load volk for all the not-yet Vulkan calls
		volkLoadInstance(vk_instance_);

#ifdef USE_VALIDATION_LAYERS
		if (VK_EXT_debug_utils_enabled) {
			VK_CHECK_RESULT(vkCreateDebugUtilsMessengerEXT(vk_instance_, &debug_utils_create_info, &vk_alloc_callbacks_, &vk_debug_utils_messenger_), 
				"Failed to create debug utils messager.");
		}
		else if (VK_EXT_debug_report_enabled) {
			VK_CHECK_RESULT(vkCreateDebugReportCallbackEXT(vk_instance_, &debug_report_create_info, &vk_alloc_callbacks_, &vk_debug_report_callback_), 
				"Failed to create debug report callback.");
		}
#endif

		// Get available physical devices
		uint32_t physical_device_count{ 0u };
		VK_CHECK_RESULT(vkEnumeratePhysicalDevices(vk_instance_, &physical_device_count, nullptr), 
			"Failed to get physical device count.");

		physical_devices_.resize(physical_device_count);
		VK_CHECK_RESULT(vkEnumeratePhysicalDevices(vk_instance_, &physical_device_count, physical_devices_.data()),
			"Failed to get physical devices.");

		// Create surface
#if EDGE_PLATFORM_ANDROID
		VkAndroidSurfaceCreateInfoKHR surface_create_info{};
		surface_create_info.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
		surface_create_info.window = create_info.window;
		VK_CHECK_RESULT(vkCreateAndroidSurfaceKHR(vk_instance_ , &surface_create_info, &vk_alloc_callbacks_, &vk_surface_), 
			"Failed to create surface.");
#elif EDGE_PLATFORM_WINDOWS
		VkWin32SurfaceCreateInfoKHR surface_create_info{};
		surface_create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		surface_create_info.hwnd = create_info.hwnd;
		surface_create_info.hinstance = create_info.hinst;

		VK_CHECK_RESULT(vkCreateWin32SurfaceKHR(vk_instance_, &surface_create_info, &vk_alloc_callbacks_, &vk_surface_), 
			"Failed to create surface.");
#endif

		//for (auto& physical_device : physical_devices) {
		//	VkPhysicalDeviceVulkan12Features features_vk12{};
		//	features_vk12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
		//
		//	VkPhysicalDeviceFeatures2 features{};
		//	features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		//	features.pNext = &features_vk12;
		//	vkGetPhysicalDeviceFeatures2(physical_device, &features);
		//
		//	VkPhysicalDeviceProperties properties{};
		//	vkGetPhysicalDeviceProperties(physical_device, &properties);
		//
		//	VkPhysicalDeviceMemoryProperties memory_properties{};
		//	vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);
		//
		//	std::stringstream uuid_ss;
		//	for (uint8_t byte : properties.pipelineCacheUUID) {
		//		uuid_ss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(byte);
		//	}
		//
		//	//spdlog::debug("[Vulkan Graphics Context]: Found GPU: {}\nDevice ID: {}\nVendor ID: {}\nDriver Version: {}\nAPI Version: {}\nDevice Type: {}\nPipeline Cache UUID: {}", 
		//	//	properties.deviceName, properties.deviceID, properties.vendorID, properties.driverVersion,
		//	//	properties.apiVersion, properties.deviceType, uuid_ss.str());
		//
		//	uint32_t queue_family_property_count{ 0u };
		//	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_property_count, nullptr);
		//
		//	std::vector<VkQueueFamilyProperties> queue_family_properties(queue_family_property_count);
		//	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_property_count, queue_family_properties.data());
		//}

		return true;
	}
}