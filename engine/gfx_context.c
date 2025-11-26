#include "gfx_context.h"
#include "runtime/platform.h"

#include <edge_allocator.h>
#include <edge_vector.h>
#include <edge_logger.h>

#if EDGE_DEBUG
#include <edge_testing.h>
#include <assert.h>
#endif

#include <volk.h>
#include <vulkan/vulkan.h>

#if defined(EDGE_DEBUG) && defined(EDGE_VK_USE_VALIDATION_LAYERS)
#ifndef USE_VALIDATION_LAYERS
#define USE_VALIDATION_LAYERS
#endif
#endif

#if defined(USE_VALIDATION_LAYERS) && (defined(EDGE_VK_USE_GPU_ASSISTED_VALIDATION) || defined(EDGE_VK_USE_BEST_PRACTICES_VALIDATION) || defined(EDGE_VK_USE_SYNCHRONIZATION_VALIDATION))
#ifndef USE_VALIDATION_LAYER_FEATURES
#define USE_VALIDATION_LAYER_FEATURES
#endif
#endif

#define GFX_ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

struct gfx_key_value {
	const char* key;
	bool required;
};

static const char* g_instance_layers[] = {
#if defined(USE_VALIDATION_LAYERS)
	"VK_LAYER_KHRONOS_validation",
#if defined(EDGE_VK_USE_SYNCHRONIZATION_VALIDATION)
	"VK_LAYER_KHRONOS_synchronization2"
#endif
#endif
};

static VkValidationFeatureEnableEXT g_validation_features_enable[] = {
#if defined(USE_VALIDATION_LAYER_FEATURES)
	VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT,
#if defined(EDGE_VK_USE_GPU_ASSISTED_VALIDATION)
	VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_RESERVE_BINDING_SLOT_EXT,
	VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
#endif
#if defined(EDGE_VK_USE_BEST_PRACTICES_VALIDATION)
	VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT,
#endif
#if defined(EDGE_VK_USE_SYNCHRONIZATION_VALIDATION)
	VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT
#endif
#endif
};

static const char* g_instance_extensions[] = {
	VK_KHR_SURFACE_EXTENSION_NAME, 
#if defined(VK_USE_PLATFORM_WIN32_KHR)
	VK_KHR_WIN32_SURFACE_EXTENSION_NAME, 
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
	VK_KHR_XLIB_SURFACE_EXTENSION_NAME, 
#elif defined(VK_USE_PLATFORM_XCB_KHR)
	VK_KHR_XCB_SURFACE_EXTENSION_NAME, 
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
	VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME, 
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
	VK_KHR_ANDROID_SURFACE_EXTENSION_NAME, 
#elif defined(VK_USE_PLATFORM_MACOS_MVK)
	VK_MVK_MACOS_SURFACE_EXTENSION_NAME, 
#elif defined(VK_USE_PLATFORM_IOS_MVK)
	VK_MVK_IOS_SURFACE_EXTENSION_NAME, 
#elif defined(VK_USE_PLATFORM_METAL_EXT)
	VK_EXT_METAL_SURFACE_EXTENSION_NAME, 
#else
	VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME, 
#endif
#if EDGE_DEBUG
	VK_EXT_DEBUG_UTILS_EXTENSION_NAME
#endif // EDGE_DEBUG
};

static struct gfx_key_value g_device_extensions[] = {
	{ VK_KHR_SWAPCHAIN_EXTENSION_NAME, true },
	{ VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME, true },
	{ VK_KHR_MAINTENANCE_4_EXTENSION_NAME, true },
	{ VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME, true },
	{ VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME, true },
	{ VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME, true },
	{ VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME, true },
	{ VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME, true },
	{ VK_KHR_8BIT_STORAGE_EXTENSION_NAME, true },
	{ VK_KHR_16BIT_STORAGE_EXTENSION_NAME, true },
	{ VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME, true },
	{ VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME, true },
	{ VK_KHR_SPIRV_1_4_EXTENSION_NAME, true },
	{ VK_KHR_SEPARATE_DEPTH_STENCIL_LAYOUTS_EXTENSION_NAME, true },
	{ VK_KHR_COPY_COMMANDS_2_EXTENSION_NAME, true },
	{ VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME, true },
	{ VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME, true },
	{ VK_KHR_VULKAN_MEMORY_MODEL_EXTENSION_NAME, true },
	{ VK_EXT_MEMORY_BUDGET_EXTENSION_NAME, false },
	{ VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME, false },
	{ VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME, false },
	{ VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME, false },
	{ VK_KHR_PERFORMANCE_QUERY_EXTENSION_NAME, false },
	{ VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME, false },
	{ VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME, false },
	{ VK_KHR_BIND_MEMORY_2_EXTENSION_NAME, false },
	{ VK_AMD_DEVICE_COHERENT_MEMORY_EXTENSION_NAME, false },
	{ VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME, true },
	{ VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME, true },
	{ VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME, true },
#if USE_NSIGHT_AFTERMATH
	{ VK_NV_DEVICE_DIAGNOSTIC_CHECKPOINTS_EXTENSION_NAME, false },
	{ VK_NV_DEVICE_DIAGNOSTICS_CONFIG_EXTENSION_NAME, false },
#endif
};

