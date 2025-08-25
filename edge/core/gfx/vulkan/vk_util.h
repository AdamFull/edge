#pragma once

#include <spdlog/spdlog.h>

#include <vulkan/vulkan.h>

#define VK_CHECK_RESULT(result, error_text) \
if(result != VK_SUCCESS) { \
	spdlog::error("[Vulkan Graphics Context]: {} Reason: {}", error_text, get_error_string(result)); \
	return false; \
}

namespace edge::gfx {
    inline auto get_error_string(VkResult result) -> const char* {
#define MAKE_FLAG_CASE(flag_name) case flag_name: return #flag_name;
        switch (result) {
            MAKE_FLAG_CASE(VK_SUCCESS);
            MAKE_FLAG_CASE(VK_NOT_READY);
            MAKE_FLAG_CASE(VK_TIMEOUT);
            MAKE_FLAG_CASE(VK_EVENT_SET);
            MAKE_FLAG_CASE(VK_EVENT_RESET);
            MAKE_FLAG_CASE(VK_INCOMPLETE);
            MAKE_FLAG_CASE(VK_ERROR_OUT_OF_HOST_MEMORY);
            MAKE_FLAG_CASE(VK_ERROR_OUT_OF_DEVICE_MEMORY);
            MAKE_FLAG_CASE(VK_ERROR_INITIALIZATION_FAILED);
            MAKE_FLAG_CASE(VK_ERROR_DEVICE_LOST);
            MAKE_FLAG_CASE(VK_ERROR_MEMORY_MAP_FAILED);
            MAKE_FLAG_CASE(VK_ERROR_LAYER_NOT_PRESENT);
            MAKE_FLAG_CASE(VK_ERROR_EXTENSION_NOT_PRESENT);
            MAKE_FLAG_CASE(VK_ERROR_FEATURE_NOT_PRESENT);
            MAKE_FLAG_CASE(VK_ERROR_INCOMPATIBLE_DRIVER);
            MAKE_FLAG_CASE(VK_ERROR_TOO_MANY_OBJECTS);
            MAKE_FLAG_CASE(VK_ERROR_FORMAT_NOT_SUPPORTED);
            MAKE_FLAG_CASE(VK_ERROR_FRAGMENTED_POOL);
            MAKE_FLAG_CASE(VK_ERROR_UNKNOWN);
            MAKE_FLAG_CASE(VK_ERROR_OUT_OF_POOL_MEMORY);
            MAKE_FLAG_CASE(VK_ERROR_INVALID_EXTERNAL_HANDLE);
            MAKE_FLAG_CASE(VK_ERROR_FRAGMENTATION);
            MAKE_FLAG_CASE(VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS);
            MAKE_FLAG_CASE(VK_PIPELINE_COMPILE_REQUIRED);
            MAKE_FLAG_CASE(VK_ERROR_NOT_PERMITTED);
            MAKE_FLAG_CASE(VK_ERROR_SURFACE_LOST_KHR);
            MAKE_FLAG_CASE(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR);
            MAKE_FLAG_CASE(VK_SUBOPTIMAL_KHR);
            MAKE_FLAG_CASE(VK_ERROR_OUT_OF_DATE_KHR);
            MAKE_FLAG_CASE(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR);
            MAKE_FLAG_CASE(VK_ERROR_VALIDATION_FAILED_EXT);
            MAKE_FLAG_CASE(VK_ERROR_INVALID_SHADER_NV);
            MAKE_FLAG_CASE(VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR);
            MAKE_FLAG_CASE(VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR);
            MAKE_FLAG_CASE(VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR);
            MAKE_FLAG_CASE(VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR);
            MAKE_FLAG_CASE(VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR);
            MAKE_FLAG_CASE(VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR);
            MAKE_FLAG_CASE(VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT);
            MAKE_FLAG_CASE(VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT);
            MAKE_FLAG_CASE(VK_THREAD_IDLE_KHR);
            MAKE_FLAG_CASE(VK_THREAD_DONE_KHR);
            MAKE_FLAG_CASE(VK_OPERATION_DEFERRED_KHR);
            MAKE_FLAG_CASE(VK_OPERATION_NOT_DEFERRED_KHR);
            MAKE_FLAG_CASE(VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR);
            MAKE_FLAG_CASE(VK_ERROR_COMPRESSION_EXHAUSTED_EXT);
            MAKE_FLAG_CASE(VK_INCOMPATIBLE_SHADER_BINARY_EXT);
            MAKE_FLAG_CASE(VK_PIPELINE_BINARY_MISSING_KHR);
            MAKE_FLAG_CASE(VK_ERROR_NOT_ENOUGH_SPACE_KHR);
        default: return "unknown"; break;
        }
#undef MAKE_FLAG_CASE
    }

    inline void make_color(uint32_t color, float out_color[4]) {
        out_color[0] = ((color >> 24) & 0xFF) / 255.0f;
        out_color[1] = ((color >> 16) & 0xFF) / 255.0f;
        out_color[2] = ((color >> 8) & 0xFF) / 255.0f;
        out_color[3] = (color & 0xFF) / 255.0f;
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
}