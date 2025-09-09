#include "vk_wrapper.h"

#include <atomic>
#include <ranges>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include <spdlog/spdlog.h>

static bool g_volk_initialized{ false };
static std::atomic<uint64_t> g_volk_ref_counter{ 0ull };

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

#define VKW_TRY_INIT_VOLK() { if (!g_volk_initialized) { g_volk_initialized = volkInitialize() == VK_SUCCESS; } g_volk_ref_counter.fetch_add(1ull); }
#define VKW_TRY_DEINIT_VOLK() { if (g_volk_initialized && g_volk_ref_counter.fetch_sub(1ull) == 0ull) { volkFinalize(); g_volk_initialized = false; } }

#define VKW_CHECK_RESULT(expr) { auto result = expr; if(result != VK_SUCCESS) return std::unexpected(result); }

#define VKW_LOGI(...) spdlog::info("[{}]: {}", VKW_SCOPE, std::format(__VA_ARGS__))
#define VKW_LOGD(...) spdlog::debug("[{}]: {}", VKW_SCOPE, std::format(__VA_ARGS__))
#define VKW_LOGT(...) spdlog::trace("[{}]: {}", VKW_SCOPE, std::format(__VA_ARGS__))
#define VKW_LOGW(...) spdlog::warn("[{}]: {}", VKW_SCOPE, std::format(__VA_ARGS__))
#define VKW_LOGE(...) spdlog::error("[{}]: {}", VKW_SCOPE, std::format(__VA_ARGS__))

namespace vkw {
#ifdef USE_VALIDATION_LAYERS
#endif

#define VKW_SCOPE "InstanceBuilder"

	inline auto enumerate_instance_layer_properties(vk::AllocationCallbacks const* allocator) -> Result<Vector<vk::LayerProperties>> {
		uint32_t count;
		if (auto result = vk::enumerateInstanceLayerProperties(&count, nullptr); result != vk::Result::eSuccess) {
			return std::unexpected(result);
		}

		Vector<vk::LayerProperties> output(count, allocator);
		if (auto result = vk::enumerateInstanceLayerProperties(&count, output.data()); result != vk::Result::eSuccess) {
			return std::unexpected(result);
		}

		return output;
	}

	inline auto enumerate_instance_extension_properties(const char* layer_name, vk::AllocationCallbacks const* allocator = nullptr) -> Result<Vector<vk::ExtensionProperties>> {
		uint32_t count;
		if (auto result = vk::enumerateInstanceExtensionProperties(layer_name, &count, nullptr); result != vk::Result::eSuccess) {
			return std::unexpected(result);
		}

		Vector<vk::ExtensionProperties> output(count, allocator);
		if (auto result = vk::enumerateInstanceExtensionProperties(layer_name, &count, output.data()); result != vk::Result::eSuccess) {
			return std::unexpected(result);
		}

		return output;
	}

	inline auto enumerate_physical_devices(vk::Instance instance, vk::AllocationCallbacks const* allocator = nullptr) -> Result<Vector<vk::PhysicalDevice>> {
		uint32_t count;
		if (auto result = instance.enumeratePhysicalDevices(&count, nullptr); result != vk::Result::eSuccess) {
			return std::unexpected(result);
		}

		Vector<vk::PhysicalDevice> output(count, allocator);
		if (auto result = instance.enumeratePhysicalDevices(&count, output.data()); result != vk::Result::eSuccess) {
			return std::unexpected(result);
		}

		return output;
	}

	inline auto enumerate_device_extension_properties(vk::PhysicalDevice device, const char* layer_name = nullptr, vk::AllocationCallbacks const* allocator = nullptr) -> Result<Vector<vk::ExtensionProperties>> {
		uint32_t count;
		if (auto result = device.enumerateDeviceExtensionProperties(layer_name, &count, nullptr); result != vk::Result::eSuccess) {
			return std::unexpected(result);
		}

		Vector<vk::ExtensionProperties> output(count, allocator);
		if (auto result = device.enumerateDeviceExtensionProperties(layer_name, &count, output.data()); result != vk::Result::eSuccess) {
			return std::unexpected(result);
		}

		return output;
	}