static const uint32_t g_required_api_version = VK_API_VERSION_1_1;

typedef struct gfx_instance {
	VkInstance handle;
	VkDebugUtilsMessengerEXT debug_messenger;

	const char* enabled_layers[32];
	uint32_t enabled_layer_count;

	const char* enabled_extensions[32];
	uint32_t enabled_extension_count;

	bool validation_enabled;
	bool synchronization_validation_enabled;
} gfx_instance_t;

typedef struct gfx_adapter {
	VkPhysicalDevice handle;

	VkPhysicalDeviceProperties properties;
	VkPhysicalDeviceFeatures features;

	const char* enabled_extensions[64];
	uint32_t enabled_extension_count;
} gfx_adapter_t;

typedef struct gfx_device {
	VkDevice handle;
} gfx_device_t;

struct gfx_context {
	const edge_allocator_t* alloc;
	VkAllocationCallbacks alloc_callbacks;

	struct gfx_instance instance;
	VkSurfaceKHR surf;
	gfx_adapter_t adapter;
	gfx_device_t device;
};

static void* VKAPI_CALL vk_alloc_cb(void* user_data, size_t size, size_t alignment, VkSystemAllocationScope allocation_scope) {
	const edge_allocator_t* alloc = (const edge_allocator_t*)user_data;
	return edge_allocator_malloc(alloc, size);
}

static void VKAPI_CALL vk_free_cb(void* user_data, void* memory) {
    const edge_allocator_t* alloc = (const edge_allocator_t*)user_data;
    edge_allocator_free(alloc, memory);
}

static void* VKAPI_CALL vk_realloc_cb(void* user_data, void* original, size_t size, size_t alignment, VkSystemAllocationScope allocation_scope) {
	const edge_allocator_t* alloc = (const edge_allocator_t*)user_data;
	return edge_allocator_realloc(alloc, original, size);
}

static void VKAPI_CALL vk_internal_alloc_cb(void* user_data, size_t size, VkInternalAllocationType allocation_type, VkSystemAllocationScope allocation_scope) {
    (void)user_data;
    (void)size;
    (void)allocation_type;
    (void)allocation_scope;
}

static void VKAPI_CALL vk_internal_free_cb(void* user_data, size_t size, VkInternalAllocationType allocation_type, VkSystemAllocationScope allocation_scope) {
    (void)user_data;
    (void)size;
    (void)allocation_type;
    (void)allocation_scope;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_utils_messenger_cb(
	VkDebugUtilsMessageSeverityFlagBitsEXT message_severity, VkDebugUtilsMessageTypeFlagsEXT message_type,
	const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data) {
	(void)message_type;
	(void)user_data;

	if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
		EDGE_LOG_DEBUG("[DebugUtilsMessenger]: %d - %s: %s", callback_data->messageIdNumber, callback_data->pMessageIdName, callback_data->pMessage);
	}
	else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
		EDGE_LOG_INFO("[DebugUtilsMessenger]: %d - %s: %s", callback_data->messageIdNumber, callback_data->pMessageIdName, callback_data->pMessage);
	}
	else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
		EDGE_LOG_WARN("[DebugUtilsMessenger]: %d - %s: %s", callback_data->messageIdNumber, callback_data->pMessageIdName, callback_data->pMessage);
	}
	else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
		EDGE_LOG_ERROR("[DebugUtilsMessenger]: %d - %s: %s", callback_data->messageIdNumber, callback_data->pMessageIdName, callback_data->pMessage);
	}

	return VK_FALSE;
}

