#pragma once

#include <array>
#include <expected>
#include <string_view>
#include <vector>
#include <memory>
#include <cstdlib>
#include <span>
#include <functional>

#include <vulkan/vulkan.hpp>
#include <volk.h>
#include <vk_mem_alloc.h>

#if defined(VKW_DEBUG) || defined(VKW_VALIDATION_LAYERS)
#ifndef USE_VALIDATION_LAYERS
#define USE_VALIDATION_LAYERS
#endif
#endif

#if defined(USE_VALIDATION_LAYERS) && (defined(VKW_VALIDATION_LAYERS_GPU_ASSISTED) || defined(VKW_VALIDATION_LAYERS_BEST_PRACTICES) || defined(VKW_VALIDATION_LAYERS_SYNCHRONIZATION))
#ifndef USE_VALIDATION_LAYER_FEATURES
#define USE_VALIDATION_LAYER_FEATURES
#endif
#endif

namespace vkw {
	class Instance;
	class PhysicalDevice;
	class Device;
	class DebugUtilsMessengerEXT;

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

	class InstanceBuilder {
	public:
		InstanceBuilder(vk::AllocationCallbacks const* allocator);

		// Application info setters
		auto set_app_name(const char* name) -> InstanceBuilder& {
			app_info_.pApplicationName = name;
			return *this;
		}

		auto set_app_version(uint32_t version) -> InstanceBuilder& {
			app_info_.applicationVersion = version;
			return *this;
		}

		auto set_app_version(uint32_t major, uint32_t minor, uint32_t patch) -> InstanceBuilder& {
			app_info_.applicationVersion = VK_MAKE_VERSION(major, minor, patch);
			return *this;
		}

		auto set_engine_name(const char* name) -> InstanceBuilder& {
			app_info_.pEngineName = name;
			return *this;
		}

		auto set_engine_version(uint32_t version) -> InstanceBuilder& {
			app_info_.engineVersion = version;
			return *this;
		}

		auto set_engine_version(uint32_t major, uint32_t minor, uint32_t patch) -> InstanceBuilder& {
			app_info_.engineVersion = VK_MAKE_VERSION(major, minor, patch);
			return *this;
		}

		auto set_api_version(uint32_t version) -> InstanceBuilder& {
			app_info_.apiVersion = version;
			return *this;
		}

		auto set_api_version(uint32_t major, uint32_t minor, uint32_t patch) -> InstanceBuilder& {
			app_info_.apiVersion = VK_MAKE_API_VERSION(0, major, minor, patch);
			return *this;
		}

		// Extension management
		auto add_extension(const char* extension_name, bool required = false) -> InstanceBuilder& {
			enabled_extensions_.emplace_back(extension_name);
			return *this;
		}

		auto add_extensions(std::span<const char* const> extensions) -> InstanceBuilder& {
			enabled_extensions_.insert(enabled_extensions_.end(), extensions.begin(), extensions.end());
			return *this;
		}

		auto remove_extension(const char* extension_name) -> InstanceBuilder& {
			enabled_extensions_.erase(
				std::remove_if(enabled_extensions_.begin(), enabled_extensions_.end(),
					[extension_name](const char* ext) { return std::strcmp(ext, extension_name) == 0; }),
				enabled_extensions_.end()
			);
			return *this;
		}

		auto clear_extensions() -> InstanceBuilder& {
			enabled_extensions_.clear();
			return *this;
		}

		// Layer management
		auto add_layer(const char* layer_name) -> InstanceBuilder& {
			enabled_layers_.emplace_back(layer_name);
			return *this;
		}

		auto add_layers(std::span<const char* const> layers) -> InstanceBuilder& {
			enabled_layers_.insert(enabled_layers_.end(), layers.begin(), layers.end());
			return *this;
		}

		auto remove_layer(const char* layer_name) -> InstanceBuilder& {
			enabled_layers_.erase(
				std::remove_if(enabled_layers_.begin(), enabled_layers_.end(),
					[layer_name](const char* layer) { return std::strcmp(layer, layer_name) == 0; }),
				enabled_layers_.end()
			);
			return *this;
		}

		auto clear_layers() -> InstanceBuilder& {
			enabled_layers_.clear();
			return *this;
		}

		auto add_validation_feature_enable(vk::ValidationFeatureEnableEXT enables) -> InstanceBuilder& {
			validation_feature_enables_.push_back(enables);
			return *this;
		}

		auto add_validation_feature_disable(vk::ValidationFeatureDisableEXT disables) -> InstanceBuilder& {
			validation_feature_disables_.push_back(disables);
			return *this;
		}

