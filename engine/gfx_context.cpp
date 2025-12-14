#include "gfx_context.h"
#include "runtime/platform.h"

#include <allocator.hpp>
#include <math.hpp>
#include <array.hpp>
#include <logger.hpp>

#include <assert.h>

#include <volk.h>

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

#define GFX_LAYERS_MAX 16
#define GFX_INSTANCE_EXTENSIONS_MAX 32
#define GFX_DEVICE_EXTENSIONS_MAX 128
#define GFX_ADAPTER_MAX 8u
#define GFX_QUEUE_FAMILY_MAX 16u
#define GFX_SURFACE_FORMAT_MAX 32u
#define GFX_PRESENT_MODES_MAX 8u
#define GFX_SWAPCHAIN_IMAGES_MAX 8

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
#if defined(EDGE_VK_USE_DEBUG_PRINTF)
	VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT,
#endif
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
	{ VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME, true },
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

static const u32 g_required_api_version = VK_API_VERSION_1_1;

namespace edge::gfx {
	struct Context {
		const Allocator* alloc;
		VkAllocationCallbacks vk_alloc;

		VkInstance inst;
		VkDebugUtilsMessengerEXT debug_msgr;

		bool validation_enabled;
		bool synchronization_validation_enabled;

		VkSurfaceKHR surf;
		VkSurfaceFormatKHR surf_formats[GFX_SURFACE_FORMAT_MAX];
		u32 surf_format_count;
		VkPresentModeKHR surf_present_modes[GFX_PRESENT_MODES_MAX];
		u32 surf_present_mode_count;

		VkPhysicalDevice adapter;

		VkPhysicalDeviceProperties properties;
		VkPhysicalDeviceFeatures features;

		const char* enabled_extensions[GFX_DEVICE_EXTENSIONS_MAX];
		u32 enabled_extension_count;

		VkQueueFamilyProperties queue_families[GFX_QUEUE_FAMILY_MAX];
		u32 queue_family_count;

		VkDevice dev;

		VmaAllocator vma;
	};

	static Context g_ctx = {};

	static void* VKAPI_CALL vk_alloc_cb(void* user_data, size_t size, size_t alignment, VkSystemAllocationScope allocation_scope) {
		const Allocator* alloc = (const Allocator*)user_data;
		return allocator_malloc(alloc, size);
	}

	static void VKAPI_CALL vk_free_cb(void* user_data, void* memory) {
		const Allocator* alloc = (const Allocator*)user_data;
		allocator_free(alloc, memory);
	}

