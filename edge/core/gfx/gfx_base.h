#pragma once

#include "../foundation/foundation.h"

#include <vector>
#include <chrono>
#include <expected>
#include <unordered_map>
#include <string_view>

#include <vulkan/vulkan.hpp>
#include <volk.h>
#include <vk_mem_alloc.h>

namespace edge::gfx::detail {
	template<typename... Args>
	inline void assert_failed(const char* scope, const char* condition, const char* file, int line, std::format_string<Args...> fmt, Args&&... args) {
		EDGE_LOGE("[{}]: Assertion failed: {} - {}", scope, condition, std::format(fmt, std::forward<Args>(args)...));
	}
}

#if _DEBUG
#define GFX_ASSERT_MSG(condition, ...) \
    do { \
        if (!(condition)) { \
            ::edge::gfx::detail::assert_failed(EDGE_LOGGER_SCOPE, #condition, __FILE__, __LINE__, __VA_ARGS__); \
            assert(false); \
        } \
    } while(0)
#else
#define GFX_ASSERT_MSG(condition, ...) ((void)0)
#endif

namespace edge::gfx {
	template<typename T>
	using Result = std::expected<T, vk::Result>;

	template<typename T>
	struct FeatureTraits {
		static_assert(sizeof(T) == 0, "Unsupported feature.");
	};

	template<>
	struct FeatureTraits<vk::PhysicalDeviceBufferDeviceAddressFeatures> {
		static constexpr const char* extension_name{ VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME };
	};

	template<>
	struct FeatureTraits<vk::PhysicalDevicePerformanceQueryFeaturesKHR> {
		static constexpr const char* extension_name{ VK_KHR_PERFORMANCE_QUERY_EXTENSION_NAME };
	};

	template<>
	struct FeatureTraits<vk::PhysicalDeviceHostQueryResetFeatures> {
		static constexpr const char* extension_name{ VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME };
	};

	template<>
	struct FeatureTraits<vk::PhysicalDeviceSynchronization2Features> {
		static constexpr const char* extension_name{ VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME };
	};

	template<>
	struct FeatureTraits<vk::PhysicalDeviceDynamicRenderingFeatures> {
		static constexpr const char* extension_name{ VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME };
	};

	template<>
	struct FeatureTraits<vk::PhysicalDeviceShaderDemoteToHelperInvocationFeaturesEXT> {
		static constexpr const char* extension_name{ VK_EXT_SHADER_DEMOTE_TO_HELPER_INVOCATION_EXTENSION_NAME };
	};

	template<>
	struct FeatureTraits<vk::PhysicalDevice16BitStorageFeaturesKHR> {
		static constexpr const char* extension_name{ VK_KHR_16BIT_STORAGE_EXTENSION_NAME };
	};

	template<>
	struct FeatureTraits<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT> {
		static constexpr const char* extension_name{ VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME };
	};

	template<>
	struct FeatureTraits<vk::PhysicalDeviceRayQueryFeaturesKHR> {
		static constexpr const char* extension_name{ VK_KHR_RAY_QUERY_EXTENSION_NAME };
	};

	template<>
	struct FeatureTraits<vk::PhysicalDeviceAccelerationStructureFeaturesKHR> {
		static constexpr const char* extension_name{ VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME };
	};

	template<>
	struct FeatureTraits<vk::PhysicalDeviceRayTracingPipelineFeaturesKHR> {
		static constexpr const char* extension_name{ VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME };
	};

	template<>
	struct FeatureTraits<vk::PhysicalDeviceDiagnosticsConfigFeaturesNV> {
		static constexpr const char* extension_name{ VK_NV_DEVICE_DIAGNOSTICS_CONFIG_EXTENSION_NAME };
	};

	inline auto make_vulkan_error(const char* scope, const char* condition,
		const std::source_location& location, vk::Result result, const mi::String& operation)
		-> foundation::detail::ErrorContextBuilder {
		return foundation::detail::ErrorContextBuilder(scope, condition, location)
			.with_message("Vulkan operation failed: {}", operation)
			.add_context("Result", vk::to_string(result))
			.add_context("Result Code", static_cast<int32_t>(result));
	}

	inline auto make_resource_error(const char* scope, const char* condition,
		const std::source_location& location, const mi::String& resource_type, uint32_t resource_id)
		-> foundation::detail::ErrorContextBuilder {
		return foundation::detail::ErrorContextBuilder(scope, condition, location)
			.with_message("Invalid resource access")
			.add_context("Resource Type", resource_type)
			.add_context("Resource ID", resource_id);
	}

	inline auto make_memory_error(const char* scope, const char* condition,
		const std::source_location& location, vk::DeviceSize requested, vk::DeviceSize available)
		-> foundation::detail::ErrorContextBuilder {
		return foundation::detail::ErrorContextBuilder(scope, condition, location)
			.with_message("Memory allocation or access error")
			.add_context("Requested Size", std::format("{} bytes", requested))
			.add_context("Available Size", std::format("{} bytes", available));
	}

	inline auto make_buffer_error(const char* scope, const char* condition,
		const std::source_location& location, const mi::String& operation,
		vk::DeviceSize buffer_size, vk::DeviceSize offset, vk::DeviceSize size)
		-> foundation::detail::ErrorContextBuilder {
		return foundation::detail::ErrorContextBuilder(scope, condition, location)
			.with_message("Buffer operation error: {}", operation)
			.add_context("Buffer Size", std::format("{} bytes", buffer_size))
			.add_context("Offset", std::format("{} bytes", offset))
			.add_context("Operation Size", std::format("{} bytes", size))
			.add_context("End Position", std::format("{} bytes", offset + size))
			.add_context("Overflow", std::format("{} bytes", (offset + size) - buffer_size));
	}

	inline auto make_image_error(const char* scope, const char* condition,
		const std::source_location& location, const mi::String& operation,
		vk::Extent3D extent, vk::Format format, uint32_t mip_levels, uint32_t array_layers)
		-> foundation::detail::ErrorContextBuilder {
		return foundation::detail::ErrorContextBuilder(scope, condition, location)
			.with_message("Image operation error: {}", operation)
			.add_context("Extent", std::format("{}x{}x{}", extent.width, extent.height, extent.depth))
			.add_context("Format", vk::to_string(format))
			.add_context("Mip Levels", mip_levels)
			.add_context("Array Layers", array_layers);
	}
}

#define EDGE_FATAL_VK_ERROR(result, operation) EDGE_FATAL_ERROR_CTX(result == vk::Result::eSuccess, ::edge::gfx::make_vulkan_error(EDGE_LOGGER_SCOPE, "result == vk::Result::eSuccess", std::source_location::current(), result, operation))
#define EDGE_FATAL_VK_BUFFER_ERROR(condition, operation, buffer_size, offset, size) EDGE_FATAL_ERROR_CTX(condition, ::edge::gfx::make_buffer_error(EDGE_LOGGER_SCOPE, #condition, std::source_location::current(), operation, buffer_size, offset, size))
#define EDGE_FATAL_VK_IMAGE_ERROR(condition, operation, extent, format, mip_levels, array_layers) EDGE_FATAL_ERROR_CTX(condition, ::edge::gfx::make_image_error(EDGE_LOGGER_SCOPE, #condition, std::source_location::current(), operation, extent, format, mip_levels, array_layers))