		// Debug utilities convenience methods
		auto enable_debug_utils() -> InstanceBuilder& {
			enable_debug_utils_ = true;
			return *this;
		}

		auto enable_portability() -> InstanceBuilder& {
			enable_portability_ = true;
			return *this;
		}

		// Surface extension convenience methods
		auto enable_surface() -> InstanceBuilder& {
			enable_surface_ = true;
			return *this;
		}

		// Flags and pNext chain
		auto add_flag(vk::InstanceCreateFlagBits flags) -> InstanceBuilder& {
			create_info_.flags |= flags;
			return *this;
		}

		auto set_flags(vk::InstanceCreateFlags flags) -> InstanceBuilder& {
			create_info_.flags = flags;
			return *this;
		}

		auto set_next_chain(const void* next) -> InstanceBuilder& {
			create_info_.pNext = next;
			return *this;
		}

		// Build the instance
		auto build() -> Result<vk::Instance>;

		// Get current configuration for inspection
		auto get_enabled_extensions() const -> const Vector<const char*>& { return enabled_extensions_; }
		auto get_enabled_layers() const -> const Vector<const char*>& { return enabled_layers_; }
		auto get_app_info() const -> const vk::ApplicationInfo& { return app_info_; }

	private:
		vk::ApplicationInfo app_info_{};
		vk::InstanceCreateInfo create_info_{};
		Vector<const char*> enabled_extensions_;
		Vector<const char*> enabled_layers_;
		Vector<vk::ValidationFeatureEnableEXT> validation_feature_enables_;
		Vector<vk::ValidationFeatureDisableEXT> validation_feature_disables_;
		const vk::AllocationCallbacks* allocator_{ nullptr };

		bool enable_debug_utils_{ false };
		bool enable_surface_{ false };
		bool enable_portability_{ false };
	};

	class DeviceSelector {
	public:
		DeviceSelector(vk::Instance instance, vk::AllocationCallbacks const* allocator);

		auto set_surface(vk::SurfaceKHR surface) -> DeviceSelector& {
			surface_ = surface;
			return *this;
		}

		auto set_api_version(uint32_t version) -> DeviceSelector& {
			minimal_api_ver = version;
			return *this;
		}

		auto set_api_version(uint32_t major, uint32_t minor, uint32_t patch) -> DeviceSelector& {
			minimal_api_ver = VK_MAKE_API_VERSION(0, major, minor, patch);
			return *this;
		}

		auto set_preferred_device_type(vk::PhysicalDeviceType type) -> DeviceSelector& {
			preferred_type_ = type;
			return *this;
		}

		// Extension management
		auto add_extension(const char* extension_name, bool required = true) -> DeviceSelector& {
			requested_extensions_.push_back(std::make_pair(extension_name, required));
			return *this;
		}

		auto remove_extension(const char* extension_name) -> DeviceSelector& {
			requested_extensions_.erase(
				std::remove_if(requested_extensions_.begin(), requested_extensions_.end(),
					[extension_name](const std::pair<const char*, bool>& ext) { return std::strcmp(ext.first, extension_name) == 0; }),
				requested_extensions_.end()
			);
			return *this;
		}

		auto clear_extensions() -> DeviceSelector& {
			requested_extensions_.clear();
			return *this;
		}

		template<typename StructureType>
		auto add_feature(bool required = true) -> DeviceSelector& {
			StructureType feature{};
			feature.pNext = nullptr;

			requested_features_.push_back(std::move(std::make_shared<StructureType>(feature)));
			auto* extension_ptr = static_cast<StructureType*>(requested_features_.back().get());

			if (last_feature_ptr_) {
				extension_ptr->pNext = last_feature_ptr_;
			}
			last_feature_ptr_ = extension_ptr;
			requested_extensions_.push_back(std::make_pair(FeatureTraits<StructureType>::extension_name, required));

			return *this;
		}

		auto select() -> Result<vk::Device>;

	private:
		vk::Instance instance_{ VK_NULL_HANDLE };

		vk::AllocationCallbacks const* allocator_{ nullptr };
		vk::SurfaceKHR surface_{ VK_NULL_HANDLE };
		uint32_t minimal_api_ver{ VK_VERSION_1_0 };
		vk::PhysicalDeviceType preferred_type_{ vk::PhysicalDeviceType::eDiscreteGpu };

		Vector<std::pair<const char*, bool>> requested_extensions_;

		Vector<std::shared_ptr<void>> requested_features_;
		void* last_feature_ptr_{ nullptr };
	};
}