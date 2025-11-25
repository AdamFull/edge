#include "gfx_context.h"
#include "runtime/platform.h"

#include <edge_allocator.h>
#include <edge_vector.h>
#include <edge_logger.h>

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

struct gfx_instance {
	const edge_allocator_t* alloc;
	const VkAllocationCallbacks* alloc_cb;

	VkInstance handle;
	VkDebugUtilsMessengerEXT debug_messenger;

	const char* enabled_layers[32];
	uint32_t enabled_layer_count;

	const char* enabled_extensions[32];
	uint32_t enabled_extension_count;

	bool validation_enabled;
	bool synchronization_validation_enabled;
};

struct gfx_context {
	const edge_allocator_t* alloc;
	VkAllocationCallbacks alloc_callbacks;

	struct gfx_instance* instance;

	VkSurfaceKHR surf;
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

//#if defined(VK_USE_PLATFORM_WIN32_KHR)
//	VkWin32SurfaceCreateInfoKHR create_info;
//	platform_context_get_surface(platform_ctx, &create_info);
//
//	VkResult result = vkCreateWin32SurfaceKHR(ctx->inst, &create_info, &ctx->alloc_callbacks, &ctx->surf);
//	if (result != VK_SUCCESS) {
//		return false;
//	}
//#elif defined(VK_USE_PLATFORM_XLIB_KHR)
//#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
//#endif

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

static struct gfx_instance* gfx_instance_create(gfx_context_t* ctx) {
	if (!ctx || !ctx->alloc) {
		return NULL;
	}

	const edge_allocator_t* alloc = ctx->alloc;

	struct gfx_instance* inst = (struct gfx_instance*)edge_allocator_malloc(alloc, sizeof(struct gfx_instance));
	if (!inst) {
		return NULL;
	}

	inst->alloc = alloc;
	inst->alloc_cb = &ctx->alloc_callbacks;
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
	}

	if (available_extensions) {
		edge_allocator_free(alloc, available_extensions);
	}

	VkApplicationInfo app_info = { 0 };
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pApplicationName = "applicationname";
	app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0); // TODO: Generate
	app_info.pEngineName = "enginename";
	app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0); // TODO: Generate
	app_info.apiVersion = VK_API_VERSION_1_1;

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

	VkResult result = vkCreateInstance(&instance_info, &ctx->alloc_callbacks, &inst->handle);
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

		result = vkCreateDebugUtilsMessengerEXT(inst->handle, &debug_info, &ctx->alloc_callbacks, &inst->debug_messenger);
		if (result != VK_SUCCESS) {
			EDGE_LOG_WARN("Failed to create debug messenger: %d", result);
		}
	}
#endif

	return inst;

fatal_error:
	if (available_layers) {
		edge_allocator_free(alloc, available_layers);
	}

	if (available_extensions) {
		edge_allocator_free(alloc, available_extensions);
	}

	if (inst) {
		edge_allocator_free(alloc, inst);
	}

	return NULL;
}

static void gfx_instance_destroy(struct gfx_instance* inst) {
	if (!inst || !inst->alloc || !inst->alloc_cb) {
		return;
	}

	if (inst->debug_messenger != VK_NULL_HANDLE) {
		vkDestroyDebugUtilsMessengerEXT(inst->handle, inst->debug_messenger, inst->alloc_cb);
	}

	if (inst->handle != VK_NULL_HANDLE) {
		vkDestroyInstance(inst->handle, inst->alloc_cb);
	}

	const edge_allocator_t* alloc = inst->alloc;
	edge_allocator_free(alloc, inst);
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

	ctx->instance = gfx_instance_create(ctx);
	if (!ctx->instance) {
		gfx_context_destroy(ctx);
		return NULL;
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

	if (ctx->instance) {
		gfx_instance_destroy(ctx->instance);
	}

	const edge_allocator_t* alloc = ctx->alloc;
	edge_allocator_free(alloc, ctx);

	volkFinalize();
}