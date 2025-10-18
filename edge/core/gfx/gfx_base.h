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

namespace edge::gfx {
	template<typename T>
	using Result = std::expected<T, vk::Result>;

	template<typename T>
	class Allocator {
	public:
		using value_type = T;
		using size_type = std::size_t;
		using difference_type = std::ptrdiff_t;
		using pointer = T*;
		using const_pointer = const T*;
		using reference = T&;
		using const_reference = const T&;

		Allocator(vk::AllocationCallbacks const* callbacks = nullptr) noexcept
			: callbacks_(callbacks) {
		}

		template<typename U>
		Allocator(const Allocator<U>& other) noexcept
			: callbacks_(other.get_callbacks()) {
		}

		auto allocate(size_type n) -> pointer {
			if (callbacks_ && callbacks_->pfnAllocation) {
				void* ptr = callbacks_->pfnAllocation(
					callbacks_->pUserData,
					n * sizeof(T),
					alignof(T),
					vk::SystemAllocationScope::eObject
				);
				if (!ptr) {
					throw std::bad_alloc();
				}
				return static_cast<pointer>(ptr);
			}
			else {
#if defined(VK_USE_PLATFORM_WIN32_KHR)
				return static_cast<pointer>(_aligned_malloc(n * sizeof(T), alignof(T)));
#else
				return static_cast<pointer>(aligned_alloc(alignof(T), n * sizeof(T)));
#endif
			}
		}

		void deallocate(pointer p, size_type) noexcept {
			if (callbacks_ && callbacks_->pfnFree) {
				callbacks_->pfnFree(callbacks_->pUserData, p);
			}
			else {
#if defined(VK_USE_PLATFORM_WIN32_KHR)
				_aligned_free(p);
#else
				std::free(p);
#endif
			}
		}

		template<typename U>
		auto operator==(const Allocator<U>& other) const noexcept -> bool {
			return callbacks_ == other.get_callbacks();
		}

		template<typename U>
		auto operator!=(const Allocator<U>& other) const noexcept -> bool {
			return !(*this == other);
		}

		auto get_callbacks() const noexcept -> vk::AllocationCallbacks const* {
			return callbacks_;
		}
	private:
		vk::AllocationCallbacks const* callbacks_;
	};

	template<typename T>
	using Vector = std::vector<T, Allocator<T>>;

	template<typename K, typename T, typename Hasher = std::hash<K>, typename KeyEq = std::equal_to<K>, typename Alloc = Allocator<std::pair<const K, T>>>
	using HashMap = std::unordered_map<K, T, Hasher, KeyEq, Alloc>;

	template<typename T>
	using BasicString = std::basic_string<T, std::char_traits<T>, Allocator<T>>;

	using String = BasicString<char>;
	using WString = BasicString<wchar_t>;

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