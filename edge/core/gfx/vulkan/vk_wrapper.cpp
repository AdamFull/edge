#include "vk_wrapper.h"

#include <atomic>
#include <ranges>
#include <numeric>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

static bool g_volk_initialized{ false };
static std::atomic<uint64_t> g_volk_ref_counter{ 0ull };

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

#define VKW_TRY_INIT_VOLK() { if (!g_volk_initialized) { g_volk_initialized = volkInitialize() == VK_SUCCESS; } g_volk_ref_counter.fetch_add(1ull); }
#define VKW_TRY_DEINIT_VOLK() { if (g_volk_initialized && g_volk_ref_counter.fetch_sub(1ull) == 0ull) { volkFinalize(); g_volk_initialized = false; } }

#define VKW_CHECK_RESULT(expr) { auto result = expr; if(result != VK_SUCCESS) return std::unexpected(result); }

namespace edge::vkw {
#ifdef USE_VALIDATION_LAYERS
#endif

#define EDGE_LOGGER_SCOPE "InstanceBuilder"
	namespace helpers {
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

		inline auto get_surface_formats(vk::PhysicalDevice device, vk::SurfaceKHR surface, vk::AllocationCallbacks const* allocator = nullptr) -> Result<Vector<vk::SurfaceFormatKHR>> {
			uint32_t count;
			if (auto result = device.getSurfaceFormatsKHR(surface, &count, nullptr); result != vk::Result::eSuccess) {
				return std::unexpected(result);
			}

			Vector<vk::SurfaceFormatKHR> output(count, allocator);
			if (auto result = device.getSurfaceFormatsKHR(surface, &count, output.data()); result != vk::Result::eSuccess) {
				return std::unexpected(result);
			}

			return output;
		}

		inline auto get_surface_present_modes(vk::PhysicalDevice device, vk::SurfaceKHR surface, vk::AllocationCallbacks const* allocator = nullptr) -> Result<Vector<vk::PresentModeKHR>> {
			uint32_t count;
			if (auto result = device.getSurfacePresentModesKHR(surface, &count, nullptr); result != vk::Result::eSuccess) {
				return std::unexpected(result);
			}

			Vector<vk::PresentModeKHR> output(count, allocator);
			if (auto result = device.getSurfacePresentModesKHR(surface, &count, output.data()); result != vk::Result::eSuccess) {
				return std::unexpected(result);
			}

			return output;
		}

		inline auto is_hdr_format(vk::Format format) -> bool {
			switch (format) {
				// 10-bit formats
			case vk::Format::eA2B10G10R10UnormPack32:
			case vk::Format::eA2R10G10B10UnormPack32:
			case vk::Format::eA2B10G10R10UintPack32:
			case vk::Format::eA2R10G10B10UintPack32:
			case vk::Format::eA2B10G10R10SintPack32:
			case vk::Format::eA2R10G10B10SintPack32:

				// 16-bit float formats
			case vk::Format::eR16G16B16A16Sfloat:
			case vk::Format::eR16G16B16Sfloat:

				// 32-bit float formats
			case vk::Format::eR32G32B32A32Sfloat:
			case vk::Format::eR32G32B32Sfloat:

				// BC6H (HDR texture compression)
			case vk::Format::eBc6HUfloatBlock:
			case vk::Format::eBc6HSfloatBlock:

				// ASTC HDR
			case vk::Format::eAstc4x4SfloatBlock:
			case vk::Format::eAstc5x4SfloatBlock:
			case vk::Format::eAstc5x5SfloatBlock:
			case vk::Format::eAstc6x5SfloatBlock:
			case vk::Format::eAstc6x6SfloatBlock:
			case vk::Format::eAstc8x5SfloatBlock:
			case vk::Format::eAstc8x6SfloatBlock:
			case vk::Format::eAstc8x8SfloatBlock:
			case vk::Format::eAstc10x5SfloatBlock:
			case vk::Format::eAstc10x6SfloatBlock:
			case vk::Format::eAstc10x8SfloatBlock:
			case vk::Format::eAstc10x10SfloatBlock:
			case vk::Format::eAstc12x10SfloatBlock:
			case vk::Format::eAstc12x12SfloatBlock:
				return true;

			default:
				return false;
			}
		}

