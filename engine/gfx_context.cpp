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
		const Allocator* alloc = nullptr;
		VkAllocationCallbacks vk_alloc = {};

		VkInstance inst = VK_NULL_HANDLE;
		VkDebugUtilsMessengerEXT debug_msgr = VK_NULL_HANDLE;

		bool validation_enabled = false;
		bool synchronization_validation_enabled = false;

		VkSurfaceKHR surf = VK_NULL_HANDLE;
		VkSurfaceFormatKHR surf_formats[GFX_SURFACE_FORMAT_MAX] = {};
		u32 surf_format_count = 0u;
		VkPresentModeKHR surf_present_modes[GFX_PRESENT_MODES_MAX] = {};
		u32 surf_present_mode_count = 0u;

		VkPhysicalDevice adapter = VK_NULL_HANDLE;

		VkPhysicalDeviceProperties properties = {};
		VkPhysicalDeviceFeatures features = {};

		const char* enabled_extensions[GFX_DEVICE_EXTENSIONS_MAX] = {};
		u32 enabled_extension_count = 0u;

		VkQueueFamilyProperties queue_families[GFX_QUEUE_FAMILY_MAX] = {};
		u32 queue_family_count = 0u;

		VkDevice dev = VK_NULL_HANDLE;

		VmaAllocator vma = VK_NULL_HANDLE;
	};

	static Context g_ctx = {};

	static void* VKAPI_CALL vk_alloc_cb(void* user_data, size_t size, size_t alignment, VkSystemAllocationScope allocation_scope) {
		const Allocator* alloc = (const Allocator*)user_data;
		return alloc->malloc(size);
	}

	static void VKAPI_CALL vk_free_cb(void* user_data, void* memory) {
		const Allocator* alloc = (const Allocator*)user_data;
		alloc->free(memory);
	}

	static void* VKAPI_CALL vk_realloc_cb(void* user_data, void* original, size_t size, size_t alignment, VkSystemAllocationScope allocation_scope) {
		const Allocator* alloc = (const Allocator*)user_data;
		return alloc->realloc(original, size);
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

	static u32 compute_max_mip_level(u32 size) {
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
			available_layers = g_ctx.alloc->allocate_array<VkLayerProperties>(layer_count);
			vkEnumerateInstanceLayerProperties(&layer_count, available_layers);
		}

		u32 extension_count = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
		VkExtensionProperties* available_extensions = nullptr;
		if (extension_count > 0) {
			available_extensions = g_ctx.alloc->allocate_array<VkExtensionProperties>(extension_count);
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
			g_ctx.alloc->deallocate_array(available_layers, layer_count);
			available_layers = nullptr;
		}

		if (available_extensions) {
			g_ctx.alloc->deallocate_array(available_extensions, extension_count);
			available_extensions = nullptr;
		}

		VkApplicationInfo app_info = {
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pApplicationName = "applicationname",
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0), // TODO: Generate
            .pEngineName = "enginename",
            .engineVersion = VK_MAKE_VERSION(1, 0, 0), // TODO: Generate
            .apiVersion = g_required_api_version
        };

		VkInstanceCreateInfo instance_info = {
			.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.pApplicationInfo = &app_info,
			.enabledLayerCount = enabled_layer_count,
			.ppEnabledLayerNames = enabled_layers,
			.enabledExtensionCount = enabled_extension_count,
			.ppEnabledExtensionNames = enabled_extensions
		};

#ifdef USE_VALIDATION_LAYER_FEATURES
		const VkValidationFeaturesEXT validation_features = {
			.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
			.enabledValidationFeatureCount = (u32)array_size(g_validation_features_enable),
			.pEnabledValidationFeatures = g_validation_features_enable
		};

		if (g_ctx.validation_enabled) {
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
			const VkDebugUtilsMessengerCreateInfoEXT debug_info = {
				.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
				.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
				.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
				.pfnUserCallback = debug_utils_messenger_cb
			};

			result = vkCreateDebugUtilsMessengerEXT(g_ctx.inst, &debug_info, &g_ctx.vk_alloc, &g_ctx.debug_msgr);
			if (result != VK_SUCCESS) {
				EDGE_LOG_WARN("Failed to create debug messenger: %d", result);
			}
		}