static bool is_extension_supported(const char* extension_name, const VkExtensionProperties* available_extensions, uint32_t available_count) {
	for (int32_t i = 0; i < available_count; i++) {
		if (strcmp(extension_name, available_extensions[i].extensionName) == 0) {
			return true;
		}
	}
	return false;
}

static bool is_layer_supported(const char* layer_name, const VkLayerProperties* available_layers, uint32_t available_count) {
	for (int32_t i = 0; i < available_count; i++) {
		if (strcmp(layer_name, available_layers[i].layerName) == 0) {
			return true;
		}
	}
	return false;
}

static bool gfx_instance_init(struct gfx_instance* inst, const edge_allocator_t* alloc, const VkAllocationCallbacks* alloc_cb) {
	if (!inst || !alloc || !alloc_cb) {
		return false;
	}

	inst->handle = VK_NULL_HANDLE;
	inst->debug_messenger = VK_NULL_HANDLE;
	inst->enabled_layer_count = 0;
	inst->enabled_extension_count = 0;
	inst->validation_enabled = false;
	inst->synchronization_validation_enabled = false;

	uint32_t layer_count = 0;
	vkEnumerateInstanceLayerProperties(&layer_count, NULL);
	VkLayerProperties* available_layers = NULL;
	if (layer_count > 0) {
		available_layers = (VkLayerProperties*)edge_allocator_malloc(alloc, layer_count * sizeof(VkLayerProperties));
		vkEnumerateInstanceLayerProperties(&layer_count, available_layers);
	}

	uint32_t extension_count = 0;
	vkEnumerateInstanceExtensionProperties(NULL, &extension_count, NULL);
	VkExtensionProperties* available_extensions = NULL;
	if (extension_count > 0) {
		available_extensions = (VkExtensionProperties*)edge_allocator_malloc(alloc, extension_count * sizeof(VkExtensionProperties));
		vkEnumerateInstanceExtensionProperties(NULL, &extension_count, available_extensions);
	}
	
	for (int i = 0; i < GFX_ARRAY_SIZE(g_instance_layers); ++i) {
		const char* layer_name = g_instance_layers[i];
		if (is_layer_supported(layer_name, available_layers, layer_count)) {
			inst->enabled_layers[inst->enabled_layer_count++] = layer_name;
#ifdef USE_VALIDATION_LAYERS
			if (strcmp(layer_name, "VK_LAYER_KHRONOS_validation") == 0) {
				inst->validation_enabled = true;
			}
			else if (strcmp(layer_name, "VK_LAYER_KHRONOS_synchronization2") == 0) {
				inst->synchronization_validation_enabled = true;
			}
#endif
		}
		else {
			EDGE_LOG_WARN("Layer not supported: %s", layer_name);
		}
	}

	for (int i = 0; i < GFX_ARRAY_SIZE(g_instance_extensions); ++i) {
		const char* ext_name = g_instance_extensions[i];
		if (is_extension_supported(ext_name, available_extensions, extension_count)) {
			inst->enabled_extensions[inst->enabled_extension_count++] = ext_name;
		}
		else {
			EDGE_LOG_WARN("Required instance extension not supported: %s\n", ext_name);
		}
	}

	if (available_layers) {
		edge_allocator_free(alloc, available_layers);
		available_layers = NULL;
	}

	if (available_extensions) {
		edge_allocator_free(alloc, available_extensions);
		available_extensions = NULL;
	}

	VkApplicationInfo app_info = { 0 };
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pApplicationName = "applicationname";
	app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0); // TODO: Generate
	app_info.pEngineName = "enginename";
	app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0); // TODO: Generate
	app_info.apiVersion = g_required_api_version;

	VkInstanceCreateInfo instance_info = { 0 };
	instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instance_info.pApplicationInfo = &app_info;
	instance_info.enabledLayerCount = inst->enabled_layer_count;
	instance_info.ppEnabledLayerNames = inst->enabled_layers;
	instance_info.enabledExtensionCount = inst->enabled_extension_count;
	instance_info.ppEnabledExtensionNames = inst->enabled_extensions;

#ifdef USE_VALIDATION_LAYER_FEATURES
	VkValidationFeaturesEXT validation_features = { 0 };
	if (inst->validation_enabled) {
		validation_features.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
		validation_features.enabledValidationFeatureCount = GFX_ARRAY_SIZE(g_validation_features_enable);
		validation_features.pEnabledValidationFeatures = g_validation_features_enable;
		instance_info.pNext = &validation_features;
	}