		inline auto is_hdr_color_space(vk::ColorSpaceKHR color_space) -> bool {
			switch (color_space) {
				// HDR10 color spaces
			case vk::ColorSpaceKHR::eHdr10St2084EXT:
			case vk::ColorSpaceKHR::eHdr10HlgEXT:

				// Dolby Vision
			case vk::ColorSpaceKHR::eDolbyvisionEXT:

				// Extended sRGB (scRGB)
			case vk::ColorSpaceKHR::eExtendedSrgbLinearEXT:
			case vk::ColorSpaceKHR::eExtendedSrgbNonlinearEXT:

				// Display P3
			case vk::ColorSpaceKHR::eDisplayP3NonlinearEXT:
			case vk::ColorSpaceKHR::eDisplayP3LinearEXT:

				// Wide color gamut
			case vk::ColorSpaceKHR::eBt2020LinearEXT:
			case vk::ColorSpaceKHR::eBt709LinearEXT:
			case vk::ColorSpaceKHR::eDciP3NonlinearEXT:
			case vk::ColorSpaceKHR::eAdobergbLinearEXT:
			case vk::ColorSpaceKHR::eAdobergbNonlinearEXT:
				return true;

			default:
				return false;
			}
		}
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

		if (auto request = helpers::enumerate_instance_layer_properties(allocator_); request.has_value()) {
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
				EDGE_SLOGW("Unsupported layer \"{}\" removed.", *it);
				it = enabled_extensions_.erase(it);
				continue;
			}

			++it;
		}

		// Request all supported extensions including validation layers
		Vector<vk::ExtensionProperties> all_extension_properties{ allocator_ };
		for (int32_t layer_index = 0; layer_index < static_cast<int32_t>(enabled_layers_.size() + 1); ++layer_index) {
			const char* layer_name = (layer_index == 0 ? nullptr : enabled_layers_[layer_index - 1]);
			if (auto request = helpers::enumerate_instance_extension_properties(layer_name, allocator_); request.has_value()) {
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
				EDGE_SLOGW("Unsupported extension \"{}\" removed.", *it);
				it = enabled_extensions_.erase(it);
				continue;
			}

			++it;
		}

#define TRY_ENABLE_EXTENSION(ext) if(!check_ext_support(ext)) { EDGE_SLOGE("Extension \"{}\" not supported.", ext); return std::unexpected(vk::Result::eErrorExtensionNotPresent); } add_extension(ext);

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

#undef EDGE_LOGGER_SCOPE // InstanceBuilder

#define EDGE_LOGGER_SCOPE "DeviceSelector"

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
		if (auto result = helpers::enumerate_physical_devices(instance_, allocator_); result.has_value()) {
			physical_devices = std::move(result.value());
		}