	static void* VKAPI_CALL vk_realloc_cb(void* user_data, void* original, size_t size, size_t alignment, VkSystemAllocationScope allocation_scope) {
		const Allocator* alloc = (const Allocator*)user_data;
		return allocator_realloc(alloc, original, size);
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

	static inline u32 compute_max_mip_level(u32 size) {
		u32 level_count = 0;
		while (size) {
			level_count++;
			size >>= 1;
		}
		return level_count;
	}

	static bool is_extension_supported(const char* extension_name, const VkExtensionProperties* available_extensions, u32 available_count) {
		for (i32 i = 0; i < available_count; i++) {
			if (strcmp(extension_name, available_extensions[i].extensionName) == 0) {
				return true;
			}
		}
		return false;
	}

	static bool is_layer_supported(const char* layer_name, const VkLayerProperties* available_layers, u32 available_count) {
		for (i32 i = 0; i < available_count; i++) {
			if (strcmp(layer_name, available_layers[i].layerName) == 0) {
				return true;
			}
		}
		return false;
	}

	static VkExtent2D choose_suitable_extent(VkExtent2D request_extent, const VkSurfaceCapabilitiesKHR* surface_caps) {
		if (surface_caps->currentExtent.width == 0xFFFFFFFF) {
			return request_extent;
		}

		if (request_extent.width < 1 || request_extent.height < 1) {
			EDGE_LOG_WARN("Image extent %dx%d is not supported. Selecting available %dx%d.",
				request_extent.width, request_extent.height,
				surface_caps->currentExtent.width, surface_caps->currentExtent.height);
			return surface_caps->currentExtent;
		}

		request_extent.width = clamp(request_extent.width, surface_caps->minImageExtent.width, surface_caps->maxImageExtent.width);
		request_extent.height = clamp(request_extent.height, surface_caps->minImageExtent.height, surface_caps->maxImageExtent.height);

		return request_extent;
	}

	static bool is_hdr_format(VkFormat format) {
		switch (format) {
			// 10-bit formats
		case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
		case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
		case VK_FORMAT_A2B10G10R10_UINT_PACK32:
		case VK_FORMAT_A2R10G10B10_UINT_PACK32:
		case VK_FORMAT_A2B10G10R10_SINT_PACK32:
		case VK_FORMAT_A2R10G10B10_SINT_PACK32:

			// 16-bit float formats
		case VK_FORMAT_R16G16B16A16_SFLOAT:
		case VK_FORMAT_R16G16B16_SFLOAT:

			// 32-bit float formats
		case VK_FORMAT_R32G32B32A32_SFLOAT:
		case VK_FORMAT_R32G32B32_SFLOAT:

			// BC6H (HDR texture compression)
		case VK_FORMAT_BC6H_UFLOAT_BLOCK:
		case VK_FORMAT_BC6H_SFLOAT_BLOCK:

			// ASTC HDR
		case VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK:
		case VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK:
		case VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK:
		case VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK:
		case VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK:
		case VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK:
		case VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK:
		case VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK:
		case VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK:
		case VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK:
		case VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK:
		case VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK:
		case VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK:
		case VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK:
			return true;

		default:
			return false;
		}
	}

	static bool is_hdr_color_space(VkColorSpaceKHR color_space) {
		switch (color_space) {
		case VK_COLOR_SPACE_HDR10_ST2084_EXT:
		case VK_COLOR_SPACE_HDR10_HLG_EXT:
		case VK_COLOR_SPACE_DOLBYVISION_EXT:
		case VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT:
		case VK_COLOR_SPACE_EXTENDED_SRGB_NONLINEAR_EXT:
		case VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT:
		case VK_COLOR_SPACE_DISPLAY_P3_LINEAR_EXT:
		case VK_COLOR_SPACE_BT2020_LINEAR_EXT:
		case VK_COLOR_SPACE_BT709_LINEAR_EXT:
		case VK_COLOR_SPACE_DCI_P3_NONLINEAR_EXT:
		case VK_COLOR_SPACE_ADOBERGB_LINEAR_EXT:
		case VK_COLOR_SPACE_ADOBERGB_NONLINEAR_EXT:
			return true;

		default:
			return false;
		}
	}

	static bool is_surface_format_hdr(const VkSurfaceFormatKHR* format) {
		return is_hdr_format(format->format) && is_hdr_color_space(format->colorSpace);
	}

	static bool surface_format_equal(const VkSurfaceFormatKHR* a, const VkSurfaceFormatKHR* b, bool full_match) {
		if (full_match) {
			return a->format == b->format && a->colorSpace == b->colorSpace;
		}
		return a->format == b->format;
	}

	static bool find_surface_format(const VkSurfaceFormatKHR* available_formats, u32 available_count,
		const VkSurfaceFormatKHR* requested, VkSurfaceFormatKHR* out_format, bool full_match) {
		for (u32 i = 0; i < available_count; i++) {
			if (surface_format_equal(&available_formats[i], requested, full_match)) {
				*out_format = available_formats[i];
				return true;
			}
		}
		return false;
	}

	static bool pick_by_priority_list(const VkSurfaceFormatKHR* available_formats, u32 available_count,
		const VkSurfaceFormatKHR* priority_list, u32 priority_count, bool hdr_only, VkSurfaceFormatKHR* out_format) {
		for (u32 p = 0; p < priority_count; p++) {
			for (u32 i = 0; i < available_count; i++) {
				// Skip if filtering by HDR and format doesn't match
				if (hdr_only && !is_surface_format_hdr(&available_formats[i])) {
					continue;
				}
				if (!hdr_only && is_surface_format_hdr(&available_formats[i])) {
					continue;
				}

				if (surface_format_equal(&available_formats[i], &priority_list[p], true) ||
					surface_format_equal(&available_formats[i], &priority_list[p], false)) {
					*out_format = available_formats[i];
					return true;
				}
			}
		}
		return false;
	}

	static VkSurfaceFormatKHR choose_surface_format(VkSurfaceFormatKHR requested_surface_format, const VkSurfaceFormatKHR* available_surface_formats, u32 available_count, bool prefer_hdr) {
		static const VkSurfaceFormatKHR hdr_priority_list[] = {
			{ VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_COLOR_SPACE_HDR10_ST2084_EXT },
			{ VK_FORMAT_A2R10G10B10_UNORM_PACK32, VK_COLOR_SPACE_HDR10_ST2084_EXT },
			{ VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT },
			{ VK_FORMAT_A2R10G10B10_UNORM_PACK32, VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT },
			{ VK_FORMAT_R16G16B16A16_SFLOAT, VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT },
			{ VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_COLOR_SPACE_EXTENDED_SRGB_NONLINEAR_EXT },
			{ VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_COLOR_SPACE_BT2020_LINEAR_EXT },
			{ VK_FORMAT_R16G16B16A16_SFLOAT, VK_COLOR_SPACE_BT2020_LINEAR_EXT }
		};

		static const VkSurfaceFormatKHR sdr_priority_list[] = {
			{VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
			{VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
			{VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
			{VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
			{VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
			{VK_FORMAT_A8B8G8R8_SRGB_PACK32, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
			{VK_FORMAT_A8B8G8R8_UNORM_PACK32, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}
		};

		VkSurfaceFormatKHR result;

		if (available_count == 0) {
			result.format = VK_FORMAT_UNDEFINED;
			result.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
			return result;
		}

		if (requested_surface_format.format != VK_FORMAT_UNDEFINED) {
			if (find_surface_format(available_surface_formats, available_count,
				&requested_surface_format, &result, true)) {
				return result;
			}
			if (find_surface_format(available_surface_formats, available_count,
				&requested_surface_format, &result, false)) {
				return result;
			}
		}

		if (prefer_hdr) {
			if (pick_by_priority_list(available_surface_formats, available_count,
				hdr_priority_list, array_size(hdr_priority_list),
				true, &result)) {
				return result;
			}
		}

		if (pick_by_priority_list(available_surface_formats, available_count,
			sdr_priority_list, array_size(sdr_priority_list),
			false, &result)) {
			return result;
		}

		return available_surface_formats[0];
	}

	static VkCompositeAlphaFlagBitsKHR choose_suitable_composite_alpha(VkCompositeAlphaFlagBitsKHR request_composite_alpha, VkCompositeAlphaFlagsKHR supported_composite_alpha) {
		if (request_composite_alpha & supported_composite_alpha) {
			return request_composite_alpha;
		}

		static const VkCompositeAlphaFlagBitsKHR composite_alpha_priority_list[] = {
			VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
			VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
			VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
		};

		for (u32 i = 0; i < array_size(composite_alpha_priority_list); i++) {
			if (composite_alpha_priority_list[i] & supported_composite_alpha) {
				return composite_alpha_priority_list[i];
			}
		}

		return VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	}

	VkPresentModeKHR choose_suitable_present_mode(VkPresentModeKHR request_present_mode,
		const VkPresentModeKHR* available_present_modes, u32 available_count,
		const VkPresentModeKHR* present_mode_priority_list, u32 priority_count) {
		for (u32 i = 0; i < available_count; i++) {
			if (available_present_modes[i] == request_present_mode) {
				return request_present_mode;
			}
		}

		for (u32 p = 0; p < priority_count; p++) {
			for (u32 i = 0; i < available_count; i++) {
				if (available_present_modes[i] == present_mode_priority_list[p]) {
					return present_mode_priority_list[p];
				}
			}
		}

		return VK_PRESENT_MODE_FIFO_KHR;
	}

	static bool instance_init() {
		u32 layer_count = 0;
		vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
		VkLayerProperties* available_layers = nullptr;
		if (layer_count > 0) {
			available_layers = allocate<VkLayerProperties>(g_ctx.alloc, layer_count);
			vkEnumerateInstanceLayerProperties(&layer_count, available_layers);
		}

		u32 extension_count = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
		VkExtensionProperties* available_extensions = nullptr;
		if (extension_count > 0) {
			available_extensions = allocate<VkExtensionProperties>(g_ctx.alloc, extension_count);
			vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, available_extensions);
		}

		const char* enabled_layers[GFX_LAYERS_MAX];
		u32 enabled_layer_count = 0;

		for (i32 i = 0; i < array_size(g_instance_layers); ++i) {
			assert(enabled_layer_count < GFX_LAYERS_MAX && "Validation layer enables overflow.");

			const char* layer_name = g_instance_layers[i];
			if (is_layer_supported(layer_name, available_layers, layer_count)) {
				enabled_layers[enabled_layer_count++] = layer_name;
#ifdef USE_VALIDATION_LAYERS
				if (strcmp(layer_name, "VK_LAYER_KHRONOS_validation") == 0) {
					g_ctx.validation_enabled = true;
				}
				else if (strcmp(layer_name, "VK_LAYER_KHRONOS_synchronization2") == 0) {
					g_ctx.synchronization_validation_enabled = true;
				}
#endif
			}
			else {
				EDGE_LOG_WARN("Layer not supported: %s", layer_name);
			}
		}

		const char* enabled_extensions[GFX_INSTANCE_EXTENSIONS_MAX];
		u32 enabled_extension_count = 0;;

		for (i32 i = 0; i < array_size(g_instance_extensions); ++i) {
			assert(enabled_extension_count < GFX_INSTANCE_EXTENSIONS_MAX && "Extension enables overflow.");

			const char* ext_name = g_instance_extensions[i];
			if (is_extension_supported(ext_name, available_extensions, extension_count)) {
				enabled_extensions[enabled_extension_count++] = ext_name;
			}
			else {
				EDGE_LOG_WARN("Required instance extension not supported: %s\n", ext_name);
			}
		}

		if (available_layers) {
			deallocate(g_ctx.alloc, available_layers);
			available_layers = nullptr;
		}

		if (available_extensions) {
			deallocate(g_ctx.alloc, available_extensions);
			available_extensions = nullptr;
		}

		VkApplicationInfo app_info;
		app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		app_info.pApplicationName = "applicationname";
		app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0); // TODO: Generate
		app_info.pEngineName = "enginename";
		app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0); // TODO: Generate
		app_info.apiVersion = g_required_api_version;

		VkInstanceCreateInfo instance_info = {};
		instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		instance_info.pApplicationInfo = &app_info;
		instance_info.enabledLayerCount = enabled_layer_count;
		instance_info.ppEnabledLayerNames = enabled_layers;
		instance_info.enabledExtensionCount = enabled_extension_count;
		instance_info.ppEnabledExtensionNames = enabled_extensions;

#ifdef USE_VALIDATION_LAYER_FEATURES
		VkValidationFeaturesEXT validation_features = {};
		if (g_ctx.validation_enabled) {
			validation_features.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
			validation_features.enabledValidationFeatureCount = array_size(g_validation_features_enable);
			validation_features.pEnabledValidationFeatures = g_validation_features_enable;
			instance_info.pNext = &validation_features;
		}
#endif

		VkResult result = vkCreateInstance(&instance_info, &g_ctx.vk_alloc, &g_ctx.inst);
		if (result != VK_SUCCESS) {
			EDGE_LOG_FATAL("Failed to create Vulkan instance: %d\n", result);
			goto fatal_error;
		}

		volkLoadInstance(g_ctx.inst);

#ifdef USE_VALIDATION_LAYERS
		if (g_ctx.validation_enabled) {
			VkDebugUtilsMessengerCreateInfoEXT debug_info = {};
			debug_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
			debug_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
			debug_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
			debug_info.pfnUserCallback = debug_utils_messenger_cb;

			result = vkCreateDebugUtilsMessengerEXT(g_ctx.inst, &debug_info, &g_ctx.vk_alloc, &g_ctx.debug_msgr);
			if (result != VK_SUCCESS) {
				EDGE_LOG_WARN("Failed to create debug messenger: %d", result);
			}
		}
#endif

		return true;

	fatal_error:
		if (available_layers) {
			deallocate(g_ctx.alloc, available_layers);
		}

		if (available_extensions) {
			deallocate(g_ctx.alloc, available_extensions);
		}

		return VK_ERROR_UNKNOWN;
	}

	static bool select_adapter() {
		i32 best_score = -1;
		i32 selected_device = -1;

		u32 adapter_count = 0;
		vkEnumeratePhysicalDevices(g_ctx.inst, &adapter_count, NULL);
		adapter_count = min(adapter_count, GFX_ADAPTER_MAX);

		if (adapter_count == 0) {
			EDGE_LOG_FATAL("No Vulkan-capable GPUs found");
			return false;
		}

		VkPhysicalDevice adapters[GFX_ADAPTER_MAX];
		vkEnumeratePhysicalDevices(g_ctx.inst, &adapter_count, adapters);

		for (i32 i = 0; i < adapter_count; i++) {
			VkPhysicalDevice adapter = adapters[i];

			VkPhysicalDeviceProperties properties;
			vkGetPhysicalDeviceProperties(adapter, &properties);
			VkPhysicalDeviceFeatures features;
			vkGetPhysicalDeviceFeatures(adapter, &features);

			u32 extension_count = 0;
			vkEnumerateDeviceExtensionProperties(adapter, NULL, &extension_count, NULL);
			VkExtensionProperties* available_extensions = allocate<VkExtensionProperties>(g_ctx.alloc, extension_count);
			vkEnumerateDeviceExtensionProperties(adapter, NULL, &extension_count, available_extensions);

			i32 adapter_score = 0;

			if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
				adapter_score += 1000;
			}

			if (properties.apiVersion >= g_required_api_version) {
				adapter_score += 500;
			}

			const char* enabled_extensions[GFX_DEVICE_EXTENSIONS_MAX];
			u32 enabled_extension_count = 0;

			// Check extensions
			bool all_required_found = true;
			for (i32 j = 0; j < array_size(g_device_extensions); ++j) {
				assert(enabled_extension_count < GFX_DEVICE_EXTENSIONS_MAX && "Device extension enables overflow.");

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

			deallocate(g_ctx.alloc, available_extensions);

			if (!all_required_found) {
				continue;
			}

			VkQueueFamilyProperties queue_families[GFX_QUEUE_FAMILY_MAX];
			u32 queue_family_count = 0;

			vkGetPhysicalDeviceQueueFamilyProperties(adapter, &queue_family_count, NULL);
			queue_family_count = min(queue_family_count, GFX_QUEUE_FAMILY_MAX);
			vkGetPhysicalDeviceQueueFamilyProperties(adapter, &queue_family_count, queue_families);

			// Check surface support
			if (g_ctx.surf != VK_NULL_HANDLE) {
				VkBool32 surface_supported = VK_FALSE;
				for (i32 j = 0; j < queue_family_count; ++j) {
					adapter_score += queue_families[j].queueCount * 10;
					if (surface_supported == VK_FALSE) {
						vkGetPhysicalDeviceSurfaceSupportKHR(adapter, j, g_ctx.surf, &surface_supported);
					}
				}

				if (surface_supported == VK_FALSE) {
					continue;
				}
			}

			if (adapter_score > best_score) {
				best_score = adapter_score;
				selected_device = i;

				g_ctx.adapter = adapter;
				g_ctx.properties = properties;
				g_ctx.features = features;

				memcpy(g_ctx.enabled_extensions, enabled_extensions, enabled_extension_count * sizeof(const char*));
				g_ctx.enabled_extension_count = enabled_extension_count;

				memcpy(g_ctx.queue_families, queue_families, queue_family_count * sizeof(VkQueueFamilyProperties));
				g_ctx.queue_family_count = queue_family_count;
			}
		}

		if (selected_device < 0) {
			return false;
		}

		return true;
	}

	static bool device_init() {
		VkDeviceQueueCreateInfo queue_create_infos[GFX_QUEUE_FAMILY_MAX];

		f32 queue_priorities[32];
		for (i32 i = 0; i < 32; ++i) {
			queue_priorities[i] = 1.0f;
		}

		for (i32 i = 0; i < g_ctx.queue_family_count; ++i) {
			VkDeviceQueueCreateInfo* queue_create_info = &queue_create_infos[i];
			queue_create_info->sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queue_create_info->flags = 0;
			queue_create_info->pNext = nullptr;
			queue_create_info->queueFamilyIndex = i;
			queue_create_info->queueCount = g_ctx.queue_families[i].queueCount;
			queue_create_info->pQueuePriorities = queue_priorities;
		}

		VkPhysicalDeviceSynchronization2FeaturesKHR sync2_features = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR,
		};

		VkPhysicalDeviceBufferDeviceAddressFeaturesKHR buffer_device_address_features = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR
		};
		sync2_features.pNext = &buffer_device_address_features;

		VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic_rendering_features = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR
		};
		buffer_device_address_features.pNext = &dynamic_rendering_features;

		VkPhysicalDeviceDescriptorIndexingFeaturesEXT descriptor_indexing_features = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT
		};
		dynamic_rendering_features.pNext = &descriptor_indexing_features;

		VkPhysicalDeviceShaderFloat16Int8FeaturesKHR shader_float16_int8_features = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES_KHR
		};
		descriptor_indexing_features.pNext = &shader_float16_int8_features;