	inline auto get_queue_family_properties(vk::PhysicalDevice device, vk::AllocationCallbacks const* allocator = nullptr) -> Vector<vk::QueueFamilyProperties> {
		uint32_t count;
		device.getQueueFamilyProperties(&count, nullptr);
		
		Vector<vk::QueueFamilyProperties> output(count, allocator);
		device.getQueueFamilyProperties(&count, output.data());

		return output;
	}

	InstanceBuilder::InstanceBuilder(vk::AllocationCallbacks const* allocator) :
		allocator_{ allocator },
		enabled_extensions_{ allocator },
		enabled_layers_{ allocator },
		validation_feature_enables_{ allocator },
		validation_feature_disables_{ allocator } {
		create_info_.pApplicationInfo = &app_info_;
	}

	auto InstanceBuilder::build() -> Result<vk::Instance> {
		// Request all validation layers supported
		Vector<vk::LayerProperties> all_layer_properties{ allocator_ };
		
		if (auto request = enumerate_instance_layer_properties(allocator_); request.has_value()) {
			all_layer_properties = std::move(request.value());
		}

		auto check_layer_support = [&all_layer_properties](const char* layer_name) {
			return std::find_if(all_layer_properties.begin(), all_layer_properties.end(),
				[layer_name](const VkLayerProperties& props) {
					return std::strcmp(props.layerName, layer_name) == 0;
				}) != all_layer_properties.end();
			};

		// Check enabled layers
		for (auto it = enabled_layers_.begin(); it != enabled_layers_.end();) {
			if (!check_layer_support(*it)) {
				VKW_LOGW("Unsupported layer \"{}\" removed.", *it);
				it = enabled_extensions_.erase(it);
				continue;
			}

			++it;
		}

		// Request all supported extensions including validation layers
		Vector<vk::ExtensionProperties> all_extension_properties{ allocator_ };
		for (int32_t layer_index = 0; layer_index < static_cast<int32_t>(enabled_layers_.size() + 1); ++layer_index) {
			const char* layer_name = (layer_index == 0 ? nullptr : enabled_layers_[layer_index - 1]);
			if (auto request = enumerate_instance_extension_properties(layer_name, allocator_); request.has_value()) {
				auto layer_ext_props = std::move(request.value());
				all_extension_properties.insert(all_extension_properties.end(), layer_ext_props.begin(), layer_ext_props.end());
			}
		}

		// Helper function
		auto check_ext_support = [&all_extension_properties](const char* extension_name) {
			return std::find_if(all_extension_properties.begin(), all_extension_properties.end(),
				[extension_name](const vk::ExtensionProperties& props) {
					return std::strcmp(props.extensionName, extension_name) == 0;
				}) != all_extension_properties.end();
			};

		// Check enabled extensions
		for (auto it = enabled_extensions_.begin(); it != enabled_extensions_.end();) {
			if (!check_ext_support(*it)) {
				VKW_LOGW("Unsupported extension \"{}\" removed.", *it);
				it = enabled_extensions_.erase(it);
				continue;
			}

			++it;
		}

#define TRY_ENABLE_EXTENSION(ext) if(!check_ext_support(ext)) { VKW_LOGE("Extension \"{}\" not supported.", ext); return std::unexpected(vk::Result::eErrorExtensionNotPresent); } add_extension(ext);

		// Surface
		if (enable_surface_) {
			TRY_ENABLE_EXTENSION(VK_KHR_SURFACE_EXTENSION_NAME);
#if defined(VK_USE_PLATFORM_WIN32_KHR)
			TRY_ENABLE_EXTENSION(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
			TRY_ENABLE_EXTENSION(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_XCB_KHR)
			TRY_ENABLE_EXTENSION(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
			TRY_ENABLE_EXTENSION(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
			TRY_ENABLE_EXTENSION(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_MACOS_MVK)
			TRY_ENABLE_EXTENSION(VK_MVK_MACOS_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_IOS_MVK)
			TRY_ENABLE_EXTENSION(VK_MVK_IOS_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_METAL_EXT)
			TRY_ENABLE_EXTENSION(VK_EXT_METAL_SURFACE_EXTENSION_NAME);
#else
#error "Unsipported platform"
#endif
		}
		else {
			TRY_ENABLE_EXTENSION(VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME);
		}

		// Debug utils
		if (enable_debug_utils_) {
			TRY_ENABLE_EXTENSION(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

		// Portability
		if (enable_portability_) {
			TRY_ENABLE_EXTENSION(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
			add_flag(vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR);
		}

#undef TRY_ENABLE_EXTENSION

		// Enable validation features if possible
		vk::ValidationFeaturesEXT validation_features_info{};
		if ((!validation_feature_enables_.empty() || !validation_feature_disables_.empty()) && check_ext_support(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME)) {
			add_extension(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);

			validation_features_info.enabledValidationFeatureCount = static_cast<uint32_t>(validation_feature_enables_.size());
			validation_features_info.pEnabledValidationFeatures = validation_feature_enables_.data();
			validation_features_info.disabledValidationFeatureCount = static_cast<uint32_t>(validation_feature_disables_.size());
			validation_features_info.pDisabledValidationFeatures = validation_feature_disables_.data();
			validation_features_info.pNext = create_info_.pNext;
			create_info_.pNext = &validation_features_info;
		}

		create_info_.enabledExtensionCount = static_cast<uint32_t>(enabled_extensions_.size());
		create_info_.ppEnabledExtensionNames = enabled_extensions_.empty() ? nullptr : enabled_extensions_.data();
		create_info_.enabledLayerCount = static_cast<uint32_t>(enabled_layers_.size());
		create_info_.ppEnabledLayerNames = enabled_layers_.empty() ? nullptr : enabled_layers_.data();

		vk::Instance instance_handle;
		if (auto result = vk::createInstance(&create_info_, allocator_, &instance_handle); result != vk::Result::eSuccess) {
			return std::unexpected(result);
		}

		// initialize the Vulkan-Hpp default dispatcher on the instance
		VULKAN_HPP_DEFAULT_DISPATCHER.init(instance_handle);

		// Need to load volk for all the not-yet Vulkan-Hpp calls
		volkLoadInstance(instance_handle);

		return instance_handle;
	}

#undef VKW_SCOPE // InstanceBuilder

#define VKW_SCOPE "DeviceSelector"

	DeviceSelector::DeviceSelector(vk::Instance instance, vk::AllocationCallbacks const* allocator) :
		instance_{ instance },
		allocator_{ allocator },
		requested_extensions_{ allocator_ },
		requested_features_{ allocator_ } {
	}

	auto DeviceSelector::select() -> Result<Device> {
		int32_t best_candidate_index{ -1 };
		int32_t fallback_index{ -1 };

		Vector<vk::PhysicalDevice> physical_devices(allocator_);
		if (auto result = enumerate_physical_devices(instance_, allocator_); result.has_value()) {
			physical_devices = std::move(result.value());
		}

		Vector<Vector<const char*>> per_device_extensions(physical_devices.size(), Vector<const char*>(allocator_), allocator_);
		for (int32_t device_idx = 0; device_idx < static_cast<int32_t>(physical_devices.size()); ++device_idx) {
			auto& physical_device = physical_devices[device_idx];

			auto properties = physical_device.getProperties();

			Vector<vk::ExtensionProperties> available_extensions{ allocator_ };
			if (auto result = enumerate_device_extension_properties(physical_device); result.has_value()) {
				available_extensions = std::move(result.value());
			}

			// No extensions found. Bug?
			if (available_extensions.empty()) {
				VKW_LOGE("Device \"{}\" have no supported extensions. Check driver.", std::string_view(properties.deviceName));
				continue;
			}

			auto check_ext_support = [&available_extensions](const char* extension_name) {
				return std::find_if(available_extensions.begin(), available_extensions.end(),
					[extension_name](const vk::ExtensionProperties& props) {
						return std::strcmp(props.extensionName, extension_name) == 0;
					}) != available_extensions.end();
				};

			// Check for all required extension support
			bool all_extension_supported{ true };
			auto& requested_extensions = per_device_extensions[device_idx];
			for (auto& ext : requested_extensions_) {
				if (!check_ext_support(ext.first)) {
					if (ext.second) {
						VKW_LOGE("Device \"{}\" is not support required extension \"{}\"", std::string_view(properties.deviceName), ext.first);
						all_extension_supported = false;
					}

					VKW_LOGW("Device \"{}\" is not support optional extension \"{}\"", std::string_view(properties.deviceName), ext.first);
					continue;
				}

				requested_extensions.push_back(ext.first);
			}

			// Can't use this device, because some extensions is not supported
			if (!all_extension_supported) {
				continue;
			}

			// Check that device have queue with present support
			if (surface_) {
				auto queue_family_props = get_queue_family_properties(physical_device, allocator_);
				
				bool surface_supported{ false };
				for (uint32_t queue_family_index = 0; queue_family_index < static_cast<uint32_t>(queue_family_props.size()); ++queue_family_index) {
					if (physical_device.getSurfaceSupportKHR(queue_family_index, surface_) == VK_TRUE) {
						surface_supported = true;
						break;
					}
				}

				// We requested surface check, but device is not support it
				if (!surface_supported) {
					continue;
				}
			}

			// Not critical requirements, save as fallback
			if (properties.apiVersion < minimal_api_ver || properties.deviceType != preferred_type_) {
				fallback_index = device_idx;
				continue;
			}

			best_candidate_index = device_idx;
			break;
		}

		int32_t selected_device_index = best_candidate_index;
		if (selected_device_index == -1) {
			if (fallback_index == -1) {
				return std::unexpected(vk::Result::eErrorIncompatibleDriver);
			}
			selected_device_index = fallback_index;
		}

		auto& selected_device = physical_devices[selected_device_index];
		auto& enabled_extensions = per_device_extensions[selected_device_index];

		auto properties = selected_device.getProperties();
		auto queue_family_properties = get_queue_family_properties(selected_device, allocator_);

		VKW_LOGD("{} device \"{}\" selected.", to_string(properties.deviceType), std::string_view(properties.deviceName));

		Vector<vk::DeviceQueueCreateInfo> queue_create_infos(queue_family_properties.size(), allocator_);
		Vector<Vector<float>> family_queue_priorities(queue_family_properties.size(), Vector<float>(allocator_), allocator_);
		for (uint32_t family_index = 0u; family_index < static_cast<uint32_t>(queue_create_infos.size()); ++family_index) {
			auto& family_props = queue_family_properties[family_index];

			auto& queue_priorities = family_queue_priorities[family_index];
			queue_priorities.resize(family_props.queueCount, 0.5f);

			auto& queue_create_info = queue_create_infos[family_index];
			queue_create_info.queueFamilyIndex = family_index;
			queue_create_info.queueCount = family_props.queueCount;
			queue_create_info.pQueuePriorities = queue_priorities.data();
		}

		vk::DeviceCreateInfo create_info{};
		create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
		create_info.pQueueCreateInfos = queue_create_infos.data();
		create_info.enabledExtensionCount = static_cast<uint32_t>(enabled_extensions.size());
		create_info.ppEnabledExtensionNames = enabled_extensions.data();
		create_info.enabledLayerCount = 0u;
		create_info.ppEnabledLayerNames = nullptr;
		create_info.pNext = nullptr;

		// Enable all possible features
		vk::PhysicalDeviceVulkan11Features features11{};
		features11.pNext = nullptr;
		vk::PhysicalDeviceVulkan12Features features12{};
		features12.pNext = nullptr;
		vk::PhysicalDeviceVulkan13Features features13{};
		features13.pNext = nullptr;
		vk::PhysicalDeviceVulkan14Features features14{};
		features14.pNext = nullptr;

		void* feature_chain{ nullptr };
		if (properties.apiVersion >= VK_API_VERSION_1_4) {
			feature_chain = &features14;
			features14.pNext = &features13;
			features13.pNext = &features12;
			features12.pNext = &features11;
			features11.pNext = last_feature_ptr_;
		}
		else if (properties.apiVersion >= VK_API_VERSION_1_3) {
			feature_chain = &features13;
			features13.pNext = &features12;
			features12.pNext = &features11;
			features11.pNext = last_feature_ptr_;
		}
		else if (properties.apiVersion >= VK_API_VERSION_1_2) {
			feature_chain = &features12;
			features12.pNext = &features11;
			features11.pNext = last_feature_ptr_;
		}
		else if (properties.apiVersion >= VK_API_VERSION_1_1) {
			feature_chain = &features11;
			features11.pNext = last_feature_ptr_;
		}
		else {
			feature_chain = last_feature_ptr_;
		}

		vk::PhysicalDeviceFeatures2 features2{};
		features2.pNext = feature_chain;
		selected_device.getFeatures2(&features2);

		create_info.pEnabledFeatures = &features2.features;
		create_info.pNext = feature_chain;

		vk::Device device;
		if (auto result = selected_device.createDevice(&create_info, allocator_, &device); result != vk::Result::eSuccess) {
			return std::unexpected(result);
		}

		return Device{ selected_device, device, allocator_, std::move(enabled_extensions) };
	}

#undef VKW_SCOPE // DeviceSelector

	Device::Device(vk::PhysicalDevice physical, vk::Device logical, vk::AllocationCallbacks const* allocator, Vector<const char*>&& enabled_extensions) 
		: physical_{ physical }, logical_{ logical }, allocator_{ allocator }, enabled_extensions_{ std::move(enabled_extensions) },
		supported_extensions_{ allocator } {
		if (physical) {
			if (auto result = enumerate_device_extension_properties(physical, nullptr, allocator); result.has_value()) {
				supported_extensions_ = std::move(result.value());
			}
		}
	}

	Device::~Device() {
		if (logical_) {
			logical_.destroy();
		}
	}

	auto Device::set_object_name(vk::ObjectType object_type, uint64_t object_handle, std::string_view name) const -> void {
		{
			vk::DebugUtilsObjectNameInfoEXT name_info{ object_type, object_handle, name.data() };
			if (auto result = logical_.setDebugUtilsObjectNameEXT(&name_info); result == vk::Result::eSuccess) {
				return;
			}
		}

		{
			vk::DebugMarkerObjectNameInfoEXT name_info{ (vk::DebugReportObjectTypeEXT)object_type, object_handle, name.data() };
			if (auto result = logical_.debugMarkerSetObjectNameEXT(&name_info); result == vk::Result::eSuccess) {
				return;
			}
		}
	}

	auto Device::is_enabled(const char* extension_name) const -> bool {
		return std::find_if(enabled_extensions_.begin(), enabled_extensions_.end(),
			[&extension_name](const char* name) -> bool {
				return strcmp(extension_name, name) == 0;
			}) != enabled_extensions_.end();
	}

	auto Device::is_supported(const char* extension_name) const -> bool {
		return std::find_if(supported_extensions_.begin(), supported_extensions_.end(),
			[&extension_name](const vk::ExtensionProperties& props) -> bool {
				return strcmp(extension_name, props.extensionName) == 0;
			}) != supported_extensions_.end();
	}
}