		Vector<Vector<const char*>> per_device_extensions(physical_devices.size(), Vector<const char*>(allocator_), allocator_);
		for (int32_t device_idx = 0; device_idx < static_cast<int32_t>(physical_devices.size()); ++device_idx) {
			auto& physical_device = physical_devices[device_idx];

			auto properties = physical_device.getProperties();

			Vector<vk::ExtensionProperties> available_extensions{ allocator_ };
			if (auto result = helpers::enumerate_device_extension_properties(physical_device); result.has_value()) {
				available_extensions = std::move(result.value());
			}

			// No extensions found. Bug?
			if (available_extensions.empty()) {
				EDGE_SLOGE("Device \"{}\" have no supported extensions. Check driver.", std::string_view(properties.deviceName));
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
						EDGE_SLOGE("Device \"{}\" is not support required extension \"{}\"", std::string_view(properties.deviceName), ext.first);
						all_extension_supported = false;
					}

					EDGE_SLOGW("Device \"{}\" is not support optional extension \"{}\"", std::string_view(properties.deviceName), ext.first);
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
				auto queue_family_props = helpers::get_queue_family_properties(physical_device, allocator_);

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
		auto queue_family_properties = helpers::get_queue_family_properties(selected_device, allocator_);

		EDGE_SLOGD("{} device \"{}\" selected.", to_string(properties.deviceType), std::string_view(properties.deviceName));

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

#undef EDGE_LOGGER_SCOPE // DeviceSelector

#define EDGE_LOGGER_SCOPE "Queue"
	Queue::Queue(Device& device, uint32_t queue_family_index, uint32_t queue_index)
		: device_{ &device }
		, queue_family_index_{ queue_family_index }
		, queue_index_{ queue_index } {

		vk::DeviceQueueInfo2 queue_info{ {}, queue_family_index, queue_index };
		handle_ = device.get_logical().getQueue2(queue_info);
	}

	Queue::~Queue() {
		if (handle_) {
			device_->release_queue(*this);
			handle_ = VK_NULL_HANDLE;
		}
	}

	auto Queue::submit(const vk::SubmitInfo2& submit_info, vk::Fence fence) const -> vk::Result {
		return handle_.submit2(1, &submit_info, fence);
	}

	auto Queue::wait_idle() const -> void {
		handle_.waitIdle();
	}
#undef EDGE_LOGGER_SCOPE

#define EDGE_LOGGER_SCOPE "Device"

	Device::Device(vk::PhysicalDevice physical, vk::Device logical, vk::AllocationCallbacks const* allocator, Vector<const char*>&& enabled_extensions)
		: physical_{ physical }
		, logical_{ logical }
		, allocator_{ allocator }
		, enabled_extensions_{ std::move(enabled_extensions) }
		, supported_extensions_{ allocator }
		, queue_family_map_{ Vector<QueueFamilyInfo>(allocator), Vector<QueueFamilyInfo>(allocator), Vector<QueueFamilyInfo>(allocator) } {
		if (physical) {
			if (auto result = helpers::enumerate_device_extension_properties(physical, nullptr, allocator); result.has_value()) {
				supported_extensions_ = std::move(result.value());
			}
		}

		// Make queue family map
		if (auto family_propersies = helpers::get_queue_family_properties(physical_, allocator_); !family_propersies.empty()) {
			for (uint32_t index = 0; index < static_cast<uint32_t>(family_propersies.size()); ++index) {
				auto const& queue_family_property = family_propersies[index];

				bool is_graphics_commands_supported = (queue_family_property.queueFlags & vk::QueueFlagBits::eGraphics) == vk::QueueFlagBits::eGraphics;
				bool is_compute_commands_supported = (queue_family_property.queueFlags & vk::QueueFlagBits::eCompute) == vk::QueueFlagBits::eCompute;
				bool is_copy_commands_supported = (queue_family_property.queueFlags & vk::QueueFlagBits::eTransfer) == vk::QueueFlagBits::eTransfer;

				vk::QueueFlagBits queue_type{};
				if (is_graphics_commands_supported &&
					is_compute_commands_supported && is_copy_commands_supported) {
					queue_type = vk::QueueFlagBits::eGraphics;
				}
				else if (is_compute_commands_supported && is_copy_commands_supported) {
					queue_type = vk::QueueFlagBits::eCompute;
				}
				else if (is_copy_commands_supported) {
					queue_type = vk::QueueFlagBits::eTransfer;
				}

				auto& group = queue_family_map_[static_cast<size_t>(queue_type) - 1u];
				auto& family_info = group.emplace_back();
				family_info.index = index;
				family_info.queue_indices = vkw::Vector<uint32_t>(queue_family_property.queueCount, allocator_);
				std::iota(family_info.queue_indices.rbegin(), family_info.queue_indices.rend(), 0u);

#ifndef NDEBUG
				std::string supported_commands;

				if (is_graphics_commands_supported) {
					supported_commands += "graphics,";
				}
				if (is_compute_commands_supported) {
					supported_commands += "compute,";
				}
				if (is_copy_commands_supported) {
					supported_commands += "transfer";
				}

				EDGE_SLOGD("Found \"{}\" queue that support: {} commands.", vk::to_string(queue_type), supported_commands);
#endif
			}
		}
	}

	Device::~Device() {
		if (logical_) {
			logical_.destroy();
		}
	}

	auto Device::create_allocator(vk::Instance instance) const -> Result<MemoryAllocator> {
		// Create vulkan memory allocator
		VmaVulkanFunctions vma_vulkan_func{};
		vma_vulkan_func.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
		vma_vulkan_func.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

		VmaAllocatorCreateInfo vma_allocator_create_info{};
		vma_allocator_create_info.pVulkanFunctions = &vma_vulkan_func;
		vma_allocator_create_info.physicalDevice = physical_;
		vma_allocator_create_info.device = logical_;
		vma_allocator_create_info.instance = (VkInstance)instance;
		vma_allocator_create_info.pAllocationCallbacks = (VkAllocationCallbacks const*)(allocator_);

		// NOTE: Nsight graphics using VkImportMemoryHostPointerEXT that cannot be used with dedicated memory allocation
		bool is_nsignt_graphics_attached{ false };
#ifdef VK_USE_PLATFORM_WIN32_KHR
		{
			HMODULE nsight_graphics_module = GetModuleHandle("Nvda.Graphics.Interception.dll");
			is_nsignt_graphics_attached = (nsight_graphics_module != nullptr);
		}
#endif
		bool can_get_memory_requirements = is_enabled(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
		bool has_dedicated_allocation = is_enabled(VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME);

		if (can_get_memory_requirements && has_dedicated_allocation && !is_nsignt_graphics_attached) {
			vma_allocator_create_info.flags |= VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;
		}

		if (is_enabled(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME)) {
			vma_allocator_create_info.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
		}

		if (is_enabled(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME)) {
			vma_allocator_create_info.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
		}

		if (is_enabled(VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME)) {
			vma_allocator_create_info.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT;
		}

		if (is_enabled(VK_KHR_BIND_MEMORY_2_EXTENSION_NAME)) {
			vma_allocator_create_info.flags |= VMA_ALLOCATOR_CREATE_KHR_BIND_MEMORY2_BIT;
		}

		if (is_enabled(VK_AMD_DEVICE_COHERENT_MEMORY_EXTENSION_NAME)) {
			vma_allocator_create_info.flags |= VMA_ALLOCATOR_CREATE_AMD_DEVICE_COHERENT_MEMORY_BIT;
		}

		VmaAllocator vma_allocator;
		if (auto result = vmaCreateAllocator(&vma_allocator_create_info, &vma_allocator); result != VK_SUCCESS) {
			return std::unexpected(static_cast<vk::Result>(result));
		}

		return MemoryAllocator(*this, vma_allocator);
	}

	auto Device::get_queue_family_properties() const -> Vector<vk::QueueFamilyProperties> {
		return helpers::get_queue_family_properties(physical_, allocator_);
	}

	auto Device::get_queue(vk::QueueFlagBits queue_type) -> Result<Queue> {
		auto& family_group = queue_family_map_[static_cast<size_t>(queue_type) - 1ull];
		for (auto& family : family_group) {
			if (!family.queue_indices.empty()) {
				auto queue_index = family.queue_indices.back();
				family.queue_indices.pop_back();

				return Queue(*this, family.index, queue_index);
			}
		}

		return std::unexpected(vk::Result::eErrorUnknown);
	}

	auto Device::release_queue(Queue& queue) -> void {
		for (auto& group : queue_family_map_) {
			for (auto& family : group) {
				if (family.index == queue.get_family_index()) {
					family.queue_indices.push_back(queue.get_queue_index());
					return;
				}
			}
		}
	}

	auto Device::create_handle(const vk::CommandPoolCreateInfo& create_info, vk::CommandPool& handle) const -> vk::Result {
		return logical_.createCommandPool(&create_info, allocator_, &handle);
	}

	auto Device::destroy_handle(vk::CommandPool handle) const -> void {
		logical_.destroyCommandPool(handle, allocator_);
	}

	auto Device::allocate_command_buffer(const vk::CommandBufferAllocateInfo& allocate_info, vk::CommandBuffer& handle) const -> vk::Result {
		return logical_.allocateCommandBuffers(&allocate_info, &handle);
	}

	auto Device::free_command_buffer(vk::CommandPool command_pool, vk::CommandBuffer handle) const -> void {
		logical_.freeCommandBuffers(command_pool, 1, &handle);
	}

	auto Device::create_handle(const vk::SemaphoreCreateInfo& create_info, vk::Semaphore& handle) const -> vk::Result {
		return logical_.createSemaphore(&create_info, allocator_, &handle);
	}

	auto Device::destroy_handle(vk::Semaphore handle) const -> void {
		logical_.destroySemaphore(handle, allocator_);
	}

	auto Device::signal_semaphore(const vk::SemaphoreSignalInfo& signal_info) const -> vk::Result {
		return logical_.signalSemaphore(&signal_info);
	}

	auto Device::wait_semaphore(const vk::SemaphoreWaitInfo& wait_info, uint64_t timeout) const -> vk::Result {
		return logical_.waitSemaphores(&wait_info, timeout);
	}

	auto Device::get_semaphore_counter_value(vk::Semaphore handle) const -> Result<uint64_t> {
		uint64_t value;
		if (auto result = logical_.getSemaphoreCounterValue(handle, &value); result != vk::Result::eSuccess) {
			return std::unexpected(result);
		}
		return value;
	}

	auto Device::create_handle(const vk::FenceCreateInfo& create_info, vk::Fence& handle) const -> vk::Result {
		return logical_.createFence(&create_info, allocator_, &handle);
	}

	auto Device::destroy_handle(vk::Fence handle) const -> void {
		logical_.destroyFence(handle, allocator_);
	}

	auto Device::create_handle(const vk::SwapchainCreateInfoKHR& create_info, vk::SwapchainKHR& handle) const -> vk::Result {
		return logical_.createSwapchainKHR(&create_info, allocator_, &handle);
	}

	auto Device::destroy_handle(vk::SwapchainKHR handle) const -> void {
		logical_.destroySwapchainKHR(handle, allocator_);
	}

	auto Device::get_buffer_device_address(const vk::BufferDeviceAddressInfo& address_info) const -> uint64_t {
		return logical_.getBufferAddress(address_info);
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

#undef EDGE_LOGGER_SCOPE // Device

#define EDGE_LOGGER_SCOPE "Swapchain"

	Swapchain::Swapchain(Device const& device, vk::SwapchainKHR swapchain, const State& new_state)
		: device_{ &device }
		, handle_{ swapchain }
		, state_{ new_state } {
	}

	Swapchain::~Swapchain() {
		if (handle_) {
			device_->destroy_handle(handle_);
		}
	}

	auto Swapchain::reset() -> void {
		handle_ = VK_NULL_HANDLE;
		device_ = nullptr;
	}

#undef EDGE_LOGGER_SCOPE // Swapchain

#define EDGE_LOGGER_SCOPE "SwapchainBuilder"

	SwapchainBuilder::SwapchainBuilder(Device const& device, vk::SurfaceKHR surface) 
		: device_{ &device }
		, surface_{ surface } {
	}

	auto SwapchainBuilder::build() -> Result<Swapchain> {
		vk::PresentModeKHR present_mode = (requested_state_.vsync) ? vk::PresentModeKHR::eFifo : vk::PresentModeKHR::eMailbox;
		std::array<vk::PresentModeKHR, 3ull> present_mode_priority_list{ 
#ifdef VK_USE_PLATFORM_ANDROID_KHR
			vk::PresentModeKHR::eFifo, vk::PresentModeKHR::eMailbox, 
#else
			vk::PresentModeKHR::eMailbox, vk::PresentModeKHR::eFifo,
#endif
			vk::PresentModeKHR::eImmediate };

		auto physical_device = device_->get_physical();

		Vector<vk::SurfaceFormatKHR> surface_formats;
		if (auto result = helpers::get_surface_formats(physical_device, surface_, device_->get_allocator()); result.has_value()) {
			surface_formats = std::move(result.value());
		}

		Vector<vk::PresentModeKHR> present_modes;
		if (auto result = helpers::get_surface_present_modes(physical_device, surface_, device_->get_allocator()); result.has_value()) {
			present_modes = std::move(result.value());
		}

		// Choose best properties based on surface capabilities
		vk::SurfaceCapabilitiesKHR const surface_capabilities = physical_device.getSurfaceCapabilitiesKHR(surface_);

		auto potential_extent = requested_state_.extent;
		if (potential_extent.width == 1 || potential_extent.height == 1) {
			potential_extent = surface_capabilities.currentExtent;
		}

		vk::SwapchainCreateInfoKHR create_info{};
		create_info.oldSwapchain = old_swapchain_;
		create_info.minImageCount = std::clamp(requested_state_.image_count, surface_capabilities.minImageCount, surface_capabilities.maxImageCount ? surface_capabilities.maxImageCount : std::numeric_limits<uint32_t>::max());
		create_info.imageExtent = choose_suitable_extent(potential_extent, surface_capabilities);

		const auto surface_format = choose_surface_format(requested_state_.format, surface_formats, requested_state_.hdr);
		if (requested_state_.format != vk::Format::eUndefined && surface_format != requested_state_.format) {
			EDGE_SLOGW("Requested format \"{}|{}\" is not supported. Selecting available \"{}|{}\".",
				vk::to_string(requested_state_.format.format), vk::to_string(requested_state_.format.colorSpace),
				vk::to_string(surface_format.format), vk::to_string(surface_format.colorSpace));
		}
		else {
			EDGE_SLOGI("Selected format \"{}|{}\".", vk::to_string(surface_format.format), vk::to_string(surface_format.colorSpace));
		}

		create_info.imageFormat = surface_format.format;
		create_info.imageColorSpace = surface_format.colorSpace;
		create_info.imageArrayLayers = 1;

		// TODO: Add usage valudation
		const auto format_properties = physical_device.getFormatProperties(create_info.imageFormat);
		create_info.imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;

		create_info.preTransform = ((requested_state_.transform & surface_capabilities.supportedTransforms) == requested_state_.transform) ? requested_state_.transform : surface_capabilities.currentTransform;
		create_info.compositeAlpha = choose_suitable_composite_alpha(vk::CompositeAlphaFlagBitsKHR::eInherit, surface_capabilities.supportedCompositeAlpha);
		create_info.presentMode = choose_suitable_present_mode(present_mode, present_modes, present_mode_priority_list);

		create_info.surface = surface_;
		create_info.clipped = VK_TRUE;
		create_info.imageSharingMode = vk::SharingMode::eExclusive;

		auto queue_family_properties = device_->get_queue_family_properties();
		Vector<uint32_t> queue_family_indices(queue_family_properties.size(), device_->get_allocator());
		std::iota(queue_family_indices.begin(), queue_family_indices.end(), 0);

		if (queue_family_indices.size() > 1) {
			create_info.queueFamilyIndexCount = static_cast<uint32_t>(queue_family_indices.size());
			create_info.pQueueFamilyIndices = queue_family_indices.data();
			create_info.imageSharingMode = vk::SharingMode::eConcurrent;
		}

		vk::SwapchainKHR swapchain{ VK_NULL_HANDLE };
		if (auto result = device_->create_handle(create_info, swapchain); result != vk::Result::eSuccess) {
			return std::unexpected(result);
		}

		Swapchain::State new_state{
			.image_count = create_info.minImageCount,
			.format = surface_format,
			.extent = create_info.imageExtent,
			.transform = create_info.preTransform,
			.vsync = requested_state_.vsync,
			.hdr = requested_state_.hdr && helpers::is_hdr_format(create_info.imageFormat) && helpers::is_hdr_color_space(create_info.imageColorSpace)
		};
		return Swapchain(*device_, swapchain, new_state);
	}

	auto SwapchainBuilder::choose_suitable_extent(vk::Extent2D request_extent, const vk::SurfaceCapabilitiesKHR& surface_caps) -> vk::Extent2D {
		if (surface_caps.currentExtent.width == 0xFFFFFFFF) {
			return request_extent;
		}

		if (request_extent.width < 1 || request_extent.height < 1) {
			EDGE_SLOGW(" Image extent ({}, {}) not supported. Selecting ({}, {}).",
				request_extent.width, request_extent.height,
				surface_caps.currentExtent.width, surface_caps.currentExtent.height);
			return surface_caps.currentExtent;
		}

		request_extent.width = std::clamp(request_extent.width, surface_caps.minImageExtent.width, surface_caps.maxImageExtent.width);
		request_extent.height = std::clamp(request_extent.height, surface_caps.minImageExtent.height, surface_caps.maxImageExtent.height);

		return request_extent;
	}

	auto SwapchainBuilder::choose_surface_format(const vk::SurfaceFormatKHR requested_surface_format, const Vector<vk::SurfaceFormatKHR>& available_surface_formats, bool prefer_hdr) -> vk::SurfaceFormatKHR {
		// Separate formats by hdr support
		Vector<vk::SurfaceFormatKHR> sdr_fromats{ available_surface_formats.get_allocator() };
		Vector<vk::SurfaceFormatKHR> hdr_fromats{ available_surface_formats.get_allocator() };
		for (const auto& surface_format : available_surface_formats) {
			if (helpers::is_hdr_format(surface_format.format) && helpers::is_hdr_color_space(surface_format.colorSpace)) {
				hdr_fromats.push_back(surface_format);
			}
			else {
				sdr_fromats.push_back(surface_format);
			}
		}

		// HDR priority list (in order of preference)
		static const std::array<vk::SurfaceFormatKHR, 8> hdr_priority_list = {
			// HDR10 formats - most compatible
			vk::SurfaceFormatKHR{vk::Format::eA2B10G10R10UnormPack32, vk::ColorSpaceKHR::eHdr10St2084EXT},
			vk::SurfaceFormatKHR{vk::Format::eA2R10G10B10UnormPack32, vk::ColorSpaceKHR::eHdr10St2084EXT},

			// Display P3 formats - Apple ecosystem
			vk::SurfaceFormatKHR{vk::Format::eA2B10G10R10UnormPack32, vk::ColorSpaceKHR::eDisplayP3NonlinearEXT},
			vk::SurfaceFormatKHR{vk::Format::eA2R10G10B10UnormPack32, vk::ColorSpaceKHR::eDisplayP3NonlinearEXT},

			// Extended sRGB (scRGB) - Windows
			vk::SurfaceFormatKHR{vk::Format::eR16G16B16A16Sfloat, vk::ColorSpaceKHR::eExtendedSrgbLinearEXT},
			vk::SurfaceFormatKHR{vk::Format::eA2B10G10R10UnormPack32, vk::ColorSpaceKHR::eExtendedSrgbNonlinearEXT},

			// Rec. 2020 - broadcast standard
			vk::SurfaceFormatKHR{vk::Format::eA2B10G10R10UnormPack32, vk::ColorSpaceKHR::eBt2020LinearEXT},
			vk::SurfaceFormatKHR{vk::Format::eR16G16B16A16Sfloat, vk::ColorSpaceKHR::eBt2020LinearEXT}
		};

		// SDR priority list (in order of preference)
		static const std::array<vk::SurfaceFormatKHR, 7> sdr_priority_list = {
			// 10-bit formats
			vk::SurfaceFormatKHR{vk::Format::eA2B10G10R10UnormPack32, vk::ColorSpaceKHR::eSrgbNonlinear},

			// Standard sRGB formats
			vk::SurfaceFormatKHR{vk::Format::eB8G8R8A8Srgb, vk::ColorSpaceKHR::eSrgbNonlinear},
			vk::SurfaceFormatKHR{vk::Format::eR8G8B8A8Srgb, vk::ColorSpaceKHR::eSrgbNonlinear},

			// Unorm variants (gamma corrected manually)
			vk::SurfaceFormatKHR{vk::Format::eB8G8R8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear},
			vk::SurfaceFormatKHR{vk::Format::eR8G8B8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear},

			// Alternative formats
			vk::SurfaceFormatKHR{vk::Format::eA8B8G8R8SrgbPack32, vk::ColorSpaceKHR::eSrgbNonlinear},
			vk::SurfaceFormatKHR{vk::Format::eA8B8G8R8UnormPack32, vk::ColorSpaceKHR::eSrgbNonlinear}
		};

		auto lookup_format = [](std::span<const vk::SurfaceFormatKHR> formats, const vk::SurfaceFormatKHR& requested, vk::SurfaceFormatKHR& out_format, bool full_match = true) -> bool {
			auto found = std::find_if(formats.begin(), formats.end(), [&requested, full_match](const vk::SurfaceFormatKHR& format) {
				return full_match ? (format.format == requested.format && format.colorSpace == requested.colorSpace) : format.format == requested.format;
				});

			if (found != formats.end()) {
				out_format = *found;
				return true;
			}
			return false;
			};

		auto pick_format = [&lookup_format](std::span<const vk::SurfaceFormatKHR> available_formats, const vk::SurfaceFormatKHR& requested, vk::SurfaceFormatKHR& out_format) -> bool {
			if (lookup_format(available_formats, requested, out_format) ||
				lookup_format(available_formats, requested, out_format, false)) {
				return true;
			}

			return false;
			};

		auto pick_by_list_format = [&pick_format](std::span<const vk::SurfaceFormatKHR> available_formats, std::span<const vk::SurfaceFormatKHR> priority_list) -> vk::SurfaceFormatKHR {
			for (const auto& preferred : priority_list) {
				vk::SurfaceFormatKHR selected;
				if (pick_format(available_formats, preferred, selected)) {
					return selected;
				}
			}
			return available_formats[0];
			};

		if (requested_surface_format.format != vk::Format::eUndefined) {
			vk::SurfaceFormatKHR selected;
			if (pick_format(available_surface_formats, requested_surface_format, selected)) {
				return selected;
			}
		}

		// Pick hdr format
		if (prefer_hdr && !hdr_fromats.empty()) {
			return pick_by_list_format(hdr_fromats, hdr_priority_list);
		}

		// Pick sdr format
		if (!sdr_fromats.empty()) {
			return pick_by_list_format(sdr_fromats, sdr_priority_list);
		}

		if (!available_surface_formats.empty()) {
			return available_surface_formats[0];
		}

		return {};
	}

	auto SwapchainBuilder::choose_suitable_composite_alpha(vk::CompositeAlphaFlagBitsKHR request_composite_alpha, vk::CompositeAlphaFlagsKHR supported_composite_alpha) -> vk::CompositeAlphaFlagBitsKHR {
		if (request_composite_alpha & supported_composite_alpha) {
			return request_composite_alpha;
		}

		static const std::vector<vk::CompositeAlphaFlagBitsKHR> composite_alpha_priority_list = {
			vk::CompositeAlphaFlagBitsKHR::eOpaque, vk::CompositeAlphaFlagBitsKHR::ePreMultiplied,
			vk::CompositeAlphaFlagBitsKHR::ePostMultiplied, vk::CompositeAlphaFlagBitsKHR::eInherit
		};

		auto const chosen_composite_alpha_it = std::find_if(composite_alpha_priority_list.begin(), composite_alpha_priority_list.end(),
			[&supported_composite_alpha](vk::CompositeAlphaFlagBitsKHR composite_alpha) { return composite_alpha & supported_composite_alpha; });

		if (chosen_composite_alpha_it == composite_alpha_priority_list.end()) {
			EDGE_SLOGE("No compatible composite alpha found.");
		}
		else {
			EDGE_SLOGW("Composite alpha '{}' not supported. Selecting '{}.", vk::to_string(request_composite_alpha), vk::to_string(*chosen_composite_alpha_it));
			return *chosen_composite_alpha_it;
		}

		return {};
	}

	auto SwapchainBuilder::choose_suitable_present_mode(vk::PresentModeKHR request_present_mode, std::span<const vk::PresentModeKHR> available_present_modes, std::span<const vk::PresentModeKHR> present_mode_priority_list) -> vk::PresentModeKHR {
	// Try to find the requested present mode in the available present modes
	auto const present_mode_it = std::find(available_present_modes.begin(), available_present_modes.end(), request_present_mode);
	if (present_mode_it == available_present_modes.end()) {
		// If the requested present mode isn't found, then try to find a mode from the priority list
		auto const chosen_present_mode_it =
			std::find_if(present_mode_priority_list.begin(), present_mode_priority_list.end(),
				[&available_present_modes](vk::PresentModeKHR present_mode) { return std::find(available_present_modes.begin(), available_present_modes.end(), present_mode) != available_present_modes.end(); });

		// If nothing found, always default to FIFO
		vk::PresentModeKHR const chosen_present_mode = (chosen_present_mode_it != present_mode_priority_list.end()) ? *chosen_present_mode_it : vk::PresentModeKHR::eFifo;

		EDGE_SLOGW("Present mode '{}' not supported. Selecting '{}'.", vk::to_string(request_present_mode), vk::to_string(chosen_present_mode));
		return chosen_present_mode;
	}
	else {
		EDGE_SLOGD("Present mode selected: {}", to_string(request_present_mode));
		return request_present_mode;
	}
		}

#undef EDGE_LOGGER_SCOPE // SwapchainBuilder

#define EDGE_LOGGER_SCOPE "Buffer"

	auto Buffer::get_gpu_virtual_address() const -> uint64_t {
		auto const& device = allocator_->get_device();

		vk::BufferDeviceAddressInfo address_info{};
		address_info.buffer = handle_;
		return device->get_buffer_device_address(address_info);
	}

#undef // Buffer

#define EDGE_LOGGER_SCOPE "MemoryAllocator"

	MemoryAllocator::MemoryAllocator(Device const& device, VmaAllocator handle)
		: handle_{ handle }
		, device_{ &device } {
	}

	MemoryAllocator::~MemoryAllocator() {
		if (handle_) {
			vmaDestroyAllocator(handle_);
		}
	}

	auto MemoryAllocator::get_memory_propersies(VmaAllocation allocation) const -> VkMemoryPropertyFlags {
		VkMemoryPropertyFlags memory_properties;
		vmaGetAllocationMemoryProperties(handle_, allocation, &memory_properties);
		return memory_properties;
	}

	auto MemoryAllocator::map_memory(VmaAllocation allocation, void** mapped_memory) const -> vk::Result {
		return static_cast<vk::Result>(vmaMapMemory(handle_, allocation, mapped_memory));
	}

	auto MemoryAllocator::unmap_memory(VmaAllocation allocation) const -> void {
		vmaUnmapMemory(handle_, allocation);
	}

	auto MemoryAllocator::flush_memory(VmaAllocation allocation, vk::DeviceSize offset, vk::DeviceSize size) const -> vk::Result {
		return static_cast<vk::Result>(vmaFlushAllocation(handle_, allocation, offset, size));
	}

	auto MemoryAllocator::allocate_image(const vk::ImageCreateInfo& create_info, VmaMemoryUsage usage) const -> Result<Image> {
		VkImage image;
		VmaAllocationInfo allocation_info;
		VmaAllocation allocation;

		VkImageCreateInfo image_create_info = static_cast<VkImageCreateInfo>(create_info);
		VmaAllocationCreateInfo allocation_create_info{};
		allocation_create_info.usage = usage;
		if (auto result = vmaCreateImage(handle_, &image_create_info, &allocation_create_info,
			&image, &allocation, &allocation_info); result != VK_SUCCESS) {
			return std::unexpected(static_cast<vk::Result>(result));
		}

		return Image(*this, vk::Image(image), allocation, allocation_info);
	}

	auto MemoryAllocator::allocate_buffer(const vk::BufferCreateInfo& create_info, VmaMemoryUsage usage) const -> Result<Buffer> {
		VkBuffer buffer;
		VmaAllocationInfo allocation_info;
		VmaAllocation allocation;

		VkBufferCreateInfo buffer_create_info = static_cast<VkBufferCreateInfo>(create_info);
		VmaAllocationCreateInfo allocation_create_info{};
		allocation_create_info.usage = usage;
		if (auto result = vmaCreateBuffer(handle_, &buffer_create_info, &allocation_create_info,
			&buffer, &allocation, &allocation_info); result != VK_SUCCESS) {
			return std::unexpected(static_cast<vk::Result>(result));
		}

		return Buffer(*this, vk::Buffer(buffer), allocation, allocation_info);
	}

	auto MemoryAllocator::deallocate(vk::Image handle, VmaAllocation allocation) const -> void {
		vmaDestroyImage(handle_, (VkImage)handle, allocation);
	}

	auto MemoryAllocator::deallocate(vk::Buffer handle, VmaAllocation allocation) const -> void {
		vmaDestroyBuffer(handle_, (VkBuffer)handle, allocation);
	}

#undef EDGE_LOGGER_SCOPE // MemoryAllocator

}