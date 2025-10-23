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
	constexpr auto is_success(const Result<T>& result) noexcept -> bool {
		return result.has_value();
	}

	template<typename T>
	constexpr auto error_or(const Result<T>& result, vk::Result default_error = vk::Result::eErrorUnknown) noexcept -> vk::Result {
		return result.has_value() ? vk::Result::eSuccess : result.error();
	}

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

	struct MemoryAllocationDesc {
		vk::DeviceSize size;
		vk::DeviceSize align;
		vk::SystemAllocationScope scope;
		std::thread::id thread_id;
	};
}