#endif

	VkResult result = vkCreateInstance(&instance_info, alloc_cb, &inst->handle);
	if (result != VK_SUCCESS) {
		EDGE_LOG_FATAL("Failed to create Vulkan instance: %d\n", result);
		goto fatal_error;
	}

	volkLoadInstance(inst->handle);

#ifdef USE_VALIDATION_LAYERS
	if (inst->validation_enabled) {
		VkDebugUtilsMessengerCreateInfoEXT debug_info = { 0 };
		debug_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		debug_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		debug_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		debug_info.pfnUserCallback = debug_utils_messenger_cb;

		result = vkCreateDebugUtilsMessengerEXT(inst->handle, &debug_info, alloc_cb, &inst->debug_messenger);
		if (result != VK_SUCCESS) {
			EDGE_LOG_WARN("Failed to create debug messenger: %d", result);
		}
	}
#endif

	return true;

fatal_error:
	if (available_layers) {
		edge_allocator_free(alloc, available_layers);
	}

	if (available_extensions) {
		edge_allocator_free(alloc, available_extensions);
	}

	return VK_ERROR_UNKNOWN;
}

static void gfx_instance_destroy(gfx_instance_t* inst, const VkAllocationCallbacks* alloc_cb) {
	if (!inst || !alloc_cb) {
		return;
	}

	if (inst->debug_messenger != VK_NULL_HANDLE) {
		vkDestroyDebugUtilsMessengerEXT(inst->handle, inst->debug_messenger, alloc_cb);
	}

	if (inst->handle != VK_NULL_HANDLE) {
		vkDestroyInstance(inst->handle, alloc_cb);
	}
}

static bool gfx_select_adapter(gfx_adapter_t* adapter_out, gfx_instance_t* instance, const edge_allocator_t* alloc, VkSurfaceKHR surface) {
	if (!adapter_out || !instance || !alloc) {
		return false;
	}

	int32_t best_score = -1;
	int32_t selected_device = -1;

	uint32_t adapter_count = 0;
	vkEnumeratePhysicalDevices(instance->handle, &adapter_count, NULL);

	if (adapter_count == 0) {
		EDGE_LOG_FATAL("No Vulkan-capable GPUs found");
		return false;
	}

	VkPhysicalDevice* adapters = (VkPhysicalDevice*)edge_allocator_malloc(alloc, adapter_count * sizeof(VkPhysicalDevice));
	vkEnumeratePhysicalDevices(instance->handle, &adapter_count, adapters);

	for (int32_t i = 0; i < adapter_count; i++) {
		VkPhysicalDevice adapter = adapters[i];

		const char* enabled_extensions[64] = { 0 };
		uint32_t enabled_extension_count = 0;

		VkPhysicalDeviceProperties properties;
		vkGetPhysicalDeviceProperties(adapter, &properties);

		VkPhysicalDeviceFeatures features;
		vkGetPhysicalDeviceFeatures(adapter, &features);

		uint32_t extension_count = 0;
		vkEnumerateDeviceExtensionProperties(adapter, NULL, &extension_count, NULL);
		VkExtensionProperties* available_extensions = (VkExtensionProperties*)edge_allocator_malloc(alloc, extension_count * sizeof(VkExtensionProperties));
		vkEnumerateDeviceExtensionProperties(adapter, NULL, &extension_count, available_extensions);

		int32_t adapter_score = 0;

		if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
			adapter_score += 1000;
		}

		// Check extensions
		bool all_required_found = true;
		for (int32_t j = 0; j < GFX_ARRAY_SIZE(g_device_extensions); ++j) {
			struct gfx_key_value* ext_pair = &g_device_extensions[j];
			bool supported = is_extension_supported(ext_pair->key, available_extensions, extension_count);
			if (!supported && ext_pair->required) {
				all_required_found = false;
				break;
			}
			else if (supported && !ext_pair->required) {
				adapter_score += 100;
			}

			if (supported) {
				enabled_extensions[enabled_extension_count++] = ext_pair->key;
			}
		}

		edge_allocator_free(alloc, available_extensions);

		if (!all_required_found) {
			continue;
		}

		uint32_t queue_family_count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(adapter, &queue_family_count, NULL);

		VkQueueFamilyProperties* queue_families = (VkQueueFamilyProperties*)edge_allocator_malloc(alloc, queue_family_count * sizeof(VkQueueFamilyProperties));
		vkGetPhysicalDeviceQueueFamilyProperties(adapter, &queue_family_count, queue_families);

		// Check surface support
		if (surface != VK_NULL_HANDLE) {
			VkBool32 surface_supported = VK_FALSE;
			for (int32_t j = 0; j < queue_family_count; ++j) {
				vkGetPhysicalDeviceSurfaceSupportKHR(adapter, j, surface, &surface_supported);
				if (surface_supported == VK_TRUE) {
					break;
				}
			}

			if (surface_supported == VK_FALSE) {
				continue;
			}
		}

		if (properties.apiVersion >= g_required_api_version) {
			adapter_score += 500;
		}

		// Check supported queues
		for (int32_t j = 0; j < queue_family_count; ++j) {
			VkQueueFamilyProperties* queue_family = &queue_families[j];
			adapter_score += queue_family->queueCount * 10;
		}

		edge_allocator_free(alloc, queue_families);

		if (adapter_score > best_score) {
			best_score = adapter_score;
			selected_device = i;

			adapter_out->handle = adapter;
			memcpy(&adapter_out->properties, &properties, sizeof(VkPhysicalDeviceProperties));
			memcpy(&adapter_out->features, &features, sizeof(VkPhysicalDeviceFeatures));
			memcpy(&adapter_out->enabled_extensions, enabled_extensions, enabled_extension_count * sizeof(const char*));
			adapter_out->enabled_extension_count = enabled_extension_count;
		}
	}

	edge_allocator_free(alloc, adapters);

	if (selected_device < 0) {
		return false;
	}

	return true;
}