#endif

		return true;

	fatal_error:
		if (available_layers) {
			g_ctx.alloc->deallocate_array(available_layers, layer_count);
		}

		if (available_extensions) {
			g_ctx.alloc->deallocate_array(available_extensions, extension_count);
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
			VkExtensionProperties* available_extensions = g_ctx.alloc->allocate_array<VkExtensionProperties>(extension_count);
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

			g_ctx.alloc->deallocate_array(available_extensions, extension_count);

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
			queue_create_infos[i] = {
				.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				.queueFamilyIndex = (u32)i,
				.queueCount = g_ctx.queue_families[i].queueCount,
				.pQueuePriorities = queue_priorities
			};
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

		VkPhysicalDeviceTimelineSemaphoreFeaturesKHR timeline_semaphore_features = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES_KHR
		};
		shader_float16_int8_features.pNext = &timeline_semaphore_features;

		void* last_feature = &sync2_features;

		VkPhysicalDeviceVulkan11Features features_vk11 = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES
		};

		VkPhysicalDeviceVulkan12Features features_vk12 = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES
		};

		VkPhysicalDeviceVulkan13Features features_vk13 = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES
		};

		VkPhysicalDeviceVulkan14Features features_vk14 = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES
		};

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

		VkPhysicalDeviceFeatures2 features2 = {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
			.pNext = feature_chain
		};

		vkGetPhysicalDeviceFeatures2(g_ctx.adapter, &features2);

		const VkDeviceCreateInfo create_info = {
			.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			.pNext = &features2,
			.queueCreateInfoCount = g_ctx.queue_family_count,
			.pQueueCreateInfos = queue_create_infos,
			.enabledExtensionCount = g_ctx.enabled_extension_count,
			.ppEnabledExtensionNames = g_ctx.enabled_extensions
		};

		VkResult result = vkCreateDevice(g_ctx.adapter, &create_info, &g_ctx.vk_alloc, &g_ctx.dev);
		if (result != VK_SUCCESS) {
			return false;
		}

		volkLoadDevice(g_ctx.dev);

		return true;
	}

	static bool allocator_init() {
		VmaVulkanFunctions vma_vulkan_func = {
			.vkGetInstanceProcAddr = vkGetInstanceProcAddr,
			.vkGetDeviceProcAddr = vkGetDeviceProcAddr
		};

		VmaAllocatorCreateInfo create_info = {
			.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
			.physicalDevice = g_ctx.adapter,
			.device = g_ctx.dev,
			.pAllocationCallbacks = &g_ctx.vk_alloc,
			.pVulkanFunctions = &vma_vulkan_func,
			.instance = g_ctx.inst
		};

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
		window_get_surface(cteate_info->window, &surface_create_info);

		result = vkCreateWin32SurfaceKHR(g_ctx.inst, &surface_create_info, &g_ctx.vk_alloc, &g_ctx.surf);
		if (result != VK_SUCCESS) {
			goto fatal_error;
		}
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
        VkAndroidSurfaceCreateInfoKHR surface_create_info;
        window_get_surface(cteate_info->window, &surface_create_info);

        result = vkCreateAndroidSurfaceKHR(g_ctx.inst, &surface_create_info, &g_ctx.vk_alloc, &g_ctx.surf);
        if (result != VK_SUCCESS) {
            goto fatal_error;
        }
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

	void context_set_object_name(const char* name, VkObjectType type, u64 handle) noexcept {
		VkDebugUtilsObjectNameInfoEXT name_info = { 
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
			.objectType = type,
			.objectHandle = handle,
			.pObjectName = name
		};
		vkSetDebugUtilsObjectNameEXT(g_ctx.dev, &name_info);
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

	static i32 queue_calculate_family_score(QueueRequest request, u32 family_index) {
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

		if (request.strategy == QUEUE_SELECTION_STRATEGY_EXACT) {
			return (caps == request.required_caps) ? 1000 : -1;
		}

		if ((caps & request.required_caps) != request.required_caps) {
			return -1;
		}

		i32 score = 100;
		switch (request.strategy) {
		case QUEUE_SELECTION_STRATEGY_PREFER_DEDICATED: {
			score -= __builtin_popcount(caps & ~request.required_caps) * 10;
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

		if (request.preferred_caps != QUEUE_CAPS_NONE) {
			if ((caps & request.preferred_caps) == request.preferred_caps) {
				score += 30;
			}
			else {
				score += __builtin_popcount(caps & request.preferred_caps) * 5;
			}
		}

		if (caps & QUEUE_CAPS_PRESENT) {
			score += 2;
		}

		return score;
	}

	bool Queue::request(QueueRequest create_info) noexcept {
		queue_index = 0; // TODO: Select specific index

		i32 best_score = -1;

		for (i32 i = 0; i < g_ctx.queue_family_count; ++i) {
			i32 score = queue_calculate_family_score(create_info, i);
			if (score > best_score) {
				best_score = score;
				family_index = i;
			}
		}

		if (best_score < 0) {
			return false;
		}

		return true;
	}

	void Queue::release() noexcept {
		// TODO: release indices
	}

	VkQueue Queue::get_handle() noexcept {
		if (family_index == -1) {
			return VK_NULL_HANDLE;
		}

		VkQueue handle;
		vkGetDeviceQueue(g_ctx.dev, family_index, queue_index, &handle);

		return handle;
	}

	bool Queue::submit(Fence fence, const VkSubmitInfo2KHR* submit_info) noexcept {
		if (!submit_info) {
			return false;
		}

		VkQueue queue_ = get_handle();
		if (queue_ == VK_NULL_HANDLE) {
			return false;
		}

		return vkQueueSubmit2KHR(queue_, 1, submit_info, fence ? fence.handle : VK_NULL_HANDLE) == VK_SUCCESS;
	}

	bool Queue::present(const VkPresentInfoKHR* present_info) noexcept {
		if (!present_info) {
			return false;
		}

		VkQueue queue_ = get_handle();
		if (queue_ == VK_NULL_HANDLE) {
			return false;
		}

		return vkQueuePresentKHR(queue_, present_info) == VK_SUCCESS;
	}

	void Queue::wait_idle() noexcept {
		if (family_index == -1) {
			return;
		}

		VkQueue queue_ = get_handle();
		if (queue_ == VK_NULL_HANDLE) {
			return;
		}

		vkQueueWaitIdle(queue_);
	}

	bool CmdPool::create(Queue queue) noexcept {
		if (!queue) {
			return false;
		}

		const VkCommandPoolCreateInfo create_info = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
			.queueFamilyIndex = (u32)queue.family_index
		};

		VkResult result = vkCreateCommandPool(g_ctx.dev, &create_info, &g_ctx.vk_alloc, &handle);
		if (result != VK_SUCCESS) {
			return false;
		}

		return true;
	}

	void CmdPool::destroy() noexcept {
		if (handle == VK_NULL_HANDLE) {
			return;
		}
		vkDestroyCommandPool(g_ctx.dev, handle, &g_ctx.vk_alloc);
		handle = VK_NULL_HANDLE;
	}

	bool CmdBuf::create(CmdPool cmd_pool) noexcept {
		if (!cmd_pool) {
			return false;
		}

		const VkCommandBufferAllocateInfo alloc_info = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = cmd_pool.handle,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1
		};

		VkResult result = vkAllocateCommandBuffers(g_ctx.dev, &alloc_info, &handle);
		if (result != VK_SUCCESS) {
			return false;
		}

		pool = cmd_pool;
		return true;
	}

	void CmdBuf::destroy() noexcept {
		if (handle == VK_NULL_HANDLE) {
			return;
		}
		vkFreeCommandBuffers(g_ctx.dev, pool.handle, 1, &handle);
		handle = VK_NULL_HANDLE;
	}

	bool CmdBuf::begin() noexcept {
		assert(handle != VK_NULL_HANDLE && "CmdBuf handle is null");
		const VkCommandBufferBeginInfo begin_info = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
		};

		return vkBeginCommandBuffer(handle, &begin_info) == VK_SUCCESS;
	}

	bool CmdBuf::end() noexcept {
		assert(handle != VK_NULL_HANDLE && "CmdBuf handle is null");
		return vkEndCommandBuffer(handle) == VK_SUCCESS;
	}

	static void make_color(uint32_t color, f32(&out_color)[4]) {
		out_color[0] = ((color >> 24) & 0xFF) / 255.0f;
		out_color[1] = ((color >> 16) & 0xFF) / 255.0f;
		out_color[2] = ((color >> 8) & 0xFF) / 255.0f;
		out_color[3] = (color & 0xFF) / 255.0f;
	}

	void CmdBuf::begin_marker(const char* name, u32 color) noexcept {
		assert(handle != VK_NULL_HANDLE && "CmdBuf handle is null");

		VkDebugMarkerMarkerInfoEXT marker_info = {
			.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT,
			.pNext = name
		};
		make_color(color, marker_info.color);

		vkCmdDebugMarkerBeginEXT(handle, &marker_info);
	}

	void CmdBuf::end_marker() noexcept {
		assert(handle != VK_NULL_HANDLE && "CmdBuf handle is null");
		vkCmdDebugMarkerEndEXT(handle);
	}

	bool CmdBuf::reset() noexcept {
		assert(handle != VK_NULL_HANDLE && "CmdBuf handle is null");
		return vkResetCommandBuffer(handle, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT) == VK_SUCCESS;
	}

	void CmdBuf::reset_query(QueryPool query, u32 first_query, u32 query_count) noexcept {

	}

	void CmdBuf::write_timestamp(QueryPool query, VkPipelineStageFlagBits2 stage, u32 query_index) noexcept {
		assert(handle != VK_NULL_HANDLE && "CmdBuf handle is null");
		assert(query && "QueryPool handle is null");
		vkCmdWriteTimestamp2KHR(handle, stage, query.handle, query_index);
	}

	void CmdBuf::bind_descriptor(PipelineLayout layout, DescriptorSet descriptor, VkPipelineBindPoint bind_point) noexcept {
		assert(handle != VK_NULL_HANDLE && "CmdBuf handle is null");
		assert(layout && "PipelineLayout handle is null");
		assert(descriptor && "DescriptorSet handle is null");
		vkCmdBindDescriptorSets(handle, bind_point, layout.handle, 0u, 1u, &descriptor.handle, 0u, NULL);
	}

	void CmdBuf::pipeline_barrier(const PipelineBarrierBuilder& builder) noexcept {
		assert(handle != VK_NULL_HANDLE && "CmdBuf handle is null");

		const VkDependencyInfoKHR dependency_info = {
			.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,
			.memoryBarrierCount = builder.memory_barrier_count,
			.pMemoryBarriers = builder.memory_barriers,
			.bufferMemoryBarrierCount = builder.buffer_barrier_count,
			.pBufferMemoryBarriers = builder.buffer_barriers,
			.imageMemoryBarrierCount = builder.image_barrier_count,
			.pImageMemoryBarriers = builder.image_barriers
		};

		vkCmdPipelineBarrier2KHR(handle, &dependency_info);
	}

	void CmdBuf::begin_rendering(const VkRenderingInfoKHR& rendering_info) noexcept {
		assert(handle != VK_NULL_HANDLE && "CmdBuf handle is null");
		vkCmdBeginRenderingKHR(handle, &rendering_info);
	}

	void CmdBuf::end_rendering() noexcept {
		assert(handle != VK_NULL_HANDLE && "CmdBuf handle is null");
		vkCmdEndRenderingKHR(handle);
	}

	void CmdBuf::bind_index_buffer(Buffer buffer, VkIndexType type) noexcept {
		assert(handle != VK_NULL_HANDLE && "CmdBuf handle is null");
		assert(buffer && "Buffer handle is null");
		vkCmdBindIndexBuffer(handle, buffer.handle, 0, type);
	}

	void CmdBuf::bind_pipeline(Pipeline pipeline) noexcept {
		assert(handle != VK_NULL_HANDLE && "CmdBuf handle is null");
		assert(pipeline && "Pipeline handle is null");
		vkCmdBindPipeline(handle, pipeline.bind_point, pipeline.handle);
	}

	void CmdBuf::set_viewport(VkViewport viewport) noexcept {
		assert(handle != VK_NULL_HANDLE && "CmdBuf handle is null");
		vkCmdSetViewport(handle, 0u, 1u, &viewport);
	}

	void CmdBuf::set_viewport(f32 x, f32 y, f32 width, f32 height, f32 depth_min, f32 depth_max) noexcept {
		assert(handle != VK_NULL_HANDLE && "CmdBuf handle is null");

		VkViewport viewport = {
			.x = x,
			.y = y,
			.width = width,
			.height = height,
			.minDepth = depth_min,
			.maxDepth = depth_max
		};
		vkCmdSetViewport(handle, 0u, 1u, &viewport);
	}

	void CmdBuf::set_scissor(VkRect2D rect) noexcept {
		assert(handle != VK_NULL_HANDLE && "CmdBuf handle is null");
		vkCmdSetScissor(handle, 0u, 1u, &rect);
	}

	void CmdBuf::set_scissor(i32 off_x, i32 off_y, u32 width, u32 height) noexcept {
		assert(handle != VK_NULL_HANDLE && "CmdBuf handle is null");
		VkRect2D rect = {
			.offset = {.x = off_x, .y = off_y },
			.extent = {.width = width, .height = height }
		};
		vkCmdSetScissor(handle, 0u, 1u, &rect);
	}

	void CmdBuf::push_constants(PipelineLayout layout, VkShaderStageFlags flags, u32 offset, u32 size, const void* data) noexcept {
		assert(handle != VK_NULL_HANDLE && "CmdBuf handle is null");
		assert(layout && "PipelineLayout handle is null");
		vkCmdPushConstants(handle, layout.handle, flags, offset, size, data);
	}

	void CmdBuf::draw_indexed(u32 idx_cnt, u32 inst_cnt, u32 first_idx, i32 vtx_offset, u32 first_inst) noexcept {
		assert(handle != VK_NULL_HANDLE && "CmdBuf handle is null");
		vkCmdDrawIndexed(handle, idx_cnt, inst_cnt, first_idx, vtx_offset, first_inst);
	}

	void updete_descriptors(const VkWriteDescriptorSet* writes, u32 count) {
		vkUpdateDescriptorSets(g_ctx.dev, count, writes, 0u, NULL);
	}

	bool QueryPool::create(VkQueryType type, u32 count) noexcept {
		const VkQueryPoolCreateInfo create_info = {
			.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
			.queryType = type,
			.queryCount = type == VK_QUERY_TYPE_TIMESTAMP ? count * 2 : count
		};

		VkResult result = vkCreateQueryPool(g_ctx.dev, &create_info, &g_ctx.vk_alloc, &handle);
		if (result != VK_SUCCESS) {
			return false;
		}

		this->type = type;
		max_query = create_info.queryCount;
		host_reset_enabled = context_is_extension_enabled(VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME);

		return true;
	}

	void QueryPool::destroy() noexcept {
		if (handle == VK_NULL_HANDLE) {
			return;
		}
		vkDestroyQueryPool(g_ctx.dev, handle, &g_ctx.vk_alloc);
		handle = VK_NULL_HANDLE;
	}

	void QueryPool::reset() noexcept {
		assert(handle != VK_NULL_HANDLE && "QueryPool handle is null");
		vkResetQueryPoolEXT(g_ctx.dev, handle, 0, max_query);
	}

	bool QueryPool::get_data(u32 first_query, void* out_data) noexcept {
		assert(handle != VK_NULL_HANDLE && "QueryPool handle is null");

		VkResult result = VK_SUCCESS;
		switch (type)
		{
		case VK_QUERY_TYPE_OCCLUSION: {
			result = vkGetQueryPoolResults(g_ctx.dev, handle, first_query, 1, sizeof(u64), out_data, sizeof(u64), VK_QUERY_RESULT_64_BIT);
			break;
		}
		case VK_QUERY_TYPE_TIMESTAMP: {
			result = vkGetQueryPoolResults(g_ctx.dev, handle, first_query * 2, 2, sizeof(u64) * 2, out_data, sizeof(u64), VK_QUERY_RESULT_64_BIT);
			break;
		}
		default:
			break;
		}

		return result == VK_SUCCESS;
	}

	void DescriptorLayoutBuilder::add_binding(VkDescriptorSetLayoutBinding binding, VkDescriptorBindingFlags flags) noexcept {
		u32 index = binding_count++;
		bindings[index] = binding;
		binding_flags[index] = flags;
	}

	bool DescriptorSetLayout::create(const DescriptorLayoutBuilder& builder) noexcept {
		const VkDescriptorSetLayoutBindingFlagsCreateInfoEXT binding_flags_create_info = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
			.bindingCount = builder.binding_count,
			.pBindingFlags = builder.binding_flags
		};

		const VkDescriptorSetLayoutCreateInfo create_info = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.pNext = &binding_flags_create_info,
			.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
			.bindingCount = builder.binding_count,
			.pBindings = builder.bindings,
		};

		VkResult result = vkCreateDescriptorSetLayout(g_ctx.dev, &create_info, &g_ctx.vk_alloc, &handle);
		if (result != VK_SUCCESS) {
			return false;
		}

		for (i32 i = 0; i < builder.binding_count; ++i) {
			VkDescriptorSetLayoutBinding binding = builder.bindings[i];
			descriptor_sizes[binding.descriptorType] += binding.descriptorCount;
		}

		return true;
	}

	void DescriptorSetLayout::destroy() noexcept {
		if (handle == VK_NULL_HANDLE) {
			return;
		}
		vkDestroyDescriptorSetLayout(g_ctx.dev, handle, &g_ctx.vk_alloc);
		handle = VK_NULL_HANDLE;
	}

	bool DescriptorPool::create(u32* descriptor_sizes) noexcept {
		if (!descriptor_sizes) {
			return false;
		}

		VkDescriptorPoolSize pool_sizes[DESCRIPTOR_SIZES_COUNT];
		for (i32 i = 0; i < DESCRIPTOR_SIZES_COUNT; ++i) {
			VkDescriptorPoolSize* pool_size = &pool_sizes[i];
			pool_size->type = (VkDescriptorType)i;
			pool_size->descriptorCount = descriptor_sizes[i] ? descriptor_sizes[i] : 1;
		}

		const VkDescriptorPoolCreateInfo create_info = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
			.maxSets = 64, // TODO: Set it manually
			.poolSizeCount = DESCRIPTOR_SIZES_COUNT,
			.pPoolSizes = pool_sizes
		};

		VkResult result = vkCreateDescriptorPool(g_ctx.dev, &create_info, &g_ctx.vk_alloc, &handle);
		if (result != VK_SUCCESS) {
			return false;
		}

		memcpy(this->descriptor_sizes, descriptor_sizes, DESCRIPTOR_SIZES_COUNT * sizeof(u32));

		return true;
	}

	void DescriptorPool::destroy() noexcept {
		if (handle == VK_NULL_HANDLE) {
			return;
		}
		vkDestroyDescriptorPool(g_ctx.dev, handle, &g_ctx.vk_alloc);
		handle = VK_NULL_HANDLE;
	}

	bool DescriptorSet::create(DescriptorPool pool, const DescriptorSetLayout* layouts) noexcept {
		if (!pool || !layouts) {
			return false;
		}

		const VkDescriptorSetAllocateInfo alloc_info = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.descriptorPool = pool.handle,
			.descriptorSetCount = 1,
			.pSetLayouts = &layouts->handle
		};

		VkResult result = vkAllocateDescriptorSets(g_ctx.dev, &alloc_info, &handle);
		if (result != VK_SUCCESS) {
			return false;
		}

		this->pool = pool;
		return true;
	}

	void DescriptorSet::destroy() noexcept {
		if (handle == VK_NULL_HANDLE) {
			return;
		}
		vkFreeDescriptorSets(g_ctx.dev, pool.handle, 1, &handle);
		handle = VK_NULL_HANDLE;
	}

	void PipelineLayoutBuilder::add_range(VkShaderStageFlags stage_flags, u32 offset, u32 size) noexcept {
		VkPushConstantRange constant_range = {
			.stageFlags = stage_flags,
			.offset = offset,
			.size = size
		};

		constant_ranges[constant_range_count++] = constant_range;
	}

	void PipelineLayoutBuilder::add_layout(DescriptorSetLayout layout) noexcept {
		if (!layout) {
			return;
		}

		descriptor_layouts[descriptor_layout_count++] = layout.handle;
	}

	bool PipelineLayout::create(const PipelineLayoutBuilder& builder) noexcept {
		const VkPipelineLayoutCreateInfo create_info = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.setLayoutCount = builder.descriptor_layout_count,
			.pSetLayouts = builder.descriptor_layouts,
			.pushConstantRangeCount = builder.constant_range_count,
			.pPushConstantRanges = builder.constant_ranges
		};

		VkResult result = vkCreatePipelineLayout(g_ctx.dev, &create_info, &g_ctx.vk_alloc, &handle);
		if (result != VK_SUCCESS) {
			return false;
		}

		return true;
	}

	void PipelineLayout::destroy() noexcept {
		if (handle == VK_NULL_HANDLE) {
			return;
		}
		vkDestroyPipelineLayout(g_ctx.dev, handle, &g_ctx.vk_alloc);
		handle = VK_NULL_HANDLE;
	}

	bool PipelineCache::create(const u8* data, usize data_size) noexcept {
		const VkPipelineCacheCreateInfo create_info = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
			.initialDataSize = data_size,
			.pInitialData = data
		};

		return vkCreatePipelineCache(g_ctx.dev, &create_info, &g_ctx.vk_alloc, &handle) == VK_SUCCESS;
	}

	void PipelineCache::destroy() noexcept {
		if (handle == VK_NULL_HANDLE) {
			return;
		}
		vkDestroyPipelineCache(g_ctx.dev, handle, &g_ctx.vk_alloc);
		handle = VK_NULL_HANDLE;
	}

	bool ShaderModule::create(const u32* code, usize size) noexcept {
		const VkShaderModuleCreateInfo create_info = {
			.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			.codeSize = size,
			.pCode = code
		};

		return vkCreateShaderModule(g_ctx.dev, &create_info, &g_ctx.vk_alloc, &handle) == VK_SUCCESS;
	}

	void ShaderModule::destroy() noexcept {
		if (handle == VK_NULL_HANDLE) {
			return;
		}
		vkDestroyShaderModule(g_ctx.dev, handle, &g_ctx.vk_alloc);
		handle = VK_NULL_HANDLE;
	}

	bool Pipeline::create(const VkGraphicsPipelineCreateInfo* create_info) noexcept {
		if (!create_info) {
			return false;
		}

		VkResult result = vkCreateGraphicsPipelines(g_ctx.dev, VK_NULL_HANDLE, 1u, create_info, &g_ctx.vk_alloc, &handle);
		if (result != VK_SUCCESS) {
			return false;
		}

		bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;
		return true;
	}

	bool Pipeline::create(ComputePipelineCreateInfo create_info) noexcept {
		const VkComputePipelineCreateInfo pipeline_create_info = {
			.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
			.stage = {
				.stage = VK_SHADER_STAGE_COMPUTE_BIT,
				.module = create_info.shader_module.handle,
				.pName = "main"
			},
			.layout = create_info.layout.handle
		};

		VkResult result = vkCreateComputePipelines(g_ctx.dev, create_info.cache ? create_info.cache.handle : VK_NULL_HANDLE, 1, &pipeline_create_info, &g_ctx.vk_alloc, &handle);
		if (result != VK_SUCCESS) {
			return false;
		}

		bind_point = VK_PIPELINE_BIND_POINT_COMPUTE;
		return true;
	}

	void Pipeline::destroy() noexcept {
		if (handle == VK_NULL_HANDLE) {
			return;
		}
		vkDestroyPipeline(g_ctx.dev, handle, &g_ctx.vk_alloc);
		handle = VK_NULL_HANDLE;
	}

	bool Swapchain::create(SwapchainCreateInfo create_info) noexcept {
		VkPresentModeKHR present_mode = create_info.vsync_enable ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;
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

		VkSurfaceFormatKHR requested_surface_format;
		requested_surface_format.format = create_info.preferred_format;
		requested_surface_format.colorSpace = create_info.preferred_color_space;

		VkSurfaceFormatKHR selected_surface_format = choose_surface_format(requested_surface_format, g_ctx.surf_formats, g_ctx.surf_format_count, create_info.hdr_enable);

		const VkSwapchainCreateInfoKHR swapchain_create_info = {
			.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			.surface = g_ctx.surf,
			.minImageCount = clamp(2u, surf_caps.minImageCount, surf_caps.maxImageCount ? surf_caps.maxImageCount : 16),
			.imageFormat = selected_surface_format.format,
			.imageColorSpace = selected_surface_format.colorSpace,
			.imageExtent = choose_suitable_extent(extent, &surf_caps),
			.imageArrayLayers = 1,
			.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			.imageSharingMode = g_ctx.queue_family_count > 1 ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = g_ctx.queue_family_count,
			.pQueueFamilyIndices = queue_family_indices,
			.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
			.compositeAlpha = choose_suitable_composite_alpha(VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR, surf_caps.supportedCompositeAlpha),
			.presentMode = choose_suitable_present_mode(present_mode, g_ctx.surf_present_modes, g_ctx.surf_present_mode_count,
			present_mode_priority_list, array_size(present_mode_priority_list)),
			.oldSwapchain = handle
		};

		result = vkCreateSwapchainKHR(g_ctx.dev, &swapchain_create_info, &g_ctx.vk_alloc, &handle);
		if (result != VK_SUCCESS) {
			return false;
		}

		format = selected_surface_format.format;
		color_space = selected_surface_format.colorSpace;
		image_count = swapchain_create_info.minImageCount;
		extent = swapchain_create_info.imageExtent;
		this->present_mode = swapchain_create_info.presentMode;
		composite_alpha = swapchain_create_info.compositeAlpha;

		return true;
	}

	void Swapchain::destroy() noexcept {
		if (handle == VK_NULL_HANDLE) {
			return;
		}
		vkDestroySwapchainKHR(g_ctx.dev, handle, &g_ctx.vk_alloc);
		handle = VK_NULL_HANDLE;
	}

	bool Swapchain::update() noexcept {
		assert(handle != VK_NULL_HANDLE && "Swapchain handle is null");

		VkSurfaceCapabilitiesKHR surf_caps;
		VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_ctx.adapter, g_ctx.surf, &surf_caps);
		if (result != VK_SUCCESS) {
			return false;
		}

		u32 queue_family_indices[GFX_QUEUE_FAMILY_MAX];
		for (i32 i = 0; i < g_ctx.queue_family_count; ++i) {
			queue_family_indices[i] = i;
		}

		const VkSwapchainCreateInfoKHR create_info = {
			.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			.surface = g_ctx.surf,
			.minImageCount = image_count,
			.imageFormat = format,
			.imageColorSpace = color_space,
			.imageExtent = choose_suitable_extent(extent, &surf_caps),
			.imageArrayLayers = 1,
			.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			.imageSharingMode = g_ctx.queue_family_count > 1 ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = g_ctx.queue_family_count,
			.pQueueFamilyIndices = queue_family_indices,
			.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
			.compositeAlpha = composite_alpha,
			.presentMode = present_mode,
			.oldSwapchain = handle
		};

		result = vkCreateSwapchainKHR(g_ctx.dev, &create_info, &g_ctx.vk_alloc, &handle);
		if (result != VK_SUCCESS) {
			return false;
		}

		extent = create_info.imageExtent;
		return true;
	}

	bool Swapchain::is_outdated() noexcept {
		assert(handle != VK_NULL_HANDLE && "Swapchain handle is null");

		VkSurfaceCapabilitiesKHR surf_caps;
		VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_ctx.adapter, g_ctx.surf, &surf_caps);
		if (result != VK_SUCCESS) {
			return false;
		}

		if (surf_caps.currentExtent.width == 0xFFFFFFFF || surf_caps.currentExtent.height == 0xFFFFFFFF) {
			return false;
		}

		return extent.width != surf_caps.currentExtent.width || extent.height != surf_caps.currentExtent.height;
	}

	bool Swapchain::get_images(Image* image_out) noexcept {
		assert(handle != VK_NULL_HANDLE && "Swapchain handle is null");

		VkImage images[GFX_SWAPCHAIN_IMAGES_MAX];

		u32 image_count;
		VkResult result = vkGetSwapchainImagesKHR(g_ctx.dev, handle, &image_count, images);
		if (result != VK_SUCCESS || image_count != image_count) {
			return false;
		}

		for (i32 i = 0; i < image_count; ++i) {
			Image& image = image_out[i];
			image.handle = images[i];
			image.extent.width = extent.width;
			image.extent.height = extent.height;
			image.extent.depth = 1;
			image.level_count = 1;
			image.layer_count = 1;
			image.face_count = 1;
			image.usage_flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
			image.format = format;
			image.layout = VK_IMAGE_LAYOUT_UNDEFINED;
		}

		return true;
	}

	bool Swapchain::acquire_next_image(u64 timeout, Semaphore semaphore, u32* next_image_idx) noexcept {
		assert(handle != VK_NULL_HANDLE && "Swapchain handle is null");
		assert(semaphore && "Semaphore handle is null");
		assert(next_image_idx && "next_image_ids is null");
		return vkAcquireNextImageKHR(g_ctx.dev, handle, timeout, semaphore.handle, VK_NULL_HANDLE, next_image_idx) == VK_SUCCESS;
	}

	void DeviceMemory::setup() noexcept {
		VkMemoryPropertyFlags memory_properties;
		vmaGetAllocationMemoryProperties(g_ctx.vma, handle, &memory_properties);

		coherent = memory_properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		persistent = mapped != nullptr;
	}

	bool DeviceMemory::is_mapped() noexcept {
		return mapped != nullptr;
	}

	void* DeviceMemory::map() noexcept {
		if (!persistent && !mapped) {
			void* mappedMemory;
			VkResult result = vmaMapMemory(g_ctx.vma, handle, &mappedMemory);
			if (result != VK_SUCCESS) {
				return nullptr;
			}
			mapped = mappedMemory;
		}

		return mapped;
	}

	void DeviceMemory::unmap() noexcept {
		if (!persistent && mapped != nullptr) {
			vmaUnmapMemory(g_ctx.vma, handle);
			mapped = nullptr;
		}
	}

	void DeviceMemory::flush(VkDeviceSize offset, VkDeviceSize size) noexcept {
		if (!coherent) {
			VkResult result = vmaFlushAllocation(g_ctx.vma, handle, offset, size);
			// TODO: fatal error
		}
	}

	void DeviceMemory::update(const void* data, VkDeviceSize size, VkDeviceSize offset) noexcept {
		if (persistent) {
			memcpy((i8*)mapped + offset, data, size);
			flush(offset, size);
		}
		else {
			map();

			memcpy((i8*)mapped + offset, data, size);
			unmap();

			flush(offset, size);
		}
	}

	bool Image::create(ImageCreateInfo create_info) noexcept {
		u32 max_dimension = (create_info.extent.width > create_info.extent.height) ? create_info.extent.width : create_info.extent.height;
		u32 max_mip_levels = compute_max_mip_level(max_dimension);

		u32 queue_family_indices[GFX_QUEUE_FAMILY_MAX];
		for (i32 i = 0; i < g_ctx.queue_family_count; ++i) {
			queue_family_indices[i] = i;
		}

		VmaAllocationCreateInfo allocation_create_info = {
			.usage = VMA_MEMORY_USAGE_AUTO
		};

		const VkImageCreateInfo image_create_info = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			.flags = (create_info.face_count == 6u) ? (VkImageCreateFlags)VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : (VkImageCreateFlags)VK_IMAGE_CREATE_EXTENDED_USAGE_BIT,
			.imageType = (create_info.extent.depth > 1u) ? VK_IMAGE_TYPE_3D : (create_info.extent.height > 1u) ? VK_IMAGE_TYPE_2D : VK_IMAGE_TYPE_1D,
			.format = create_info.format,
			.extent = create_info.extent,
			.mipLevels = clamp(create_info.level_count, 1u, max_mip_levels),
			.arrayLayers = clamp(create_info.layer_count * create_info.face_count, 1u, g_ctx.properties.limits.maxImageArrayLayers),
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.tiling = VK_IMAGE_TILING_OPTIMAL,
			.usage = create_info.usage_flags,
			.sharingMode = g_ctx.queue_family_count > 1 ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = g_ctx.queue_family_count,
			.pQueueFamilyIndices = queue_family_indices,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
		};

		bool is_color_attachment = create_info.usage_flags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		bool is_depth_attachment = create_info.usage_flags & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		if (is_color_attachment || is_depth_attachment) {
			allocation_create_info.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
			allocation_create_info.priority = 1.0f;
		}
		VmaAllocationInfo alloc_info;
		VkResult result = vmaCreateImage(g_ctx.vma, &image_create_info, &allocation_create_info, &handle, &memory.handle, &alloc_info);
		if (result != VK_SUCCESS) {
			return false;
		}

		memory.size = alloc_info.size;
		memory.mapped = alloc_info.pMappedData;
		memory.setup();

		extent = create_info.extent;
		level_count = create_info.level_count;
		layer_count = create_info.layer_count;
		face_count = create_info.face_count;
		usage_flags = create_info.usage_flags;
		format = create_info.format;
		layout = VK_IMAGE_LAYOUT_UNDEFINED;

		return true;
	}

	void Image::destroy() noexcept {
		if (handle == VK_NULL_HANDLE || memory.handle == VK_NULL_HANDLE) {
			return;
		}
		vmaDestroyImage(g_ctx.vma, handle, memory.handle);
		handle = VK_NULL_HANDLE;
		memory.handle = VK_NULL_HANDLE;
	}

	bool ImageView::create(Image image, VkImageViewType type, VkImageSubresourceRange subresource_range) noexcept {
		assert(image && "Image handle is null");

		const VkImageViewCreateInfo create_info = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = image.handle,
			.viewType = type,
			.format = image.format,
			.components = {
				.r = VK_COMPONENT_SWIZZLE_R,
				.g = VK_COMPONENT_SWIZZLE_G,
				.b = VK_COMPONENT_SWIZZLE_B,
				.a = VK_COMPONENT_SWIZZLE_A,
			},
			.subresourceRange = subresource_range
		};

		VkResult result = vkCreateImageView(g_ctx.dev, &create_info, &g_ctx.vk_alloc, &handle);
		if (result != VK_SUCCESS) {
			return false;
		}

		this->type = type;
		range = subresource_range;
		return true;
	}

	void ImageView::destroy() noexcept {
		if (handle == VK_NULL_HANDLE) {
			return;
		}
		vkDestroyImageView(g_ctx.dev, handle, &g_ctx.vk_alloc);
		handle = VK_NULL_HANDLE;
	}

	bool Buffer::create(BufferCreateInfo create_info) noexcept {
		u32 queue_family_indices[GFX_QUEUE_FAMILY_MAX];
		for (i32 i = 0; i < g_ctx.queue_family_count; ++i) {
			queue_family_indices[i] = i;
		}

		VmaAllocationCreateInfo allocation_create_info = {
			.usage = VMA_MEMORY_USAGE_AUTO
		};

		VkBufferCreateInfo buffer_create_info = {
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.sharingMode = g_ctx.queue_family_count > 1 ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = g_ctx.queue_family_count,
			.pQueueFamilyIndices = queue_family_indices
		};

		u32 minimal_alignment = 1;
		if (create_info.flags & BUFFER_FLAG_DYNAMIC) {
			allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
			allocation_create_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
				VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
		}
		else if (create_info.flags & BUFFER_FLAG_READBACK) {
			buffer_create_info.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			allocation_create_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
		}
		else if (create_info.flags & BUFFER_FLAG_STAGING) {
			buffer_create_info.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
			allocation_create_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
		}

		if (create_info.flags & BUFFER_FLAG_DEVICE_ADDRESS) {
			buffer_create_info.usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		}

		if (create_info.flags & BUFFER_FLAG_UNIFORM) {
			buffer_create_info.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			minimal_alignment = lcm(g_ctx.properties.limits.minUniformBufferOffsetAlignment, g_ctx.properties.limits.nonCoherentAtomSize);
		}
		else if (create_info.flags & BUFFER_FLAG_STORAGE) {
			buffer_create_info.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			minimal_alignment = max(minimal_alignment, (u32)g_ctx.properties.limits.minStorageBufferOffsetAlignment);
		}
		else if (create_info.flags & BUFFER_FLAG_VERTEX) {
			buffer_create_info.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			minimal_alignment = max(minimal_alignment, 4u);
		}
		else if (create_info.flags & BUFFER_FLAG_INDEX) {
			buffer_create_info.usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			minimal_alignment = max(minimal_alignment, 1u);
		}
		else if (create_info.flags & BUFFER_FLAG_INDIRECT) {
			buffer_create_info.usage |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		}
		else if (create_info.flags & BUFFER_FLAG_ACCELERATION_BUILD) {
			buffer_create_info.usage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		}
		else if (create_info.flags & BUFFER_FLAG_ACCELERATION_STORE) {
			buffer_create_info.usage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		}
		else if (create_info.flags & BUFFER_FLAG_SHADER_BINDING_TABLE) {
			buffer_create_info.usage |= VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		}

		buffer_create_info.size = align_up((u32)create_info.size, minimal_alignment);

		VmaAllocationInfo alloc_info;
		VkResult result = vmaCreateBuffer(g_ctx.vma, &buffer_create_info, &allocation_create_info, &handle, &memory.handle, &alloc_info);
		if (result != VK_SUCCESS) {
			return false;
		}

		memory.size = alloc_info.size;
		memory.mapped = alloc_info.pMappedData;

		memory.setup();

		if (create_info.flags & BUFFER_FLAG_DEVICE_ADDRESS) {
			const VkBufferDeviceAddressInfo device_address_info = {
				.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
				.buffer = handle
			};
			address = vkGetBufferDeviceAddressKHR(g_ctx.dev, &device_address_info);
		}

		flags = create_info.flags;
		return true;
	}

	void Buffer::destroy() noexcept {
		if (handle == VK_NULL_HANDLE) {
			return;
		}
		vmaDestroyBuffer(g_ctx.vma, handle, memory.handle);
		handle = VK_NULL_HANDLE;
		memory.handle = VK_NULL_HANDLE;
	}

	void BufferView::write(Span<const u8> data, VkDeviceSize offset) noexcept {
		if (offset + data.size() > size) {
			return;
		}

		memcpy((u8*)buffer.memory.map() + local_offset + offset, data.data(), data.size());
	}

	bool Semaphore::create(VkSemaphoreType type, u64 value) noexcept {
		const VkSemaphoreTypeCreateInfo type_create_info = {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
			.semaphoreType = type,
			.initialValue = value
		};

		const VkSemaphoreCreateInfo create_info = {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			.pNext = &type_create_info
		};

		VkResult result = vkCreateSemaphore(g_ctx.dev, &create_info, &g_ctx.vk_alloc, &handle);
		if (result != VK_SUCCESS) {
			return false;
		}

		this->type = type;
		this->value = value;

		return true;
	}

	void Semaphore::destroy() noexcept {
		if (handle == VK_NULL_HANDLE) {
			return;
		}
		vkDestroySemaphore(g_ctx.dev, handle, &g_ctx.vk_alloc);
		handle = VK_NULL_HANDLE;
	}

	bool Fence::create(VkFenceCreateFlags flags) noexcept {
		const VkFenceCreateInfo create_info = {
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.flags = flags
		};

		VkResult result = vkCreateFence(g_ctx.dev, &create_info, &g_ctx.vk_alloc, &handle);
		if (result != VK_SUCCESS) {
			return false;
		}

		return true;
	}

	void Fence::destroy() noexcept {
		if (handle == VK_NULL_HANDLE) {
			return;
		}
		vkDestroyFence(g_ctx.dev, handle, &g_ctx.vk_alloc);
		handle = VK_NULL_HANDLE;
	}

	bool Fence::wait(u64 timeout) noexcept {
		assert(handle != VK_NULL_HANDLE && "Fence handle is null");

		VkResult result = vkWaitForFences(g_ctx.dev, 1, &handle, VK_TRUE, timeout);
		if (result != VK_SUCCESS) {
			return false;
		}

		return true;
	}

	void Fence::reset() noexcept {
		assert(handle != VK_NULL_HANDLE && "Fence handle is null");
		vkResetFences(g_ctx.dev, 1, &handle);
	}

	bool PipelineBarrierBuilder::add_memory(VkPipelineStageFlags2 src_stage_mask, VkAccessFlags2 src_access_mask,
		VkPipelineStageFlags2 dst_stage_mask, VkAccessFlags2 dst_access_mask) noexcept {
		if (memory_barrier_count >= MEMORY_BARRIERS_MAX) {
			return false;
		}

		// TODO: Use StackBuffer
		memory_barriers[memory_barrier_count++] = {
			.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
			.srcStageMask = src_stage_mask,
			.srcAccessMask = src_access_mask,
			.dstStageMask = dst_stage_mask,
			.dstAccessMask = dst_access_mask
		};

		return true;
	}

	struct LayoutInfo {
		VkPipelineStageFlags2KHR stageFlags;
		VkAccessFlags2 accessFlags;
	};

	static LayoutInfo get_buffer_layout_info(BufferLayout layout) {
		constexpr VkPipelineStageFlags2KHR shader_stages = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT |
			VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT |
			VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;

		switch (layout) {
		case BufferLayout::TransferSrc: return {
			.stageFlags = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
			.accessFlags = VK_ACCESS_2_TRANSFER_READ_BIT
		};
		case BufferLayout::TransferDst: return {
			.stageFlags = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
			.accessFlags = VK_ACCESS_2_TRANSFER_WRITE_BIT
		};
		case BufferLayout::VertexBuffer: return {
			.stageFlags = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT,
			.accessFlags = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT
		};
		case BufferLayout::IndexBuffer: return {
			.stageFlags = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT,
			.accessFlags = VK_ACCESS_2_INDEX_READ_BIT
		};
		case BufferLayout::UniformBuffer: return {
			.stageFlags = shader_stages,
			.accessFlags = VK_ACCESS_2_UNIFORM_READ_BIT
		};
		case BufferLayout::StorageBufferRead: return {
			.stageFlags = shader_stages,
			.accessFlags = VK_ACCESS_2_SHADER_STORAGE_READ_BIT
		};
		case BufferLayout::StorageBufferWrite: return {
			.stageFlags = shader_stages,
			.accessFlags = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT
		};
		case BufferLayout::StorageBufferRW: return {
			.stageFlags = shader_stages,
			.accessFlags = VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT
		};
		case BufferLayout::IndirectBuffer: return {
			.stageFlags = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
			.accessFlags = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT
		};
		case BufferLayout::HostRead: return {
			.stageFlags = VK_PIPELINE_STAGE_2_HOST_BIT,
			.accessFlags = VK_ACCESS_2_HOST_READ_BIT
		};
		case BufferLayout::HostWrite: return {
			.stageFlags = VK_PIPELINE_STAGE_2_HOST_BIT,
			.accessFlags = VK_ACCESS_2_HOST_WRITE_BIT
		};
		case BufferLayout::ShaderRead: return {
			.stageFlags = shader_stages,
			.accessFlags = VK_ACCESS_2_SHADER_READ_BIT_KHR
		};
		case BufferLayout::ShaderWrite: return {
			.stageFlags = shader_stages,
			.accessFlags = VK_ACCESS_2_SHADER_WRITE_BIT_KHR
		};
		case BufferLayout::ShaderRW: return {
			.stageFlags = shader_stages,
			.accessFlags = VK_ACCESS_2_SHADER_READ_BIT_KHR | VK_ACCESS_2_SHADER_WRITE_BIT_KHR
		};
		case BufferLayout::Undefined:
		default: return {
			.stageFlags = VK_PIPELINE_STAGE_2_NONE,
			.accessFlags = VK_ACCESS_2_NONE
		};
		}
	}

	bool PipelineBarrierBuilder::add_buffer(Buffer buffer, BufferLayout new_layout, VkDeviceSize offset, VkDeviceSize size) noexcept {
		if (buffer_barrier_count >= BUFFER_BARRIERS_MAX || !buffer) {
			return false;
		}

		LayoutInfo src_layout_info = get_buffer_layout_info(buffer.layout);
		LayoutInfo dst_layout_info = get_buffer_layout_info(new_layout);

		// TODO: Use StackBuffer
		buffer_barriers[buffer_barrier_count++] = {
			.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
			.srcStageMask = src_layout_info.stageFlags,
			.srcAccessMask = src_layout_info.accessFlags,
			.dstStageMask = dst_layout_info.stageFlags,
			.dstAccessMask = dst_layout_info.accessFlags,
			.buffer = buffer.handle,
			.offset = offset,
			.size = size
		};

		return true;
	}

	static LayoutInfo get_image_layout_info(VkImageLayout layout) {
		switch (layout)
		{
		case VK_IMAGE_LAYOUT_UNDEFINED: return {
			.stageFlags = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			.accessFlags = VK_ACCESS_2_NONE
		};
		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: return {
			.stageFlags = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			.accessFlags = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
		};
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: return {
			.stageFlags = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
			.accessFlags = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
		};
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL: return {
			.stageFlags = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
			.accessFlags = VK_ACCESS_2_SHADER_READ_BIT
		};
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: return {
			.stageFlags = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
			.accessFlags = VK_ACCESS_2_SHADER_READ_BIT
		};
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: return {
			.stageFlags = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
			.accessFlags = VK_ACCESS_2_TRANSFER_READ_BIT
		};
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: return {
			.stageFlags = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
			.accessFlags = VK_ACCESS_2_TRANSFER_WRITE_BIT
		};
		case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: return {
			.stageFlags = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			.accessFlags = VK_ACCESS_2_NONE
		};
		default: return {
			.stageFlags = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			.accessFlags = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT
		};
		}
	}

	bool PipelineBarrierBuilder::add_image(Image image, VkImageLayout new_layout, VkImageSubresourceRange subresource_range) noexcept {
		if (image_barrier_count >= IMAGE_BARRIERS_MAX || !image) {
			return false;
		}

		LayoutInfo src_layout_info = get_image_layout_info(image.layout);
		LayoutInfo dst_layout_info = get_image_layout_info(new_layout);

		// TODO: Use StackBuffer
		image_barriers[image_barrier_count++] = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.srcStageMask = src_layout_info.stageFlags,
			.srcAccessMask = src_layout_info.accessFlags,
			.dstStageMask = dst_layout_info.stageFlags,
			.dstAccessMask = dst_layout_info.accessFlags,
			.oldLayout = image.layout,
			.newLayout = new_layout,
			.image = image.handle,
			.subresourceRange = subresource_range
		};

		return true;
	}

	void PipelineBarrierBuilder::reset() noexcept {
		memory_barrier_count = 0;
		buffer_barrier_count = 0;
		image_barrier_count = 0;
		dependency_flags = 0;
	}

	bool Sampler::create(const VkSamplerCreateInfo& create_info) noexcept {
		return vkCreateSampler(g_ctx.dev, &create_info, &g_ctx.vk_alloc, &handle) == VK_SUCCESS;
	}

	void Sampler::destroy() noexcept {
		if (handle == VK_NULL_HANDLE) {
			return;
		}
		vkDestroySampler(g_ctx.dev, handle, &g_ctx.vk_alloc);
		handle = VK_NULL_HANDLE;
	}
}