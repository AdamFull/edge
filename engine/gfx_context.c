#include "gfx_context.h"
#include "runtime/platform.h"

#include <edge_allocator.h>
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

struct gfx_context {
	const edge_allocator_t* alloc;
	VkAllocationCallbacks alloc_callbacks;
	VkResult volk_result;

	VkInstance inst;
	VkDebugUtilsMessengerEXT debug_messenger;
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

static bool gfx_context_init_instance(gfx_context_t* ctx) {
	if (!ctx) {
		return false;
	}

	const char* layers[] = {
#if defined(USE_VALIDATION_LAYERS)
		"VK_LAYER_KHRONOS_validation"
#endif
	};
	uint32_t layer_count = sizeof(layers) / sizeof(layers[0]);

	const char* extensions[] = {
		VK_KHR_SURFACE_EXTENSION_NAME,
#if defined(VK_USE_PLATFORM_WIN32_KHR)
		VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
		VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
		VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
#endif
		VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
#if defined(EDGE_DEBUG)
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
	};
	uint32_t extension_count = sizeof(extensions) / sizeof(extensions[0]);

	VkApplicationInfo application_info;
	application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	application_info.pNext = NULL;
	application_info.pApplicationName = "applicationname";
	application_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	application_info.pEngineName = "enginename";
	application_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	application_info.apiVersion = VK_MAKE_VERSION(1, 1, 0);

	VkInstanceCreateInfo create_info;
	create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.pNext = NULL;
	create_info.flags = 0;
	create_info.pApplicationInfo = &application_info;
	create_info.enabledLayerCount = layer_count;
	create_info.ppEnabledLayerNames = layers;
	create_info.enabledExtensionCount = extension_count;
	create_info.ppEnabledExtensionNames = extensions;

	VkResult result = vkCreateInstance(&create_info, &ctx->alloc_callbacks, &ctx->inst);
	if (result != VK_SUCCESS) {
		return false;
	}

	volkLoadInstance(ctx->inst);

#if defined(EDGE_DEBUG)
	VkDebugUtilsMessengerCreateInfoEXT debug_create_info;
	debug_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	debug_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	debug_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	debug_create_info.pfnUserCallback = debug_utils_messenger_cb;

	result = vkCreateDebugUtilsMessengerEXT(ctx->inst, &debug_create_info, &ctx->alloc_callbacks, &ctx->debug_messenger);
	if (result != VK_SUCCESS) {
		return false;
	}
#endif

	return true;
}

gfx_context_t* gfx_context_create(const edge_allocator_t* alloc, platform_context_t* platform_ctx) {
	if (!alloc || !platform_ctx) {
		return NULL;
	}

	gfx_context_t* ctx = (gfx_context_t*)edge_allocator_calloc(alloc, 1, sizeof(gfx_context_t));
	if (!ctx) {
		return NULL;
	}

	ctx->alloc = alloc;

	// Init allocation callbacks
	ctx->alloc_callbacks.pUserData = (void*)alloc;
	ctx->alloc_callbacks.pfnAllocation = vk_alloc_cb;
	ctx->alloc_callbacks.pfnReallocation = vk_realloc_cb;
	ctx->alloc_callbacks.pfnFree = vk_free_cb;
	ctx->alloc_callbacks.pfnInternalAllocation = vk_internal_alloc_cb;
	ctx->alloc_callbacks.pfnInternalFree = vk_internal_free_cb;

	// Init volk
	ctx->volk_result = (VkResult)-1;
	ctx->volk_result = volkInitialize();

	if (!gfx_context_init_instance(ctx)) {
		gfx_context_destroy(ctx);
		return NULL;
	}

#if defined(VK_USE_PLATFORM_WIN32_KHR)
	VkWin32SurfaceCreateInfoKHR surface_create_info;
	platform_context_get_surface(platform_ctx, &surface_create_info);

	VkResult result = vkCreateWin32SurfaceKHR(ctx->inst, &surface_create_info, &ctx->alloc_callbacks, &ctx->surf);
	if (result != VK_SUCCESS) {
		gfx_context_destroy(ctx);
		return NULL;
	}
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
#endif

	return ctx;
}

void gfx_context_destroy(gfx_context_t* ctx) {
	if (!ctx) {
		return;
	}

	if (ctx->surf != VK_NULL_HANDLE) {
		vkDestroySurfaceKHR(ctx->inst, ctx->surf, &ctx->alloc_callbacks);
	}

#if defined(EDGE_DEBUG)
	if (ctx->debug_messenger != VK_NULL_HANDLE) {
		vkDestroyDebugUtilsMessengerEXT(ctx->inst, ctx->debug_messenger, &ctx->alloc_callbacks);
	}
#endif

	if (ctx->inst != VK_NULL_HANDLE) {
		vkDestroyInstance(ctx->inst, &ctx->alloc_callbacks);
	}

	if (ctx->volk_result == VK_SUCCESS) {
		volkFinalize();
	}

	const edge_allocator_t* alloc = ctx->alloc;
	edge_allocator_free(alloc, ctx);
}