static bool gfx_device_init(gfx_device_t* device_out, gfx_adapter_t* adapter, const edge_allocator_t* alloc, const VkAllocationCallbacks* alloc_cb) {
	if (!device_out || !alloc || !alloc_cb) {
		return false;
	}

	uint32_t queue_family_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(adapter->handle, &queue_family_count, NULL);

	VkQueueFamilyProperties* queue_families = (VkQueueFamilyProperties*)edge_allocator_malloc(alloc, queue_family_count * sizeof(VkQueueFamilyProperties));
	vkGetPhysicalDeviceQueueFamilyProperties(adapter->handle, &queue_family_count, queue_families);

	VkDeviceQueueCreateInfo* queue_create_infos = (VkDeviceQueueCreateInfo*)edge_allocator_calloc(alloc, queue_family_count, sizeof(VkDeviceQueueCreateInfo));
	float queue_priorities[32];
	for (int32_t i = 0; i < 32; ++i) {
		queue_priorities[i] = 1.0f;
	}

	for (int32_t i = 0; i < queue_family_count; ++i) {
		VkDeviceQueueCreateInfo* queue_create_info = &queue_create_infos[i];
		queue_create_info->sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_create_info->queueFamilyIndex = i;
		queue_create_info->queueCount = queue_families[i].queueCount;
		queue_create_info->pQueuePriorities = queue_priorities;
	}

	if (queue_families) {
		edge_allocator_free(alloc, queue_families);
		queue_families = NULL;
	}

	VkPhysicalDeviceSynchronization2FeaturesKHR sync2_features = { 0 };
	sync2_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR;
	sync2_features.pNext = NULL;

	VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic_rendering_features = { 0 };
	dynamic_rendering_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
	sync2_features.pNext = &dynamic_rendering_features;

	VkPhysicalDeviceDescriptorIndexingFeaturesEXT descriptor_indexing_features = { 0 };
	descriptor_indexing_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;
	dynamic_rendering_features.pNext = &descriptor_indexing_features;

	void* last_feature = &sync2_features;

	VkPhysicalDeviceVulkan11Features features_vk11 = { 0 };
	features_vk11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;

	VkPhysicalDeviceVulkan12Features features_vk12 = { 0 };
	features_vk12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;

	VkPhysicalDeviceVulkan13Features features_vk13 = { 0 };
	features_vk13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

	VkPhysicalDeviceVulkan14Features features_vk14 = { 0 };
	features_vk14.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES;

	void* feature_chain = NULL;
	if (g_required_api_version >= VK_API_VERSION_1_4) {
		feature_chain = &features_vk14;
		features_vk14.pNext = &features_vk13;
		features_vk13.pNext = &features_vk12;
		features_vk12.pNext = &features_vk11;
		features_vk11.pNext = last_feature;
	}
	else if (g_required_api_version >= VK_API_VERSION_1_3) {
		feature_chain = &features_vk13;
		features_vk13.pNext = &features_vk12;
		features_vk12.pNext = &features_vk11;
		features_vk11.pNext = last_feature;
	}
	else if (g_required_api_version >= VK_API_VERSION_1_2) {
		feature_chain = &features_vk12;
		features_vk12.pNext = &features_vk11;
		features_vk11.pNext = last_feature;
	}
	else if (g_required_api_version >= VK_API_VERSION_1_1) {
		feature_chain = &features_vk11;
		features_vk11.pNext = last_feature;
	}
	else {
		feature_chain = last_feature;
	}

	VkPhysicalDeviceFeatures2 features2 = { 0 };
	features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	features2.pNext = feature_chain;

	vkGetPhysicalDeviceFeatures2(adapter->handle, &features2);

	VkDeviceCreateInfo create_info = { 0 };
	create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	create_info.queueCreateInfoCount = queue_family_count;
	create_info.pQueueCreateInfos = queue_create_infos;
	create_info.enabledExtensionCount = adapter->enabled_extension_count;
	create_info.ppEnabledExtensionNames = adapter->enabled_extensions;
	create_info.pNext = &features2;

	// TODO: Add enabled extension flags in device

	VkResult result = vkCreateDevice(adapter->handle, &create_info, alloc_cb, &device_out->handle);
	if (result != VK_SUCCESS) {
		edge_allocator_free(alloc, queue_create_infos);
		return false;
	}

	edge_allocator_free(alloc, queue_create_infos);

	return true;
}