		void* last_feature = &sync2_features;

		VkPhysicalDeviceVulkan11Features features_vk11 = {};
		features_vk11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;

		VkPhysicalDeviceVulkan12Features features_vk12 = {};
		features_vk12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;

		VkPhysicalDeviceVulkan13Features features_vk13 = {};
		features_vk13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

		VkPhysicalDeviceVulkan14Features features_vk14 = {};
		features_vk14.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES;

		void* feature_chain = nullptr;
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

		VkPhysicalDeviceFeatures2 features2 = {};
		features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		features2.pNext = feature_chain;

		vkGetPhysicalDeviceFeatures2(g_ctx.adapter, &features2);

		VkDeviceCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		create_info.queueCreateInfoCount = g_ctx.queue_family_count;
		create_info.pQueueCreateInfos = queue_create_infos;
		create_info.enabledExtensionCount = g_ctx.enabled_extension_count;
		create_info.ppEnabledExtensionNames = g_ctx.enabled_extensions;
		create_info.pNext = &features2;

		VkResult result = vkCreateDevice(g_ctx.adapter, &create_info, &g_ctx.vk_alloc, &g_ctx.dev);
		if (result != VK_SUCCESS) {
			return false;
		}

		volkLoadDevice(g_ctx.dev);

		return true;
	}

	static bool allocator_init() {
		VmaVulkanFunctions vma_vulkan_func = {};
		vma_vulkan_func.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
		vma_vulkan_func.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

		VmaAllocatorCreateInfo create_info = {};
		create_info.pVulkanFunctions = &vma_vulkan_func;
		create_info.physicalDevice = g_ctx.adapter;
		create_info.device = g_ctx.dev;
		create_info.instance = g_ctx.inst;
		create_info.pAllocationCallbacks = &g_ctx.vk_alloc;
		create_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

		if (context_is_extension_enabled(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME)) {
			create_info.flags |= VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;
		}

		if (context_is_extension_enabled(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME)) {
			create_info.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
		}

		if (context_is_extension_enabled(VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME)) {
			create_info.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT;
		}

		if (context_is_extension_enabled(VK_KHR_BIND_MEMORY_2_EXTENSION_NAME)) {
			create_info.flags |= VMA_ALLOCATOR_CREATE_KHR_BIND_MEMORY2_BIT;
		}

		if (context_is_extension_enabled(VK_AMD_DEVICE_COHERENT_MEMORY_EXTENSION_NAME)) {
			create_info.flags |= VMA_ALLOCATOR_CREATE_AMD_DEVICE_COHERENT_MEMORY_BIT;
		}

		VkResult result = vmaCreateAllocator(&create_info, &g_ctx.vma);
		if (result != VK_SUCCESS) {
			return false;
		}

		return true;
	}

	bool context_init(const ContextCreateInfo* cteate_info) {
		if (!cteate_info || !cteate_info->alloc || !cteate_info->platform_context) {
			return false;
		}

		VkResult result = volkInitialize();
		if (result != VK_SUCCESS) {
			EDGE_LOG_ERROR("Failed to initialize volk: %d", result);
			return false;
		}

		g_ctx.alloc = cteate_info->alloc;

		// Init allocation callbacks
		g_ctx.vk_alloc.pUserData = (void*)g_ctx.alloc;
		g_ctx.vk_alloc.pfnAllocation = vk_alloc_cb;
		g_ctx.vk_alloc.pfnReallocation = vk_realloc_cb;
		g_ctx.vk_alloc.pfnFree = vk_free_cb;
		g_ctx.vk_alloc.pfnInternalAllocation = vk_internal_alloc_cb;
		g_ctx.vk_alloc.pfnInternalFree = vk_internal_free_cb;

		if (!instance_init()) {
			goto fatal_error;
		}

		// Surface initialize
#if defined(VK_USE_PLATFORM_WIN32_KHR)
		VkWin32SurfaceCreateInfoKHR surface_create_info;
		platform_context_get_surface(cteate_info->platform_context, &surface_create_info);

		result = vkCreateWin32SurfaceKHR(g_ctx.inst, &surface_create_info, &g_ctx.vk_alloc, &g_ctx.surf);
		if (result != VK_SUCCESS) {
			goto fatal_error;
		}
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
#endif

		if (!select_adapter()) {
			goto fatal_error;
		}

		result = vkGetPhysicalDeviceSurfaceFormatsKHR(g_ctx.adapter, g_ctx.surf, &g_ctx.surf_format_count, NULL);
		if (result != VK_SUCCESS) {
			goto fatal_error;
		}

		g_ctx.surf_format_count = min(g_ctx.surf_format_count, GFX_SURFACE_FORMAT_MAX);

		result = vkGetPhysicalDeviceSurfaceFormatsKHR(g_ctx.adapter, g_ctx.surf, &g_ctx.surf_format_count, g_ctx.surf_formats);
		if (result != VK_SUCCESS) {
			goto fatal_error;
		}

		result = vkGetPhysicalDeviceSurfacePresentModesKHR(g_ctx.adapter, g_ctx.surf, &g_ctx.surf_present_mode_count, NULL);
		if (result != VK_SUCCESS) {
			goto fatal_error;
		}

		g_ctx.surf_present_mode_count = min(g_ctx.surf_present_mode_count, GFX_PRESENT_MODES_MAX);

		result = vkGetPhysicalDeviceSurfacePresentModesKHR(g_ctx.adapter, g_ctx.surf, &g_ctx.surf_present_mode_count, g_ctx.surf_present_modes);
		if (result != VK_SUCCESS) {
			goto fatal_error;
		}

		if (!device_init()) {
			goto fatal_error;
		}

		if (!allocator_init()) {
			goto fatal_error;
		}

		return true;

	fatal_error:
		context_shutdown();
		return false;
	}

	void context_shutdown() {
		if (!g_ctx.alloc) {
			return;
		}

		if (g_ctx.vma != VK_NULL_HANDLE) {
			vmaDestroyAllocator(g_ctx.vma);
		}

		if (g_ctx.dev != VK_NULL_HANDLE) {
			vkDestroyDevice(g_ctx.dev, &g_ctx.vk_alloc);
		}

		if (g_ctx.surf != VK_NULL_HANDLE) {
			vkDestroySurfaceKHR(g_ctx.inst, g_ctx.surf, &g_ctx.vk_alloc);
		}

		if (g_ctx.debug_msgr != VK_NULL_HANDLE) {
			vkDestroyDebugUtilsMessengerEXT(g_ctx.inst, g_ctx.debug_msgr, &g_ctx.vk_alloc);
		}

		if (g_ctx.inst != VK_NULL_HANDLE) {
			vkDestroyInstance(g_ctx.inst, &g_ctx.vk_alloc);
		}

		volkFinalize();
	}

	bool context_is_extension_enabled(const char* name) {
		for (i32 i = 0; i < g_ctx.enabled_extension_count; ++i) {
			if (strcmp(g_ctx.enabled_extensions[i], name) == 0) {
				return true;
			}
		}
		return false;
	}

	const VkPhysicalDeviceProperties* get_adapter_props() {
		return &g_ctx.properties;
	}

	static i32 queue_calculate_family_score(const QueueRequest* request, u32 family_index) {
		if (!request) {
			return -1;
		}

		QueueCapsFlags caps = QUEUE_CAPS_NONE;

		VkQueueFamilyProperties props = g_ctx.queue_families[family_index];
		if (props.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			caps |= QUEUE_CAPS_GRAPHICS;
		}
		if (props.queueFlags & VK_QUEUE_COMPUTE_BIT) {
			caps |= QUEUE_CAPS_COMPUTE;
		}
		if (props.queueFlags & VK_QUEUE_TRANSFER_BIT) {
			caps |= QUEUE_CAPS_TRANSFER;
		}
		if (props.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) {
			caps |= QUEUE_CAPS_SPARSE_BINDING;
		}
		if (props.queueFlags & VK_QUEUE_PROTECTED_BIT) {
			caps |= QUEUE_CAPS_PROTECTED;
		}

		if (props.queueFlags & VK_QUEUE_VIDEO_DECODE_BIT_KHR) {
			caps |= QUEUE_CAPS_VIDEO_DECODE;
		}
		if (props.queueFlags & VK_QUEUE_VIDEO_ENCODE_BIT_KHR) {
			caps |= QUEUE_CAPS_VIDEO_ENCODE;
		}

		if (g_ctx.surf) {
			VkBool32 surface_supported = VK_FALSE;
			vkGetPhysicalDeviceSurfaceSupportKHR(g_ctx.adapter, family_index, g_ctx.surf, &surface_supported);
			if (surface_supported) {
				caps |= QUEUE_CAPS_PRESENT;
			}
		}

		if (request->strategy == QUEUE_SELECTION_STRATEGY_EXACT) {
			return (caps == request->required_caps) ? 1000 : -1;
		}

		if ((caps & request->required_caps) != request->required_caps) {
			return -1;
		}

		i32 score = 100;
		switch (request->strategy) {
		case QUEUE_SELECTION_STRATEGY_PREFER_DEDICATED: {
			score -= __builtin_popcount(caps & ~request->required_caps) * 10;
			break;
		}

		case QUEUE_SELECTION_STRATEGY_PREFER_SHARED: {
			score += __builtin_popcount(caps) * 5;
			break;
		}

		case QUEUE_SELECTION_STRATEGY_MINIMAL:
		default:
			break;
		}

		if (request->preferred_caps != QUEUE_CAPS_NONE) {
			if ((caps & request->preferred_caps) == request->preferred_caps) {
				score += 30;
			}
			else {
				score += __builtin_popcount(caps & request->preferred_caps) * 5;
			}
		}

		if (caps & QUEUE_CAPS_PRESENT) {
			score += 2;
		}

		return score;
	}

	bool get_queue(const QueueRequest* request, Queue* queue) {
		if (!request || !queue) {
			return false;
		}

		queue->queue_index = 0; // TODO: Select specific index

		i32 best_score = -1;

		for (i32 i = 0; i < g_ctx.queue_family_count; ++i) {
			i32 score = queue_calculate_family_score(request, i);
			if (score > best_score) {
				best_score = score;
				queue->family_index = 0;
			}
		}

		if (best_score < 0) {
			return false;
		}

		return true;
	}

	void release_queue(Queue* queue) {
		if (!queue) {
			return;
		}

		// TODO: release indices
	}

	VkQueue queue_handle(const Queue* queue) {
		if (!queue) {
			return VK_NULL_HANDLE;
		}

		VkQueue handle;
		vkGetDeviceQueue(g_ctx.dev, queue->family_index, queue->queue_index, &handle);

		return handle;
	}

	bool queue_submit(const Queue* queue, const Fence* fence, const VkSubmitInfo2KHR* submit_info) {
		if (!queue || !submit_info) {
			return false;
		}

		VkQueue queue_ = queue_handle(queue);
		if (queue_ == VK_NULL_HANDLE) {
			return false;
		}

		VkResult result = vkQueueSubmit2KHR(queue_, 1, submit_info, fence ? fence->handle : VK_NULL_HANDLE);
		return result == VK_SUCCESS;
	}

	bool queue_present(const Queue* queue, const VkPresentInfoKHR* present_info) {
		if (!queue || !present_info) {
			return false;
		}

		VkQueue queue_ = queue_handle(queue);
		if (queue_handle == VK_NULL_HANDLE) {
			return false;
		}

		VkResult result = vkQueuePresentKHR(queue_, present_info);
		return result == VK_SUCCESS;
	}

	void queue_wait_idle(const Queue* queue) {
		if (!queue) {
			return;
		}

		VkQueue queue_ = queue_handle(queue);
		if (queue_ == VK_NULL_HANDLE) {
			return;
		}

		vkQueueWaitIdle(queue_);
	}

	bool cmd_pool_create(const Queue* queue, CmdPool* cmd_pool) {
		if (!queue || !cmd_pool) {
			return false;
		}

		VkCommandPoolCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		create_info.queueFamilyIndex = queue->family_index;

		VkResult result = vkCreateCommandPool(g_ctx.dev, &create_info, &g_ctx.vk_alloc, &cmd_pool->handle);
		if (result != VK_SUCCESS) {
			return false;
		}

		return true;
	}

	void cmd_pool_destroy(CmdPool* command_pool) {
		if (!command_pool) {
			return;
		}

		if (command_pool->handle != VK_NULL_HANDLE) {
			vkDestroyCommandPool(g_ctx.dev, command_pool->handle, &g_ctx.vk_alloc);
		}
	}

	bool cmd_buf_create(const CmdPool* cmd_pool, CmdBuf* cmd_buf) {
		if (!cmd_pool || !cmd_buf) {
			return false;
		}

		VkCommandBufferAllocateInfo alloc_info = {};
		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.commandPool = cmd_pool->handle;
		alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_info.commandBufferCount = 1;

		VkResult result = vkAllocateCommandBuffers(g_ctx.dev, &alloc_info, &cmd_buf->handle);
		if (result != VK_SUCCESS) {
			return false;
		}

		cmd_buf->pool = cmd_pool;

		return true;
	}

	bool cmd_begin(const CmdBuf* cmd_buf) {
		if (!cmd_buf) {
			return false;
		}

		VkCommandBufferBeginInfo begin_info = {};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		VkResult result = vkBeginCommandBuffer(cmd_buf->handle, &begin_info);
		return result == VK_SUCCESS;
	}

	bool cmd_end(const CmdBuf* cmd_buf) {
		if (!cmd_buf) {
			return false;
		}

		VkResult result = vkEndCommandBuffer(cmd_buf->handle);
		return result == VK_SUCCESS;
	}

	bool cmd_reset(const CmdBuf* cmd_buf) {
		if (!cmd_buf) {
			return false;
		}

		VkResult result = vkResetCommandBuffer(cmd_buf->handle, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
		return result == VK_SUCCESS;
	}

	void cmd_write_timestamp(const CmdBuf* cmd_buf, const QueryPool* query, VkPipelineStageFlagBits2 stage, u32 query_index) {
		if (!cmd_buf || !query) {
			return;
		}

		vkCmdWriteTimestamp2KHR(cmd_buf->handle, stage, query->handle, query_index);
	}

	void cmd_bind_descriptor(const CmdBuf* cmd_buf, const PipelineLayout* layout, const DescriptorSet* descriptor, VkPipelineBindPoint bind_point) {
		if (!cmd_buf || !layout || !descriptor) {
			return;
		}

		vkCmdBindDescriptorSets(cmd_buf->handle, bind_point, layout->handle, 0u, 1u, &descriptor->handle, 0u, NULL);
	}

	void cmd_pipeline_barrier(const CmdBuf* cmd_buf, const PipelineBarrierBuilder* builder) {
		if (!cmd_buf || !builder) {
			return;
		}

		VkDependencyInfoKHR dependency_info = {
			.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,
			.pNext = NULL,
			.dependencyFlags = 0,
			.memoryBarrierCount = builder->memory_barrier_count,
			.pMemoryBarriers = builder->memory_barriers,
			.bufferMemoryBarrierCount = builder->buffer_barrier_count,
			.pBufferMemoryBarriers = builder->buffer_barriers,
			.imageMemoryBarrierCount = builder->image_barrier_count,
			.pImageMemoryBarriers = builder->image_barriers
		};

		vkCmdPipelineBarrier2KHR(cmd_buf->handle, &dependency_info);
	}

	void cmd_buf_destroy(CmdBuf* cmd_buf) {
		if (!cmd_buf) {
			return;
		}

		if (cmd_buf->handle != VK_NULL_HANDLE) {
			vkFreeCommandBuffers(g_ctx.dev, cmd_buf->pool->handle, 1, &cmd_buf->handle);
		}
	}

	void updete_descriptors(const VkWriteDescriptorSet* writes, u32 count) {
		vkUpdateDescriptorSets(g_ctx.dev, count, writes, 0u, NULL);
	}

	void cmd_reset_query(const CmdBuf* cmd_buf, const QueryPool* query, u32 first_query, u32 query_count) {
		if (!cmd_buf || !query) {
			return;
		}

		if (query->handle != VK_NULL_HANDLE) {
			vkCmdResetQueryPool(cmd_buf->handle, query->handle, first_query, query_count);
		}
	}

	bool query_pool_create(VkQueryType type, u32 count, QueryPool* query_pool) {
		if (!query_pool) {
			return false;
		}

		VkQueryPoolCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
		create_info.queryType = type;
		create_info.queryCount = count;

		if (type == VK_QUERY_TYPE_TIMESTAMP) {
			create_info.queryCount *= 2;
		}

		VkResult result = vkCreateQueryPool(g_ctx.dev, &create_info, &g_ctx.vk_alloc, &query_pool->handle);
		if (result != VK_SUCCESS) {
			return false;
		}

		query_pool->type = type;
		query_pool->max_query = create_info.queryCount;
		query_pool->host_reset_enabled = context_is_extension_enabled(VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME);

		return true;
	}

	void query_pool_reset(QueryPool* query_pool) {
		if (!query_pool) {
			return;
		}

		vkResetQueryPoolEXT(g_ctx.dev, query_pool->handle, 0, query_pool->max_query);
	}

	bool query_pool_get_data(const QueryPool* query_pool, u32 first_query, void* out_data) {
		if (!query_pool || !out_data) {
			return false;
		}

		VkResult result = VK_SUCCESS;
		switch (query_pool->type)
		{
		case VK_QUERY_TYPE_OCCLUSION: {
			result = vkGetQueryPoolResults(g_ctx.dev, query_pool->handle, first_query, 1, sizeof(u64), out_data, sizeof(u64), VK_QUERY_RESULT_64_BIT);
			break;
		}
		case VK_QUERY_TYPE_TIMESTAMP: {
			result = vkGetQueryPoolResults(g_ctx.dev, query_pool->handle, first_query * 2, 2, sizeof(u64) * 2, out_data, sizeof(u64), VK_QUERY_RESULT_64_BIT);
			break;
		}
		default:
			break;
		}

		return result == VK_SUCCESS;
	}

	void query_pool_destroy(QueryPool* query_pool) {
		if (!query_pool) {
			return;
		}

		if (query_pool->handle != VK_NULL_HANDLE) {
			vkDestroyQueryPool(g_ctx.dev, query_pool->handle, &g_ctx.vk_alloc);
		}
	}

	void descriptor_layout_builder_add_binding(VkDescriptorSetLayoutBinding binding, VkDescriptorBindingFlags flags, DescriptorLayoutBuilder* builder) {
		if (!builder) {
			return;
		}

		u32 index = builder->binding_count++;
		builder->bindings[index] = binding;
		builder->binding_flags[index] = flags;
	}

	bool descriptor_set_layout_create(const DescriptorLayoutBuilder* builder, DescriptorSetLayout* descriptor_set_layout) {
		if (!descriptor_set_layout) {
			return false;
		}

		VkDescriptorSetLayoutBindingFlagsCreateInfoEXT binding_flags_create_info = {};
		binding_flags_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
		binding_flags_create_info.bindingCount = builder->binding_count;
		binding_flags_create_info.pBindingFlags = builder->binding_flags;

		VkDescriptorSetLayoutCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		create_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
		create_info.bindingCount = builder->binding_count;
		create_info.pBindings = builder->bindings;
		create_info.pNext = &binding_flags_create_info;

		VkResult result = vkCreateDescriptorSetLayout(g_ctx.dev, &create_info, &g_ctx.vk_alloc, &descriptor_set_layout->handle);
		if (result != VK_SUCCESS) {
			return false;
		}

		for (i32 i = 0; i < builder->binding_count; ++i) {
			VkDescriptorSetLayoutBinding binding = builder->bindings[i];
			descriptor_set_layout->descriptor_sizes[binding.descriptorType] += binding.descriptorCount;
		}

		return true;
	}

	void descriptor_set_layout_destroy(DescriptorSetLayout* descriptor_set_layout) {
		if (!descriptor_set_layout) {
			return;
		}

		if (descriptor_set_layout->handle != VK_NULL_HANDLE) {
			vkDestroyDescriptorSetLayout(g_ctx.dev, descriptor_set_layout->handle, &g_ctx.vk_alloc);
		}
	}

	bool descriptor_pool_create(u32* descriptor_sizes, DescriptorPool* descriptor_pool) {
		if (!descriptor_sizes || !descriptor_pool) {
			return false;
		}

		VkDescriptorPoolSize pool_sizes[DESCRIPTOR_SIZES_COUNT];
		for (i32 i = 0; i < DESCRIPTOR_SIZES_COUNT; ++i) {
			VkDescriptorPoolSize* pool_size = &pool_sizes[i];
			pool_size->type = (VkDescriptorType)i;
			pool_size->descriptorCount = descriptor_sizes[i] ? descriptor_sizes[i] : 1;
		}

		VkDescriptorPoolCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		create_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
		create_info.maxSets = 64; // TODO: Set it manually
		create_info.poolSizeCount = DESCRIPTOR_SIZES_COUNT;
		create_info.pPoolSizes = pool_sizes;

		VkResult result = vkCreateDescriptorPool(g_ctx.dev, &create_info, &g_ctx.vk_alloc, &descriptor_pool->handle);
		if (result != VK_SUCCESS) {
			return false;
		}

		memcpy(descriptor_pool->descriptor_sizes, descriptor_sizes, DESCRIPTOR_SIZES_COUNT * sizeof(u32));

		return true;
	}

	void descriptor_pool_destroy(DescriptorPool* descriptor_pool) {
		if (!descriptor_pool) {
			return;
		}

		if (descriptor_pool->handle != VK_NULL_HANDLE) {
			vkDestroyDescriptorPool(g_ctx.dev, descriptor_pool->handle, &g_ctx.vk_alloc);
		}
	}

	bool descriptor_set_create(const DescriptorPool* pool, const DescriptorSetLayout* layouts, DescriptorSet* set) {
		if (!pool || !layouts || !set) {
			return false;
		}

		VkDescriptorSetAllocateInfo alloc_info = {};
		alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc_info.descriptorPool = pool->handle;
		alloc_info.descriptorSetCount = 1;
		alloc_info.pSetLayouts = &layouts->handle;

		VkResult result = vkAllocateDescriptorSets(g_ctx.dev, &alloc_info, &set->handle);
		if (result != VK_SUCCESS) {
			return false;
		}

		set->pool = pool;

		return true;
	}

	void descriptor_set_destroy(DescriptorSet* set) {
		if (!set) {
			return;
		}

		vkFreeDescriptorSets(g_ctx.dev, set->pool->handle, 1, &set->handle);
	}

	void pipeline_layout_builder_add_range(PipelineLayoutBuilder* builder, VkShaderStageFlags stage_flags, u32 offset, u32 size) {
		if (!builder) {
			return;
		}

		VkPushConstantRange constant_range = {
			.stageFlags = stage_flags,
			.offset = offset,
			.size = size
		};

		builder->constant_ranges[builder->constant_range_count++] = constant_range;
	}

	void pipeline_layout_builder_add_layout(PipelineLayoutBuilder* builder, const DescriptorSetLayout* layout) {
		if (!layout || !builder) {
			return;
		}

		builder->descriptor_layouts[builder->descriptor_layout_count++] = layout->handle;
	}

	bool pipeline_layout_create(const PipelineLayoutBuilder* builder, PipelineLayout* pipeline_layout) {
		if (!builder || !pipeline_layout) {
			return false;
		}

		VkPipelineLayoutCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		create_info.setLayoutCount = builder->descriptor_layout_count;
		create_info.pSetLayouts = builder->descriptor_layouts;
		create_info.pushConstantRangeCount = builder->constant_range_count;
		create_info.pPushConstantRanges = builder->constant_ranges;

		VkResult result = vkCreatePipelineLayout(g_ctx.dev, &create_info, &g_ctx.vk_alloc, &pipeline_layout->handle);
		if (result != VK_SUCCESS) {
			return false;
		}

		return true;
	}

	void pipeline_layout_destroy(PipelineLayout* pipeline_layout) {
		if (!pipeline_layout) {
			return;
		}

		if (pipeline_layout->handle != VK_NULL_HANDLE) {
			vkDestroyPipelineLayout(g_ctx.dev, pipeline_layout->handle, &g_ctx.vk_alloc);
		}
	}

	bool pipeline_cache_create(const u8* data, size_t data_size, PipelineCache* pipeline_cache) {
		if (!pipeline_cache) {
			return false;
		}

		VkPipelineCacheCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
		create_info.initialDataSize = data_size;
		create_info.pInitialData = data;

		VkResult result = vkCreatePipelineCache(g_ctx.dev, &create_info, &g_ctx.vk_alloc, &pipeline_cache->handle);
		return result == VK_SUCCESS;
	}

	void pipeline_cache_destroy(PipelineCache* pipeline_cache) {
		if (!pipeline_cache) {
			return;
		}

		if (pipeline_cache->handle != VK_NULL_HANDLE) {
			vkDestroyPipelineCache(g_ctx.dev, pipeline_cache->handle, &g_ctx.vk_alloc);
		}
	}

	bool shader_module_create(const u32* code, size_t size, ShaderModule* shader_module) {
		if (!shader_module) {
			return false;
		}

		VkShaderModuleCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		create_info.codeSize = size;
		create_info.pCode = code;

		VkResult result = vkCreateShaderModule(g_ctx.dev, &create_info, &g_ctx.vk_alloc, &shader_module->handle);
		return result == VK_SUCCESS;
	}

	void shader_module_destroy(ShaderModule* shader_module) {
		if (!shader_module) {
			return;
		}

		if (shader_module->handle != VK_NULL_HANDLE) {
			vkDestroyShaderModule(g_ctx.dev, shader_module->handle, &g_ctx.vk_alloc);
		}
	}

	bool pipeline_graphics_create(const GraphicsPipelineCreateInfo* create_info, Pipeline* pipeline) {
		if (!create_info || !pipeline) {
			return false;
		}

		return true;
	}

	bool pipeline_compute_create(const ComputePipelineCreateInfo* create_info, Pipeline* pipeline) {
		if (!create_info || !pipeline) {
			return false;
		}

		VkComputePipelineCreateInfo pipeline_create_info = {};
		pipeline_create_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		pipeline_create_info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		pipeline_create_info.stage.module = create_info->shader_module->handle;
		pipeline_create_info.stage.pName = "main";
		pipeline_create_info.layout = create_info->layout->handle;

		VkResult result = vkCreateComputePipelines(g_ctx.dev, create_info->cache ? create_info->cache->handle : VK_NULL_HANDLE, 1, &pipeline_create_info, &g_ctx.vk_alloc, &pipeline->handle);
		if (result != VK_SUCCESS) {
			return false;
		}

		pipeline->bind_point = VK_PIPELINE_BIND_POINT_COMPUTE;

		return true;
	}

	void pipeline_destroy(Pipeline* pipeline) {
		if (!pipeline) {
			return;
		}

		if (pipeline->handle != VK_NULL_HANDLE) {
			vkDestroyPipeline(g_ctx.dev, pipeline->handle, &g_ctx.vk_alloc);
		}
	}

	bool swapchain_create(const SwapchainCreateInfo* create_info, Swapchain* swapchain) {
		if (!create_info || !swapchain) {
			return false;
		}

		VkPresentModeKHR present_mode = create_info->vsync_enable ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;
		VkPresentModeKHR present_mode_priority_list[3] = {
	#ifdef VK_USE_PLATFORM_ANDROID_KHR
				VK_PRESENT_MODE_FIFO_KHR, VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR
	#else
				VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_FIFO_KHR
	#endif
		};

		VkSurfaceCapabilitiesKHR surf_caps;
		VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_ctx.adapter, g_ctx.surf, &surf_caps);
		if (result != VK_SUCCESS) {
			return false;
		}

		u32 queue_family_indices[GFX_QUEUE_FAMILY_MAX];
		for (i32 i = 0; i < g_ctx.queue_family_count; ++i) {
			queue_family_indices[i] = i;
		}

		VkSwapchainCreateInfoKHR swapchain_create_info = {};
		swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		swapchain_create_info.surface = g_ctx.surf;
		swapchain_create_info.minImageCount = 2;
		swapchain_create_info.minImageCount = clamp(swapchain_create_info.minImageCount, surf_caps.minImageCount, surf_caps.maxImageCount ? surf_caps.maxImageCount : 16);

		VkSurfaceFormatKHR requested_surface_format;
		requested_surface_format.format = create_info->preferred_format;
		requested_surface_format.colorSpace = create_info->preferred_color_space;

		VkSurfaceFormatKHR selected_surface_format = choose_surface_format(requested_surface_format, g_ctx.surf_formats, g_ctx.surf_format_count, create_info->hdr_enable);
		swapchain_create_info.imageFormat = selected_surface_format.format;
		swapchain_create_info.imageColorSpace = selected_surface_format.colorSpace;

		swapchain_create_info.imageExtent = choose_suitable_extent(swapchain->extent, &surf_caps);
		swapchain_create_info.imageArrayLayers = 1;
		swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		swapchain_create_info.imageSharingMode = g_ctx.queue_family_count > 1 ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
		swapchain_create_info.queueFamilyIndexCount = g_ctx.queue_family_count;
		swapchain_create_info.pQueueFamilyIndices = queue_family_indices;
		swapchain_create_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
		swapchain_create_info.compositeAlpha = choose_suitable_composite_alpha(VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR, surf_caps.supportedCompositeAlpha);
		swapchain_create_info.presentMode = choose_suitable_present_mode(present_mode, g_ctx.surf_present_modes, g_ctx.surf_present_mode_count,
			present_mode_priority_list, array_size(present_mode_priority_list));
		swapchain_create_info.oldSwapchain = swapchain->handle;

		result = vkCreateSwapchainKHR(g_ctx.dev, &swapchain_create_info, &g_ctx.vk_alloc, &swapchain->handle);
		if (result != VK_SUCCESS) {
			return false;
		}

		swapchain->format = selected_surface_format.format;
		swapchain->color_space = selected_surface_format.colorSpace;
		swapchain->image_count = swapchain_create_info.minImageCount;
		swapchain->extent = swapchain_create_info.imageExtent;
		swapchain->present_mode = swapchain_create_info.presentMode;
		swapchain->composite_alpha = swapchain_create_info.compositeAlpha;

		return true;
	}

	bool swapchain_update(Swapchain* swapchain) {
		if (!swapchain) {
			return false;
		}

		VkSurfaceCapabilitiesKHR surf_caps;
		VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_ctx.adapter, g_ctx.surf, &surf_caps);
		if (result != VK_SUCCESS) {
			return false;
		}

		u32 queue_family_indices[GFX_QUEUE_FAMILY_MAX];
		for (i32 i = 0; i < g_ctx.queue_family_count; ++i) {
			queue_family_indices[i] = i;
		}

		VkSwapchainCreateInfoKHR create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		create_info.surface = g_ctx.surf;
		create_info.minImageCount = swapchain->image_count;
		create_info.imageFormat = swapchain->format;
		create_info.imageColorSpace = swapchain->color_space;
		create_info.imageExtent = choose_suitable_extent(swapchain->extent, &surf_caps);
		create_info.imageArrayLayers = 1;
		create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		create_info.imageSharingMode = g_ctx.queue_family_count > 1 ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
		create_info.queueFamilyIndexCount = g_ctx.queue_family_count;
		create_info.pQueueFamilyIndices = queue_family_indices;
		create_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
		create_info.compositeAlpha = swapchain->composite_alpha;
		create_info.presentMode = swapchain->present_mode;
		create_info.oldSwapchain = swapchain->handle;

		result = vkCreateSwapchainKHR(g_ctx.dev, &create_info, &g_ctx.vk_alloc, &swapchain->handle);
		if (result != VK_SUCCESS) {
			return false;
		}

		swapchain->extent = create_info.imageExtent;

		return true;
	}

	bool swapchain_is_outdated(const Swapchain* swapchain) {
		if (!swapchain) {
			return false;
		}

		VkSurfaceCapabilitiesKHR surf_caps;
		VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_ctx.adapter, g_ctx.surf, &surf_caps);
		if (result != VK_SUCCESS) {
			return false;
		}

		if (surf_caps.currentExtent.width == 0xFFFFFFFF || surf_caps.currentExtent.height == 0xFFFFFFFF) {
			return false;
		}

		return swapchain->extent.width != surf_caps.currentExtent.width || swapchain->extent.height != surf_caps.currentExtent.height;
	}

	bool swapchain_get_images(const Swapchain* swapchain, Image* image_out) {
		if (!swapchain || !image_out) {
			return false;
		}

		VkImage images[GFX_SWAPCHAIN_IMAGES_MAX];

		u32 image_count;
		VkResult result = vkGetSwapchainImagesKHR(g_ctx.dev, swapchain->handle, &image_count, images);
		if (result != VK_SUCCESS || image_count != swapchain->image_count) {
			return false;
		}

		for (i32 i = 0; i < swapchain->image_count; ++i) {
			Image* image = &image_out[i];
			image->handle = images[i];
			image->extent.width = swapchain->extent.width;
			image->extent.height = swapchain->extent.height;
			image->extent.depth = 1;
			image->level_count = 1;
			image->layer_count = 1;
			image->face_count = 1;
			image->usage_flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
			image->format = swapchain->format;
			image->layout = VK_IMAGE_LAYOUT_UNDEFINED;
		}

		return true;
	}

	bool swapchain_acquire_next_image(const Swapchain* swapchain, u64 timeout, const Semaphore* semaphore, u32* next_image_idx) {
		if (!swapchain || !semaphore || !next_image_idx) {
			return false;
		}

		return vkAcquireNextImageKHR(g_ctx.dev, swapchain->handle, timeout, semaphore->handle, VK_NULL_HANDLE, next_image_idx) == VK_SUCCESS;
	}

	void swapchain_destroy(Swapchain* swapchain) {
		if (!swapchain) {
			return;
		}

		if (swapchain->handle != VK_NULL_HANDLE) {
			vkDestroySwapchainKHR(g_ctx.dev, swapchain->handle, &g_ctx.vk_alloc);
		}
	}

	void device_memory_setup(DeviceMemory* mem) {
		if (!mem) {
			return;
		}

		VkMemoryPropertyFlags memory_properties;
		vmaGetAllocationMemoryProperties(g_ctx.vma, mem->handle, &memory_properties);

		mem->coherent = memory_properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		mem->persistent = mem->info.pMappedData != nullptr;
	}

	bool device_memory_is_mapped(const DeviceMemory* mem) {
		if (!mem || mem->handle == VK_NULL_HANDLE) {
			return false;
		}

		return mem->info.pMappedData != nullptr;
	}

	void* device_memory_map(DeviceMemory* mem) {
		if (!mem || mem->handle == VK_NULL_HANDLE) {
			return nullptr;
		}

		if (!mem->persistent && !mem->info.pMappedData) {
			void* mappedMemory;
			VkResult result = vmaMapMemory(g_ctx.vma, mem->handle, &mappedMemory);
			if (result != VK_SUCCESS) {
				return nullptr;
			}
			mem->info.pMappedData = mappedMemory;
		}

		return mem->info.pMappedData;
	}

	void device_memory_unmap(DeviceMemory* mem) {
		if (!mem || mem->handle == VK_NULL_HANDLE) {
			return;
		}

		if (!mem->persistent && mem->info.pMappedData != nullptr) {
			vmaUnmapMemory(g_ctx.vma, mem->handle);
			mem->info.pMappedData = nullptr;
		}
	}

	void device_memory_flush(DeviceMemory* mem, VkDeviceSize offset, VkDeviceSize size) {
		if (!mem || mem->handle == VK_NULL_HANDLE) {
			return;
		}

		if (!mem->coherent) {
			VkResult result = vmaFlushAllocation(g_ctx.vma, mem->handle, offset, size);
			// TODO: fatal error
		}
	}

	void device_memory_update(DeviceMemory* mem, const void* data, VkDeviceSize size, VkDeviceSize offset) {
		if (!mem || mem->handle == VK_NULL_HANDLE) {
			return;
		}

		if (mem->persistent) {
			memcpy((i8*)mem->info.pMappedData + offset, data, size);
			device_memory_flush(mem, offset, size);
		}
		else {
			device_memory_map(mem);

			memcpy((i8*)mem->info.pMappedData + offset, data, size);
			device_memory_unmap(mem);

			device_memory_flush(mem, offset, size);
		}
	}

	bool image_create(const ImageCreateInfo* create_info, Image* image) {
		if (!create_info || !image) {
			return false;
		}

		u32 max_dimension = (create_info->extent.width > create_info->extent.height) ? create_info->extent.width : create_info->extent.height;
		u32 max_mip_levels = compute_max_mip_level(max_dimension);

		u32 queue_family_indices[GFX_QUEUE_FAMILY_MAX];
		for (i32 i = 0; i < g_ctx.queue_family_count; ++i) {
			queue_family_indices[i] = i;
		}

		VmaAllocationCreateInfo allocation_create_info = {};
		allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO;

		VkImageCreateInfo image_create_info = {};
		image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		image_create_info.flags = (create_info->face_count == 6u) ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : VK_IMAGE_CREATE_EXTENDED_USAGE_BIT;
		image_create_info.imageType = (create_info->extent.depth > 1u) ? VK_IMAGE_TYPE_3D : (create_info->extent.height > 1u) ? VK_IMAGE_TYPE_2D : VK_IMAGE_TYPE_1D;
		image_create_info.format = create_info->format;
		image_create_info.extent = create_info->extent;
		image_create_info.mipLevels = clamp(create_info->level_count, 1u, max_mip_levels);
		image_create_info.arrayLayers = clamp(create_info->layer_count * create_info->face_count, 1u, g_ctx.properties.limits.maxImageArrayLayers);
		image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
		image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
		image_create_info.usage = create_info->usage_flags;
		image_create_info.sharingMode = g_ctx.queue_family_count > 1 ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
		image_create_info.queueFamilyIndexCount = g_ctx.queue_family_count;
		image_create_info.pQueueFamilyIndices = queue_family_indices;
		image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		bool is_color_attachment = create_info->usage_flags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		bool is_depth_attachment = create_info->usage_flags & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		if (is_color_attachment || is_depth_attachment) {
			allocation_create_info.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
			allocation_create_info.priority = 1.0f;
		}

		VkResult result = vmaCreateImage(g_ctx.vma, &image_create_info, &allocation_create_info, &image->handle, &image->memory.handle, &image->memory.info);
		if (result != VK_SUCCESS) {
			return false;
		}

		device_memory_setup(&image->memory);

		image->extent = create_info->extent;
		image->level_count = create_info->level_count;
		image->layer_count = create_info->layer_count;
		image->face_count = create_info->face_count;
		image->usage_flags = create_info->usage_flags;
		image->format = create_info->format;
		image->layout = VK_IMAGE_LAYOUT_UNDEFINED;

		return true;
	}

	void image_destroy(Image* image) {
		if (!image) {
			return;
		}

		if (image->handle != VK_NULL_HANDLE && image->memory.handle != VK_NULL_HANDLE) {
			vmaDestroyImage(g_ctx.vma, image->handle, image->memory.handle);
		}
	}

	bool image_view_create(const Image* image, VkImageViewType type, VkImageSubresourceRange subresource_range, ImageView* view) {
		if (!image || !view) {
			return false;
		}

		VkImageViewCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		create_info.image = image->handle;
		create_info.viewType = type;
		create_info.format = image->format;
		create_info.components.r = VK_COMPONENT_SWIZZLE_R;
		create_info.components.g = VK_COMPONENT_SWIZZLE_G;
		create_info.components.b = VK_COMPONENT_SWIZZLE_B;
		create_info.components.a = VK_COMPONENT_SWIZZLE_A;
		create_info.subresourceRange = subresource_range;

		VkResult result = vkCreateImageView(g_ctx.dev, &create_info, &g_ctx.vk_alloc, &view->handle);
		if (result != VK_SUCCESS) {
			return false;
		}

		view->type = type;
		view->range = subresource_range;

		return true;
	}

	void image_view_destroy(ImageView* view) {
		if (!view) {
			return;
		}

		if (view->handle != VK_NULL_HANDLE) {
			vkDestroyImageView(g_ctx.dev, view->handle, &g_ctx.vk_alloc);
		}
	}

	bool buffer_create(const BufferCreateInfo* create_info, Buffer* buffer) {
		if (!create_info || !buffer) {
			return false;
		}

		u32 queue_family_indices[GFX_QUEUE_FAMILY_MAX];
		for (i32 i = 0; i < g_ctx.queue_family_count; ++i) {
			queue_family_indices[i] = i;
		}

		VmaAllocationCreateInfo allocation_create_info = {};
		allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO;

		VkBufferCreateInfo buffer_create_info = {};
		buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buffer_create_info.sharingMode = g_ctx.queue_family_count > 1 ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
		buffer_create_info.queueFamilyIndexCount = g_ctx.queue_family_count;
		buffer_create_info.pQueueFamilyIndices = queue_family_indices;

		u32 minimal_alignment = 1;
		if (create_info->flags & BUFFER_FLAG_DYNAMIC) {
			allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
			allocation_create_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
				VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
		}
		else if (create_info->flags & BUFFER_FLAG_READBACK) {
			buffer_create_info.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			allocation_create_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
		}
		else if (create_info->flags & BUFFER_FLAG_STAGING) {
			buffer_create_info.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
			allocation_create_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
		}

		if (create_info->flags & BUFFER_FLAG_DEVICE_ADDRESS) {
			buffer_create_info.usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		}

		if (create_info->flags & BUFFER_FLAG_UNIFORM) {
			buffer_create_info.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			minimal_alignment = lcm(g_ctx.properties.limits.minUniformBufferOffsetAlignment, g_ctx.properties.limits.nonCoherentAtomSize);
		}
		else if (create_info->flags & BUFFER_FLAG_STORAGE) {
			buffer_create_info.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			minimal_alignment = max(minimal_alignment, (u32)g_ctx.properties.limits.minStorageBufferOffsetAlignment);
		}
		else if (create_info->flags & BUFFER_FLAG_VERTEX) {
			buffer_create_info.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			minimal_alignment = max(minimal_alignment, 4u);
		}
		else if (create_info->flags & BUFFER_FLAG_INDEX) {
			buffer_create_info.usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			minimal_alignment = max(minimal_alignment, 1u);
		}
		else if (create_info->flags & BUFFER_FLAG_INDIRECT) {
			buffer_create_info.usage |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		}
		else if (create_info->flags & BUFFER_FLAG_ACCELERATION_BUILD) {
			buffer_create_info.usage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		}
		else if (create_info->flags & BUFFER_FLAG_ACCELERATION_STORE) {
			buffer_create_info.usage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		}
		else if (create_info->flags & BUFFER_FLAG_SHADER_BINDING_TABLE) {
			buffer_create_info.usage |= VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		}

		buffer_create_info.size = align_up((u32)create_info->size, minimal_alignment);

		VkResult result = vmaCreateBuffer(g_ctx.vma, &buffer_create_info, &allocation_create_info, &buffer->handle, &buffer->memory.handle, &buffer->memory.info);
		if (result != VK_SUCCESS) {
			return false;
		}

		device_memory_setup(&buffer->memory);

		if (create_info->flags & BUFFER_FLAG_DEVICE_ADDRESS) {
			VkBufferDeviceAddressInfo device_address_info = {};
			device_address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
			device_address_info.buffer = buffer->handle;
			buffer->address = vkGetBufferDeviceAddress(g_ctx.dev, &device_address_info);
		}

		buffer->flags = create_info->flags;

		return true;
	}

	void buffer_destroy(Buffer* buffer) {
		if (!buffer) {
			return;
		}

		if (buffer->handle != VK_NULL_HANDLE && buffer->memory.handle != VK_NULL_HANDLE) {
			vmaDestroyBuffer(g_ctx.vma, buffer->handle, buffer->memory.handle);
		}
	}

	bool semaphore_create(VkSemaphoreType type, u64 value, Semaphore* semaphore) {
		if (!semaphore) {
			return false;
		}

		VkSemaphoreTypeCreateInfo type_create_info = {};
		type_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
		type_create_info.semaphoreType = type;
		type_create_info.initialValue = value;

		VkSemaphoreCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		create_info.pNext = &type_create_info;

		VkResult result = vkCreateSemaphore(g_ctx.dev, &create_info, &g_ctx.vk_alloc, &semaphore->handle);
		if (result != VK_SUCCESS) {
			return false;
		}

		semaphore->type = type;
		semaphore->value = value;

		return true;
	}

	void semaphore_destroy(Semaphore* semaphore) {
		if (!semaphore) {
			return;
		}

		vkDestroySemaphore(g_ctx.dev, semaphore->handle, &g_ctx.vk_alloc);
	}

	bool fence_create(VkFenceCreateFlags flags, Fence* fence) {
		if (!fence) {
			return false;
		}

		VkFenceCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		create_info.flags = flags;

		VkResult result = vkCreateFence(g_ctx.dev, &create_info, &g_ctx.vk_alloc, &fence->handle);
		if (result != VK_SUCCESS) {
			return false;
		}

		return true;
	}

	bool fence_wait(const Fence* fence, u64 timeout) {
		if (!fence) {
			return false;
		}

		VkResult result = vkWaitForFences(g_ctx.dev, 1, &fence->handle, VK_TRUE, timeout);
		if (result != VK_SUCCESS) {
			return false;
		}

		return true;
	}

	void fence_reset(const Fence* fence) {
		if (!fence) {
			return;
		}

		vkResetFences(g_ctx.dev, 1, &fence->handle);
	}

	void fence_destroy(Fence* fence) {
		if (!fence) {
			return;
		}

		vkDestroyFence(g_ctx.dev, fence->handle, &g_ctx.vk_alloc);
	}

	bool pipeline_barrier_add_memory(PipelineBarrierBuilder* builder, VkPipelineStageFlags2 src_stage_mask, VkAccessFlags2 src_access_mask,
		VkPipelineStageFlags2 dst_stage_mask, VkAccessFlags2 dst_access_mask) {
		if (builder->memory_barrier_count >= MEMORY_BARRIERS_MAX) {
			return false;
		}

		VkMemoryBarrier2* barrier = &builder->memory_barriers[builder->memory_barrier_count++];
		barrier->sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
		barrier->pNext = nullptr;
		barrier->srcStageMask = src_stage_mask;
		barrier->srcAccessMask = src_access_mask;
		barrier->dstStageMask = dst_stage_mask;
		barrier->dstAccessMask = dst_access_mask;

		return true;
	}

	bool pipeline_barrier_add_buffer(PipelineBarrierBuilder* builder, const Buffer* buffer, VkPipelineStageFlags2 src_stage_mask,
		VkAccessFlags2 src_access_mask, VkPipelineStageFlags2 dst_stage_mask, VkAccessFlags2 dst_access_mask, VkDeviceSize offset, VkDeviceSize size) {
		if (builder->buffer_barrier_count >= BUFFER_BARRIERS_MAX || !buffer) {
			return false;
		}

		VkBufferMemoryBarrier2* barrier = &builder->buffer_barriers[builder->buffer_barrier_count++];
		barrier->sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
		barrier->pNext = nullptr;
		barrier->srcStageMask = src_stage_mask;
		barrier->srcAccessMask = src_access_mask;
		barrier->dstStageMask = dst_stage_mask;
		barrier->dstAccessMask = dst_access_mask;
		barrier->srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier->dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier->buffer = buffer->handle;
		barrier->offset = offset;
		barrier->size = size;

		return true;
	}

	static void image_get_stage_and_acces(VkImageLayout layout, VkPipelineStageFlags2KHR* out_stage, VkAccessFlags2* out_access) {
		switch (layout)
		{
		case VK_IMAGE_LAYOUT_UNDEFINED:
			*out_stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
			*out_access = VK_ACCESS_2_NONE;
			break;
		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			*out_stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
			*out_access = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			*out_stage = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
			*out_access = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
			*out_stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
			*out_access = VK_ACCESS_2_SHADER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			*out_stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
			*out_access = VK_ACCESS_2_SHADER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			*out_stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
			*out_access = VK_ACCESS_2_TRANSFER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			*out_stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
			*out_access = VK_ACCESS_2_TRANSFER_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
			*out_stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
			*out_access = VK_ACCESS_2_NONE;
			break;
		default:
			*out_stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
			*out_access = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
			break;
		}
	}

	bool pipeline_barrier_add_image(PipelineBarrierBuilder* builder, const Image* image, VkImageLayout new_layout, VkImageSubresourceRange subresource_range) {
		if (builder->image_barrier_count >= IMAGE_BARRIERS_MAX || !image) {
			return false;
		}

		VkImageMemoryBarrier2* barrier = &builder->image_barriers[builder->image_barrier_count++];
		barrier->sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		barrier->pNext = nullptr;
		image_get_stage_and_acces(image->layout, &barrier->srcStageMask, &barrier->srcAccessMask);
		image_get_stage_and_acces(new_layout, &barrier->dstStageMask, &barrier->dstAccessMask);
		barrier->oldLayout = image->layout;
		barrier->newLayout = new_layout;
		barrier->srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier->dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier->image = image->handle;
		barrier->subresourceRange = subresource_range;

		return true;
	}

	void pipeline_barrier_builder_reset(PipelineBarrierBuilder* builder) {
		if (!builder) {
			return;
		}

		builder->memory_barrier_count = 0;
		builder->buffer_barrier_count = 0;
		builder->image_barrier_count = 0;
		builder->dependency_flags = 0;
	}
}