static void gfx_device_destroy(gfx_device_t* dev, const VkAllocationCallbacks* alloc_cb) {
	if (!dev || !alloc_cb) {
		return;
	}

	if (dev->handle != VK_NULL_HANDLE) {
		vkDestroyDevice(dev->handle, alloc_cb);
	}
}

gfx_context_t* gfx_context_create(const gfx_context_create_info_t* cteate_info) {
	if (!cteate_info || !cteate_info->alloc || !cteate_info->platform_context) {
		return NULL;
	}

	VkResult result = volkInitialize();
	if (result != VK_SUCCESS) {
		EDGE_LOG_ERROR("Failed to initialize volk: %d", result);
		return NULL;
	}

	gfx_context_t* ctx = (gfx_context_t*)edge_allocator_calloc(cteate_info->alloc, 1, sizeof(gfx_context_t));
	if (!ctx) {
		goto fatal_error;
	}

	ctx->alloc = cteate_info->alloc;

	// Init allocation callbacks
	ctx->alloc_callbacks.pUserData = (void*)ctx->alloc;
	ctx->alloc_callbacks.pfnAllocation = vk_alloc_cb;
	ctx->alloc_callbacks.pfnReallocation = vk_realloc_cb;
	ctx->alloc_callbacks.pfnFree = vk_free_cb;
	ctx->alloc_callbacks.pfnInternalAllocation = vk_internal_alloc_cb;
	ctx->alloc_callbacks.pfnInternalFree = vk_internal_free_cb;

	if (!gfx_instance_init(&ctx->instance, ctx->alloc, &ctx->alloc_callbacks)) {
		goto fatal_error;
	}

	// TODO: Maybe move to instance
	{
#if defined(VK_USE_PLATFORM_WIN32_KHR)
		VkWin32SurfaceCreateInfoKHR surface_create_info;
		platform_context_get_surface(cteate_info->platform_context, &surface_create_info);

		result = vkCreateWin32SurfaceKHR(ctx->instance.handle, &surface_create_info, &ctx->alloc_callbacks, &ctx->surf);
		if (result != VK_SUCCESS) {
			goto fatal_error;
		}
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
#endif
	}

	if (!gfx_select_adapter(&ctx->adapter, &ctx->instance, ctx->alloc, ctx->surf)) {
		goto fatal_error;
	}

	if (!gfx_device_init(&ctx->device, &ctx->adapter, ctx->alloc, &ctx->alloc_callbacks)) {
		goto fatal_error;
	}

	return ctx;

fatal_error:
	gfx_context_destroy(ctx);
	return NULL;
}

void gfx_context_destroy(gfx_context_t* ctx) {
	if (!ctx) {
		return;
	}

	gfx_device_destroy(&ctx->device, &ctx->alloc_callbacks);
	gfx_instance_destroy(&ctx->instance, &ctx->alloc_callbacks);

	const edge_allocator_t* alloc = ctx->alloc;
	edge_allocator_free(alloc, ctx);

	volkFinalize();
}