#include "gfx_context.h"

#include "../platform/platform.h"

#include <vk_mem_alloc.h>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

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

namespace edge::gfx {
	class VulkanLifetime {
	public:
		VulkanLifetime() {
			// Create allocation callbacks
			callbacks_ = std::make_unique<vk::AllocationCallbacks>();
			callbacks_->pfnAllocation = (vk::PFN_AllocationFunction)memalloc;
			callbacks_->pfnFree = (vk::PFN_FreeFunction)memfree;
			callbacks_->pfnReallocation = (vk::PFN_ReallocationFunction)memrealloc;
			callbacks_->pfnInternalAllocation = (vk::PFN_InternalAllocationNotification)internalmemalloc;
			callbacks_->pfnInternalFree = (vk::PFN_InternalFreeNotification)internalmemfree;
			callbacks_->pUserData = this;

			volkInitialize();
			VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr); 
		}

		~VulkanLifetime() {
			volkFinalize();
		}

		static auto get_instance() -> VulkanLifetime& {
			static VulkanLifetime lifetime;
			return lifetime;
		}

		auto get_allocator() const -> vk::AllocationCallbacks const* { return callbacks_.get(); }
	private:
		auto do_allocation(size_t size, size_t alignment, vk::SystemAllocationScope allocation_scope) -> void* {
			return mi_aligned_alloc(alignment, size);
		}

		auto do_deallocation(void* mem) -> void {
			mi_free(mem);
		}

		auto do_reallocation(void* old, size_t size, size_t alignment, vk::SystemAllocationScope allocation_scope) -> void* {
			return mi_aligned_recalloc(old, 1, size, alignment);
		}

		static void* VKAPI_CALL memalloc(void* user_data, size_t size, size_t alignment, vk::SystemAllocationScope allocation_scope) {
			VulkanLifetime* self = static_cast<VulkanLifetime*>(user_data);
			return self ? self->do_allocation(size, alignment, allocation_scope) : nullptr;
		}

		static void VKAPI_CALL memfree(void* user_data, void* mem) {
			VulkanLifetime* self = static_cast<VulkanLifetime*>(user_data);
			self->do_deallocation(mem);
		}

		static void* VKAPI_CALL memrealloc(void* user_data, void* old, size_t size, size_t alignment, vk::SystemAllocationScope allocation_scope) {
			VulkanLifetime* self = static_cast<VulkanLifetime*>(user_data);
			return self ? self->do_reallocation(old, size, alignment, allocation_scope) : nullptr;
		}

		static void VKAPI_CALL internalmemalloc(void* user_data, size_t size, vk::InternalAllocationType allocation_type, vk::SystemAllocationScope allocation_scope) {

		}

		static void VKAPI_CALL internalmemfree(void* user_data, size_t size, vk::InternalAllocationType allocation_type, vk::SystemAllocationScope allocation_scope) {

		}

		std::unique_ptr<vk::AllocationCallbacks> callbacks_;
	};

	vk::detail::DynamicLoader loader_;
	vk::AllocationCallbacks const* allocator_{ nullptr };
	Instance instance_{};
	Surface surface_{};
	Adapter adapter_{};
	Device device_{};
	MemoryAllocator memory_allocator_{};

#define EDGE_LOGGER_SCOPE "Validation"

	VKAPI_ATTR vk::Bool32 VKAPI_CALL debug_utils_messenger_callback(vk::DebugUtilsMessageSeverityFlagBitsEXT message_severity,
		vk::DebugUtilsMessageTypeFlagsEXT message_type,
		const vk::DebugUtilsMessengerCallbackDataEXT* callback_data,
		void* user_data) {
		// Log debug message
		if (message_severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose) {
			EDGE_SLOGT("{} - {}: {}", callback_data->messageIdNumber, callback_data->pMessageIdName, callback_data->pMessage);
		}
		else if (message_severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo) {
			//EDGE_SLOGI("{} - {}: {}", callback_data->messageIdNumber, callback_data->pMessageIdName, callback_data->pMessage);
		}
		else if (message_severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning) {
			EDGE_SLOGW("{} - {}: {}", callback_data->messageIdNumber, callback_data->pMessageIdName, callback_data->pMessage);
		}
		else if (message_severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eError) {
			EDGE_SLOGE("{} - {}: {}", callback_data->messageIdNumber, callback_data->pMessageIdName, callback_data->pMessage);
		}
		return VK_FALSE;
	}

#undef EDGE_LOGGER_SCOPE // Validation
#define EDGE_LOGGER_SCOPE "gfx::util"

	namespace util {
		inline auto enumerate_instance_layer_properties() -> mi::Vector<vk::LayerProperties> {
			uint32_t count;
			auto result = vk::enumerateInstanceLayerProperties(&count, nullptr);
			EDGE_FATAL_VK_ERROR(result, "vk::enumerateInstanceLayerProperties");

			mi::Vector<vk::LayerProperties> output(count);
			result = vk::enumerateInstanceLayerProperties(&count, output.data());
			EDGE_FATAL_VK_ERROR(result, "vk::enumerateInstanceLayerProperties");

			return output;
		}

		inline auto enumerate_instance_extension_properties(const char* layer_name) -> mi::Vector<vk::ExtensionProperties> {
			uint32_t count;
			auto result = vk::enumerateInstanceExtensionProperties(layer_name, &count, nullptr);
			EDGE_FATAL_VK_ERROR(result, "vk::enumerateInstanceExtensionProperties");

			mi::Vector<vk::ExtensionProperties> output(count);
			result = vk::enumerateInstanceExtensionProperties(layer_name, &count, output.data());
			EDGE_FATAL_VK_ERROR(result, "vk::enumerateInstanceExtensionProperties");

			return output;
		}

		inline auto enumerate_physical_devices(vk::Instance instance) -> mi::Vector<vk::PhysicalDevice> {
			uint32_t count;
			auto result = instance.enumeratePhysicalDevices(&count, nullptr);
			EDGE_FATAL_VK_ERROR(result, "enumeratePhysicalDevices");

			mi::Vector<vk::PhysicalDevice> output(count);
			result = instance.enumeratePhysicalDevices(&count, output.data());
			EDGE_FATAL_VK_ERROR(result, "enumeratePhysicalDevices");

			return output;
		}

		inline auto enumerate_device_extension_properties(vk::PhysicalDevice device, const char* layer_name = nullptr) -> mi::Vector<vk::ExtensionProperties> {
			uint32_t count;
			auto result = device.enumerateDeviceExtensionProperties(layer_name, &count, nullptr);
			EDGE_FATAL_VK_ERROR(result, "enumerateDeviceExtensionProperties");

			mi::Vector<vk::ExtensionProperties> output(count);
			result = device.enumerateDeviceExtensionProperties(layer_name, &count, output.data());
			EDGE_FATAL_VK_ERROR(result, "enumerateDeviceExtensionProperties");

			return output;
		}

		inline auto get_queue_family_properties(vk::PhysicalDevice device) -> mi::Vector<vk::QueueFamilyProperties> {
			uint32_t count;
			device.getQueueFamilyProperties(&count, nullptr);

			mi::Vector<vk::QueueFamilyProperties> output(count);
			device.getQueueFamilyProperties(&count, output.data());

			return output;
		}

		inline auto get_surface_formats(vk::PhysicalDevice device, vk::SurfaceKHR surface) -> mi::Vector<vk::SurfaceFormatKHR> {
			uint32_t count;
			auto result = device.getSurfaceFormatsKHR(surface, &count, nullptr);
			EDGE_FATAL_VK_ERROR(result, "getSurfaceFormatsKHR");

			mi::Vector<vk::SurfaceFormatKHR> output(count);
			result = device.getSurfaceFormatsKHR(surface, &count, output.data());
			EDGE_FATAL_VK_ERROR(result, "getSurfaceFormatsKHR");

			return output;
		}

		inline auto get_surface_present_modes(vk::PhysicalDevice device, vk::SurfaceKHR surface) -> mi::Vector<vk::PresentModeKHR> {
			uint32_t count;
			auto result = device.getSurfacePresentModesKHR(surface, &count, nullptr);
			EDGE_FATAL_VK_ERROR(result, "getSurfacePresentModesKHR");

			mi::Vector<vk::PresentModeKHR> output(count);
			result = device.getSurfacePresentModesKHR(surface, &count, output.data());
			EDGE_FATAL_VK_ERROR(result, "getSurfacePresentModesKHR");

			return output;
		}

		inline auto get_swapchain_images(vk::Device device, vk::SwapchainKHR swapchain) -> mi::Vector<vk::Image> {
			uint32_t count;
			auto result = device.getSwapchainImagesKHR(swapchain, &count, nullptr);
			EDGE_FATAL_VK_ERROR(result, "getSwapchainImagesKHR");

			mi::Vector<vk::Image> output(count);
			result = device.getSwapchainImagesKHR(swapchain, &count, output.data());
			EDGE_FATAL_VK_ERROR(result, "getSwapchainImagesKHR");

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

		inline auto is_depth_format(vk::Format format) noexcept -> bool {
			return format == vk::Format::eD16Unorm || format == vk::Format::eD32Sfloat;
		}

		inline auto is_depth_stencil_format(vk::Format format) noexcept -> bool {
			return format == vk::Format::eD32SfloatS8Uint || 
				format == vk::Format::eD16UnormS8Uint || 
				format == vk::Format::eD24UnormS8Uint;
		}
	}

#undef EDGE_LOGGER_SCOPE // gfx util
#define EDGE_LOGGER_SCOPE "gfx::Instance"

	Instance::Instance(vk::Instance handle, vk::DebugUtilsMessengerEXT debug_messenger, mi::Vector<const char*>&& enabled_extensions, mi::Vector<const char*> enabled_layers)
		: Handle{ handle }
		, debug_messenger_{ debug_messenger }
		, enabled_extensions_{ std::move(enabled_extensions) }
		, enabled_layers_{ std::move(enabled_layers) } {
	}

	Instance::~Instance() {
		if (debug_messenger_) {
			handle_.destroyDebugUtilsMessengerEXT(debug_messenger_, allocator_);
			debug_messenger_ = VK_NULL_HANDLE;
		}

		if (handle_) {
			handle_.destroy(allocator_);
			handle_ = VK_NULL_HANDLE;
		}
	}

	auto Instance::is_extension_enabled(const char* extension_name) const -> bool {
		return std::find_if(enabled_extensions_.begin(), enabled_extensions_.end(),
			[&extension_name](const char* name) -> bool {
				return strcmp(extension_name, name) == 0;
			}) != enabled_extensions_.end();
	}

	auto Instance::is_layer_enabled(const char* layer_name) const -> bool {
		return std::find_if(enabled_layers_.begin(), enabled_layers_.end(),
			[&layer_name](const char* name) -> bool {
				return strcmp(layer_name, name) == 0;
			}) != enabled_layers_.end();
	}

	auto Instance::get_adapters() const -> mi::Vector<Adapter> {
		mi::Vector<Adapter> adapters{};

		for (const auto& adapter : util::enumerate_physical_devices(handle_)) {
			mi::Vector<vk::ExtensionProperties> all_device_extensions{};
			for (int32_t layer_index = 0; layer_index < static_cast<int32_t>(enabled_layers_.size() + 1); ++layer_index) {
				auto* layer_name = (layer_index == 0 ? nullptr : enabled_layers_[layer_index - 1]);
				auto layer_ext_props = util::enumerate_device_extension_properties(adapter, layer_name);
				all_device_extensions.insert(all_device_extensions.end(), layer_ext_props.begin(), layer_ext_props.end());
			}

			adapters.push_back(Adapter(adapter, std::move(all_device_extensions)));
		}

		return adapters;
	}

#undef EDGE_LOGGER_SCOPE // Instance
#define EDGE_LOGGER_SCOPE "gfx::InstanceBuilder"

	auto InstanceBuilder::is_valid() const -> bool {
		return 
			app_info_.pApplicationName != nullptr &&
			app_info_.pEngineName != nullptr &&
			app_info_.apiVersion != 0;
	}

	auto InstanceBuilder::build() -> Instance {
		EDGE_FATAL_ERROR(is_valid(), "Invalid instance creation parameters set.");

		// Surface
		if (enable_surface_) {
			add_extension(VK_KHR_SURFACE_EXTENSION_NAME, true);
#if defined(VK_USE_PLATFORM_WIN32_KHR)
			add_extension(VK_KHR_WIN32_SURFACE_EXTENSION_NAME, true);
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
			add_extension(VK_KHR_XLIB_SURFACE_EXTENSION_NAME, true);
#elif defined(VK_USE_PLATFORM_XCB_KHR)
			add_extension(VK_KHR_XCB_SURFACE_EXTENSION_NAME, true);
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
			add_extension(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME, true);
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
			add_extension(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME, true);
#elif defined(VK_USE_PLATFORM_MACOS_MVK)
			add_extension(VK_MVK_MACOS_SURFACE_EXTENSION_NAME, true);
#elif defined(VK_USE_PLATFORM_IOS_MVK)
			add_extension(VK_MVK_IOS_SURFACE_EXTENSION_NAME, true);
#elif defined(VK_USE_PLATFORM_METAL_EXT)
			add_extension(VK_EXT_METAL_SURFACE_EXTENSION_NAME, true);
#else
#error "Unsipported platform"
#endif
		}
		else {
			add_extension(VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME, true);
		}

		// Debug utils
		if (enable_debug_utils_) {
			add_extension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, true);
		}

		// Portability
		if (enable_portability_) {
			add_extension(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME, true);
			add_flag(vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR);
		}

		// Enable validation features
		if ((!validation_feature_enables_.empty() || !validation_feature_disables_.empty())) {
			add_extension(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME, true);
		}

		auto all_layer_properties = util::enumerate_instance_layer_properties();

		auto check_layer_support = [&all_layer_properties](const char* layer_name) {
			return std::find_if(all_layer_properties.begin(), all_layer_properties.end(),
				[layer_name](const VkLayerProperties& props) {
					return std::strcmp(props.layerName, layer_name) == 0;
				}) != all_layer_properties.end();
			};

		// Check enabled layers
		mi::Vector<const char*> enabled_layers{};
		for (const auto& [layer_name, required] : requested_layers_) {
			if (required) {
				EDGE_FATAL_ERROR(check_layer_support(layer_name), "Required layer \"{}\" is not supported.", layer_name);
			}
			enabled_layers.push_back(layer_name);
		}

		// Request all supported extensions including validation layers
		mi::Vector<vk::ExtensionProperties> all_extension_properties{};
		for (int32_t layer_index = 0; layer_index < static_cast<int32_t>(enabled_layers.size() + 1); ++layer_index) {
			const char* layer_name = (layer_index == 0 ? nullptr : enabled_layers[layer_index - 1]);
			auto layer_ext_props = util::enumerate_instance_extension_properties(layer_name);
			all_extension_properties.insert(all_extension_properties.end(), layer_ext_props.begin(), layer_ext_props.end());
		}

		// Helper function
		auto check_ext_support = [&all_extension_properties](const char* extension_name) {
			return std::find_if(all_extension_properties.begin(), all_extension_properties.end(),
				[extension_name](const vk::ExtensionProperties& props) {
					return std::strcmp(props.extensionName, extension_name) == 0;
				}) != all_extension_properties.end();
			};

		// Check enabled extensions
		mi::Vector<const char*> enabled_extensions{};
		for (const auto& [extension_name, required] : requested_extensions_) {
			if (required) {
				EDGE_FATAL_ERROR(check_ext_support(extension_name), "Required extension \"{}\" is not supported.", extension_name);
			}
			enabled_extensions.push_back(extension_name);
		}

#if defined(USE_VALIDATION_LAYERS)
		vk::ValidationFeaturesEXT validation_features_info{};
		if ((!validation_feature_enables_.empty() || !validation_feature_disables_.empty())) {
			validation_features_info.enabledValidationFeatureCount = static_cast<uint32_t>(validation_feature_enables_.size());
			validation_features_info.pEnabledValidationFeatures = validation_feature_enables_.data();
			validation_features_info.disabledValidationFeatureCount = static_cast<uint32_t>(validation_feature_disables_.size());
			validation_features_info.pDisabledValidationFeatures = validation_feature_disables_.data();
			validation_features_info.pNext = create_info_.pNext;
			create_info_.pNext = &validation_features_info;
		}
#endif

#if defined(EDGE_DEBUG)
		vk::DebugUtilsMessengerCreateInfoEXT debug_messenger_create_info{};
		if (enable_debug_utils_) {
			debug_messenger_create_info.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eError | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
				vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo;
			debug_messenger_create_info.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
			debug_messenger_create_info.pfnUserCallback = debug_utils_messenger_callback;
			debug_messenger_create_info.pNext = create_info_.pNext;
			create_info_.pNext = &debug_messenger_create_info;
		}
#endif

		create_info_.enabledExtensionCount = static_cast<uint32_t>(enabled_extensions.size());
		create_info_.ppEnabledExtensionNames = enabled_extensions.empty() ? nullptr : enabled_extensions.data();
		create_info_.enabledLayerCount = static_cast<uint32_t>(enabled_layers.size());
		create_info_.ppEnabledLayerNames = enabled_layers.empty() ? nullptr : enabled_layers.data();

		vk::Instance instance_handle;
		auto result = vk::createInstance(&create_info_, allocator_, &instance_handle);
		EDGE_FATAL_VK_ERROR(result, "vk::createInstance");

		// initialize the Vulkan-Hpp default dispatcher on the instance
		VULKAN_HPP_DEFAULT_DISPATCHER.init(instance_handle);

		// Need to load volk for all the not-yet Vulkan-Hpp calls
		volkLoadInstance(instance_handle);

		vk::DebugUtilsMessengerEXT debug_messenger{ VK_NULL_HANDLE };
#if defined(EDGE_DEBUG)
		result = instance_handle.createDebugUtilsMessengerEXT(&debug_messenger_create_info, allocator_, &debug_messenger);
		EDGE_FATAL_VK_ERROR(result, "createDebugUtilsMessengerEXT");
#endif

		return Instance{ instance_handle, debug_messenger, std::move(enabled_extensions), std::move(enabled_layers) };
	}

#undef EDGE_LOGGER_SCOPE // InstanceBuilder
#define EDGE_LOGGER_SCOPE "gfx::Surface"

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
	auto Surface::create(const vk::AndroidSurfaceCreateInfoKHR& create_info) -> Surface {
		vk::SurfaceKHR surface;
		auto result = instance_->createAndroidSurfaceKHR(&create_info, allocator_, &surface);
		EDGE_FATAL_VK_ERROR(result, "createAndroidSurfaceKHR");
		return Surface{ surface };
	}
#elif defined(VK_USE_PLATFORM_WIN32_KHR)
	auto Surface::create(const vk::Win32SurfaceCreateInfoKHR& create_info) -> Surface {
		vk::SurfaceKHR surface;
		auto result = instance_->createWin32SurfaceKHR(&create_info, allocator_, &surface);
		EDGE_FATAL_VK_ERROR(result, "createWin32SurfaceKHR");
		return Surface{ surface };
	}
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
	auto Surface::create(const vk::XlibSurfaceCreateInfoKHR& create_info) -> Surface {
		vk::SurfaceKHR surface;
		auto result = instance_->createXlibSurfaceKHR(&create_info, allocator_, &surface);
		EDGE_FATAL_VK_ERROR(result, "createXlibSurfaceKHR");
		return Surface{ surface };
	}
#endif

#undef EDGE_LOGGER_SCOPE // Surface
#define EDGE_LOGGER_SCOPE "gfx::Adapter"

	Adapter::Adapter(vk::PhysicalDevice handle, mi::Vector<vk::ExtensionProperties>&& device_extensions)
		: Handle{ handle }
		, supported_extensions_{ std::move(device_extensions) } {
	}

	auto Adapter::get_core_extension_names(uint32_t core_version) const -> mi::Vector<const char*> {
		typedef struct {
			const char* extension_name;
			uint32_t promoted_in_version;
		} ExtensionPromotion;

		static const ExtensionPromotion promoted_extensions[] = {
			// Vulkan 1.1 promotions
			{"VK_KHR_16bit_storage", VK_API_VERSION_1_1},
			{"VK_KHR_bind_memory2", VK_API_VERSION_1_1},
			{"VK_KHR_dedicated_allocation", VK_API_VERSION_1_1},
			{"VK_KHR_descriptor_update_template", VK_API_VERSION_1_1},
			{"VK_KHR_device_group", VK_API_VERSION_1_1},
			{"VK_KHR_device_group_creation", VK_API_VERSION_1_1},
			{"VK_KHR_external_fence", VK_API_VERSION_1_1},
			{"VK_KHR_external_fence_capabilities", VK_API_VERSION_1_1},
			{"VK_KHR_external_memory", VK_API_VERSION_1_1},
			{"VK_KHR_external_memory_capabilities", VK_API_VERSION_1_1},
			{"VK_KHR_external_semaphore", VK_API_VERSION_1_1},
			{"VK_KHR_external_semaphore_capabilities", VK_API_VERSION_1_1},
			{"VK_KHR_get_memory_requirements2", VK_API_VERSION_1_1},
			{"VK_KHR_get_physical_device_properties2", VK_API_VERSION_1_1},
			{"VK_KHR_maintenance1", VK_API_VERSION_1_1},
			{"VK_KHR_maintenance2", VK_API_VERSION_1_1},
			{"VK_KHR_maintenance3", VK_API_VERSION_1_1},
			{"VK_KHR_multiview", VK_API_VERSION_1_1},
			{"VK_KHR_relaxed_block_layout", VK_API_VERSION_1_1},
			{"VK_KHR_sampler_ycbcr_conversion", VK_API_VERSION_1_1},
			{"VK_KHR_shader_draw_parameters", VK_API_VERSION_1_1},
			{"VK_KHR_storage_buffer_storage_class", VK_API_VERSION_1_1},
			{"VK_KHR_variable_pointers", VK_API_VERSION_1_1},

			// Vulkan 1.2 promotions
			{"VK_KHR_8bit_storage", VK_API_VERSION_1_2},
			{"VK_KHR_buffer_device_address", VK_API_VERSION_1_2},
			{"VK_KHR_create_renderpass2", VK_API_VERSION_1_2},
			{"VK_KHR_depth_stencil_resolve", VK_API_VERSION_1_2},
			{"VK_KHR_draw_indirect_count", VK_API_VERSION_1_2},
			{"VK_KHR_driver_properties", VK_API_VERSION_1_2},
			{"VK_KHR_image_format_list", VK_API_VERSION_1_2},
			{"VK_KHR_imageless_framebuffer", VK_API_VERSION_1_2},
			{"VK_KHR_sampler_mirror_clamp_to_edge", VK_API_VERSION_1_2},
			{"VK_KHR_separate_depth_stencil_layouts", VK_API_VERSION_1_2},
			{"VK_KHR_shader_atomic_int64", VK_API_VERSION_1_2},
			{"VK_KHR_shader_float16_int8", VK_API_VERSION_1_2},
			{"VK_KHR_shader_float_controls", VK_API_VERSION_1_2},
			{"VK_KHR_shader_subgroup_extended_types", VK_API_VERSION_1_2},
			{"VK_KHR_spirv_1_4", VK_API_VERSION_1_2},
			{"VK_KHR_timeline_semaphore", VK_API_VERSION_1_2},
			{"VK_KHR_uniform_buffer_standard_layout", VK_API_VERSION_1_2},
			{"VK_KHR_vulkan_memory_model", VK_API_VERSION_1_2},
			{"VK_EXT_descriptor_indexing", VK_API_VERSION_1_2},
			{"VK_EXT_host_query_reset", VK_API_VERSION_1_2},
			{"VK_EXT_sampler_filter_minmax", VK_API_VERSION_1_2},
			{"VK_EXT_scalar_block_layout", VK_API_VERSION_1_2},
			{"VK_EXT_separate_stencil_usage", VK_API_VERSION_1_2},
			{"VK_EXT_shader_viewport_index_layer", VK_API_VERSION_1_2},

			// Vulkan 1.3 promotions
			{"VK_KHR_copy_commands2", VK_API_VERSION_1_3},
			{"VK_KHR_dynamic_rendering", VK_API_VERSION_1_3},
			{"VK_KHR_format_feature_flags2", VK_API_VERSION_1_3},
			{"VK_KHR_maintenance4", VK_API_VERSION_1_3},
			{"VK_KHR_shader_integer_dot_product", VK_API_VERSION_1_3},
			{"VK_KHR_shader_non_semantic_info", VK_API_VERSION_1_3},
			{"VK_KHR_shader_terminate_invocation", VK_API_VERSION_1_3},
			{"VK_KHR_synchronization2", VK_API_VERSION_1_3},
			{"VK_KHR_zero_initialize_workgroup_memory", VK_API_VERSION_1_3},
			{"VK_EXT_4444_formats", VK_API_VERSION_1_3},
			{"VK_EXT_extended_dynamic_state", VK_API_VERSION_1_3},
			{"VK_EXT_extended_dynamic_state2", VK_API_VERSION_1_3},
			{"VK_EXT_image_robustness", VK_API_VERSION_1_3},
			{"VK_EXT_inline_uniform_block", VK_API_VERSION_1_3},
			{"VK_EXT_pipeline_creation_cache_control", VK_API_VERSION_1_3},
			{"VK_EXT_pipeline_creation_feedback", VK_API_VERSION_1_3},
			{"VK_EXT_private_data", VK_API_VERSION_1_3},
			{"VK_EXT_shader_demote_to_helper_invocation", VK_API_VERSION_1_3},
			{"VK_EXT_subgroup_size_control", VK_API_VERSION_1_3},
			{"VK_EXT_texel_buffer_alignment", VK_API_VERSION_1_3},
			{"VK_EXT_texture_compression_astc_hdr", VK_API_VERSION_1_3},
			{"VK_EXT_tooling_info", VK_API_VERSION_1_3},
			{"VK_EXT_ycbcr_2plane_444_formats", VK_API_VERSION_1_3},

			// Vulkan 1.4 promotions
			{"VK_KHR_dynamic_rendering_local_read", VK_API_VERSION_1_4},
			{"VK_EXT_host_image_copy", VK_API_VERSION_1_4},
			{"VK_KHR_push_descriptor", VK_API_VERSION_1_4},
			{"VK_EXT_pipeline_protected_access", VK_API_VERSION_1_4},
			{"VK_KHR_line_rasterization", VK_API_VERSION_1_4},
			{"VK_KHR_shader_subgroup_rotate", VK_API_VERSION_1_4},
			{"VK_KHR_global_priority", VK_API_VERSION_1_4},
			{"VK_KHR_shader_float_controls2", VK_API_VERSION_1_4},
			{"VK_KHR_shader_expect_assume", VK_API_VERSION_1_4},
			{"VK_KHR_maintenance5", VK_API_VERSION_1_4},
			{"VK_KHR_maintenance6", VK_API_VERSION_1_4},
			{"VK_EXT_index_type_uint8", VK_API_VERSION_1_4},
			{"VK_EXT_pipeline_robustness", VK_API_VERSION_1_4},
			{"VK_EXT_vertex_attribute_divisor", VK_API_VERSION_1_4}
		};

		mi::Vector<const char*> output{};
		for (int32_t i = 0; i < std::size(promoted_extensions); ++i) {
			auto& extension = promoted_extensions[i];
			if (is_supported(extension.extension_name) && extension.promoted_in_version <= core_version) {
				output.push_back(extension.extension_name);
			}
		}
		return output;
	}

	auto Adapter::is_supported(const char* extension_name) const -> bool {
		return std::find_if(supported_extensions_.begin(), supported_extensions_.end(),
			[&extension_name](const vk::ExtensionProperties& props) -> bool {
				return strcmp(extension_name, props.extensionName) == 0;
			}) != supported_extensions_.end();
	}

#undef EDGE_LOGGER_SCOPE // Adapter
#define EDGE_LOGGER_SCOPE "gfx::Device"

	Device::Device(vk::Device handle, mi::Vector<const char*>&& enabled_extensions, mi::Vector<QueueFamilyInfo>&& queue_families)
		: Handle{ handle }
		, enabled_extensions_{ std::move(enabled_extensions) }
		, queue_families_{ std::move(queue_families) } {
	}

	Device::~Device() {
		if (handle_) {
			handle_.destroy(allocator_);
		}
	}

	auto Device::QueueFamilyInfo::calculate_score(const QueueRequest& request) const -> int32_t {
		if (request.strategy == EDGE_GFX_QUEUE_SELECTION_STRATEGY_EXACT) {
			return (capabilities == request.required_caps) ? 1000 : -1;
		}

		if ((capabilities & request.required_caps) != request.required_caps) {
			return -1;
		}

		int32_t score = 100;

		switch (request.strategy) {
		case EDGE_GFX_QUEUE_SELECTION_STRATEGY_PREFER_DEDICATED: {
			// Prefer queues with fewer extra capabilities
			// Count bits in (capabilities - required_caps)
			auto extra_caps = static_cast<uint32_t>(capabilities & ~request.required_caps);
			int32_t extra_count = std::popcount(extra_caps);
			score -= extra_count * 10; // Penalty for each extra capability
			break;
		}

		case EDGE_GFX_QUEUE_SELECTION_STRATEGY_PREFER_SHARED: {
			// Prefer queues with more capabilities
			auto all_caps = static_cast<uint32_t>(capabilities);
			int32_t cap_count = std::popcount(all_caps);
			score += cap_count * 5; // Bonus for each capability
			break;
		}

		case EDGE_GFX_QUEUE_SELECTION_STRATEGY_MINIMAL:
		default:
			// No special preference, just meets requirements
			break;
		}

		if (request.preferred_caps != EDGE_GFX_QUEUE_CAPABILITY_NONE) {
			if ((capabilities & request.preferred_caps) == request.preferred_caps) {
				score += 30;
			}
			else {
				auto matched_preferred = static_cast<uint32_t>(capabilities & request.preferred_caps);
				score += std::popcount(matched_preferred) * 5;
			}
		}

		score += std::min<int32_t>(static_cast<int32_t>(available_queue_indices.size()), 10);

		if (capabilities & EDGE_GFX_QUEUE_CAPABILITY_PRESENT) {
			score += 2;
		}

		return score;
	}

	auto Device::get_queue(const QueueRequest& request) -> Result<Queue> {
		int32_t best_score = -1;
		QueueFamilyInfo* best_family = nullptr;

		for (auto& family : queue_families_) {
			if (family.available_queue_indices.empty()) {
				continue;
			}

			if (request.prefer_separate_family && used_queue_families_.contains(family.family_index)) {
				continue;
			}

			int32_t score = family.calculate_score(request);
			if (score > best_score) {
				best_score = score;
				best_family = &family;
			}
		}

		if (!best_family || best_score < 0) {
			EDGE_LOGW("No suitable queue family found for request (required caps: {})", request.required_caps);

			EDGE_LOGD("Available queue families:");
			for (const auto& family : queue_families_) {
				EDGE_LOGD("  Family {}: caps={}, available={}", family.family_index, family.capabilities, family.available_queue_indices.size());
			}

			return std::unexpected(vk::Result::eErrorFeatureNotPresent);
		}

		uint32_t queue_index = best_family->available_queue_indices.back();
		best_family->available_queue_indices.pop_back();

		used_queue_families_.insert(best_family->family_index);

		vk::Queue queue;
		handle_.getQueue(best_family->family_index, queue_index, &queue);

#ifndef NDEBUG
		EDGE_SLOGD("Allocated queue: family={}, index={}, caps={}, score={}",
			best_family->family_index, queue_index, best_family->capabilities, best_score);
#endif

		return Queue(queue, best_family->family_index, queue_index);
	}

	auto Device::get_queue_family(const QueueRequest& request) -> Result<uint32_t> {
		int32_t best_score = -1;
		uint32_t best_family_index = ~0u;

		for (const auto& family : queue_families_) {
			if (family.available_queue_indices.empty()) {
				continue; // No queues available
			}

			int32_t score = family.calculate_score(request);
			if (score > best_score) {
				best_score = score;
				best_family_index = family.family_index;
			}
		}

		if (best_score < 0) {
			EDGE_LOGW("No suitable queue family found for request");
			return std::unexpected(vk::Result::eErrorFeatureNotPresent);
		}

		return best_family_index;
	}

	auto Device::is_enabled(const char* extension_name) const -> bool {
		return std::find_if(enabled_extensions_.begin(), enabled_extensions_.end(),
			[&extension_name](const char* name) -> bool {
				return strcmp(extension_name, name) == 0;
			}) != enabled_extensions_.end();
	}

#undef EDGE_LOGGER_SCOPE // Device
#define EDGE_LOGGER_SCOPE "gfx::DeviceSelector"

	auto DeviceSelector::select() -> std::tuple<Adapter, Device> {
		int32_t best_candidate_index{ -1 };
		int32_t fallback_index{ -1 };

		auto adapters = instance_.get_adapters();

		mi::Vector<mi::Vector<const char*>> per_device_extensions(adapters.size());
		for (int32_t device_idx = 0; device_idx < static_cast<int32_t>(adapters.size()); ++device_idx) {
			auto const& adapter = adapters[device_idx].get_handle();

			vk::PhysicalDeviceProperties properties;
			adapter.getProperties(&properties);

			auto available_extensions = util::enumerate_device_extension_properties(adapter, nullptr);

			// No extensions found. Bug?
			if (available_extensions.empty()) {
				GFX_ASSERT_MSG(false, "Device \"{}\" have no supported extensions. Check driver.", std::string_view(properties.deviceName));
				continue;
			}

			auto check_ext_support = [&available_extensions](const char* extension_name) -> bool {
				return std::find_if(available_extensions.begin(), available_extensions.end(),
					[extension_name](const vk::ExtensionProperties& props) {
						return std::strcmp(props.extensionName, extension_name) == 0;
					}) != available_extensions.end();
				};

			auto& requested_extensions = per_device_extensions[device_idx];
			requested_extensions = adapters[device_idx].get_core_extension_names(minimal_api_ver);

			auto check_ext_alresdy_enabled = [&requested_extensions](const char* extension_name) -> bool {
				return std::find_if(requested_extensions.begin(), requested_extensions.end(), [&extension_name](const char* name) {
					return strcmp(name, extension_name) == 0;
					}) != requested_extensions.end();
				};

			// Check for all required extension support
			bool all_extension_supported{ true };
			for (auto& ext : requested_extensions_) {
				if (check_ext_support(ext.first)) {
					if (!check_ext_alresdy_enabled(ext.first)) {
						requested_extensions.push_back(ext.first);
					}
					continue;
				}

				if (ext.second) {
					EDGE_SLOGE("Device \"{}\" is not support required extension \"{}\"", std::string_view(properties.deviceName), ext.first);
					all_extension_supported = false;
				}

				EDGE_SLOGW("Device \"{}\" is not support optional extension \"{}\"", std::string_view(properties.deviceName), ext.first);
				continue;
			}

			// TODO: Add all extension names that is in requested core version

			// Can't use this device, because some extensions is not supported
			if (!all_extension_supported) {
				continue;
			}

			// Check that device have queue with present support
			if (surface_) {
				auto queue_family_props = util::get_queue_family_properties(adapter);

				bool surface_supported{ false };
				for (uint32_t queue_family_index = 0; queue_family_index < static_cast<uint32_t>(queue_family_props.size()); ++queue_family_index) {
					vk::Bool32 supported;
					if (auto result = adapter.getSurfaceSupportKHR(queue_family_index, surface_, &supported); result == vk::Result::eSuccess && supported == VK_TRUE) {
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
			EDGE_FATAL_ERROR(fallback_index != -1, "No suitable graphics adapter found.");
			selected_device_index = fallback_index;
		}

		auto const& selected_adapter = adapters[selected_device_index].get_handle();;
		auto& enabled_extensions = per_device_extensions[selected_device_index];

		vk::PhysicalDeviceProperties properties;
		selected_adapter.getProperties(&properties);

		auto queue_family_properties = util::get_queue_family_properties(selected_adapter);

		EDGE_SLOGD("{} device \"{}\" selected.", to_string(properties.deviceType), std::string_view(properties.deviceName));

		mi::Vector<vk::DeviceQueueCreateInfo> queue_create_infos(queue_family_properties.size());
		mi::Vector<mi::Vector<float>> family_queue_priorities(queue_family_properties.size());
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

		auto api_version = false ? properties.apiVersion : minimal_api_ver;

		void* feature_chain{ nullptr };
		if (api_version >= VK_API_VERSION_1_4) {
			feature_chain = &features14;
			features14.pNext = &features13;
			features13.pNext = &features12;
			features12.pNext = &features11;
			features11.pNext = last_feature_ptr_;
		}
		else if (api_version >= VK_API_VERSION_1_3) {
			feature_chain = &features13;
			features13.pNext = &features12;
			features12.pNext = &features11;
			features11.pNext = last_feature_ptr_;
		}
		else if (api_version >= VK_API_VERSION_1_2) {
			feature_chain = &features12;
			features12.pNext = &features11;
			features11.pNext = last_feature_ptr_;
		}
		else if (api_version >= VK_API_VERSION_1_1) {
			feature_chain = &features11;
			features11.pNext = last_feature_ptr_;
		}
		else {
			feature_chain = last_feature_ptr_;
		}

		vk::PhysicalDeviceFeatures2 features2{};
		features2.pNext = feature_chain;
		selected_adapter.getFeatures2(&features2);

		create_info.pEnabledFeatures = &features2.features;
		create_info.pNext = feature_chain;

		vk::Device device;
		auto result = selected_adapter.createDevice(&create_info, allocator_, &device);
		EDGE_FATAL_VK_ERROR(result, "createDevice");

		auto queue_families = build_queue_families(selected_adapter, surface_);

		return std::tuple(std::move(adapters[selected_device_index]), Device{ device, std::move(enabled_extensions), std::move(queue_families) });
	}

	auto DeviceSelector::build_queue_families(vk::PhysicalDevice adapter, vk::SurfaceKHR surface) const -> mi::Vector<Device::QueueFamilyInfo> {
		auto queue_family_properties = util::get_queue_family_properties(adapter);

		mi::Vector<Device::QueueFamilyInfo> queue_families;
		queue_families.reserve(queue_family_properties.size());

		for (uint32_t index = 0; index < queue_family_properties.size(); ++index) {
			auto const& props = queue_family_properties[index];

			Device::QueueFamilyInfo family_info{};
			family_info.family_index = index;
			family_info.queue_flags = props.queueFlags;
			family_info.queue_count = props.queueCount;
			family_info.timestamp_valid_bits = props.timestampValidBits;
			family_info.min_image_transfer_granularity = props.minImageTransferGranularity;
			family_info.supports_present = false;

			QueueCapabilityFlags caps = EDGE_GFX_QUEUE_CAPABILITY_NONE;

			if (props.queueFlags & vk::QueueFlagBits::eGraphics) {
				caps |= EDGE_GFX_QUEUE_CAPABILITY_GRAPHICS;
			}
			if (props.queueFlags & vk::QueueFlagBits::eCompute) {
				caps |= EDGE_GFX_QUEUE_CAPABILITY_COMPUTE;
			}
			if (props.queueFlags & vk::QueueFlagBits::eTransfer) {
				caps |= EDGE_GFX_QUEUE_CAPABILITY_TRANSFER;
			}
			if (props.queueFlags & vk::QueueFlagBits::eSparseBinding) {
				caps |= EDGE_GFX_QUEUE_CAPABILITY_SPARSE_BINDING;
			}
			if (props.queueFlags & vk::QueueFlagBits::eProtected) {
				caps |= EDGE_GFX_QUEUE_CAPABILITY_PROTECTED;
			}

			if (props.queueFlags & vk::QueueFlagBits::eVideoDecodeKHR) {
				caps |= EDGE_GFX_QUEUE_CAPABILITY_VIDEO_DECODE;
			}
			if (props.queueFlags & vk::QueueFlagBits::eVideoEncodeKHR) {
				caps |= EDGE_GFX_QUEUE_CAPABILITY_VIDEO_ENCODE;
			}

			if (props.queueFlags & vk::QueueFlagBits::eOpticalFlowNV) {
				caps |= EDGE_GFX_QUEUE_CAPABILITY_OPTICAL_FLOW;
			}

			if (surface) {
				vk::Bool32 supported;
				if (auto result = adapter.getSurfaceSupportKHR(index, surface, &supported);
					result == vk::Result::eSuccess && supported == VK_TRUE) {
					caps |= EDGE_GFX_QUEUE_CAPABILITY_PRESENT;
					family_info.supports_present = true;
				}
			}

			family_info.capabilities = caps;

			family_info.available_queue_indices.resize(props.queueCount);
			std::iota(family_info.available_queue_indices.rbegin(), family_info.available_queue_indices.rend(), 0u);

			queue_families.push_back(std::move(family_info));

#ifndef NDEBUG
			EDGE_SLOGD("Discovered Queue Family {}: count={}, caps={}", index, props.queueCount, caps);
#endif
		}

		return queue_families;
	}

#undef EDGE_LOGGER_SCOPE // DeviceSelector
#define EDGE_LOGGER_SCOPE "gfx::Queue"

	auto Queue::create(const QueueRequest& request) -> Result<Queue> {
		int32_t best_score = -1;
		return {};
	}

	auto Queue::create_command_pool() const -> CommandPool {
		vk::CommandPoolCreateInfo create_info{};
		create_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
		create_info.queueFamilyIndex = family_index_;

		vk::CommandPool command_pool;
		auto result = device_->createCommandPool(&create_info, allocator_, &command_pool);
		EDGE_FATAL_VK_ERROR(result, "createCommandPool");

		return CommandPool{ command_pool };
	}

	auto Fence::create(vk::FenceCreateFlags flags) -> Fence {
		vk::Fence handle;
		vk::FenceCreateInfo create_info{};
		create_info.flags = flags;
		auto result = device_->createFence(&create_info, allocator_, &handle);
		EDGE_FATAL_VK_ERROR(result, "createFence");

		return Fence{ handle };
	}

	auto Fence::wait(uint64_t timeout) const -> vk::Result {
		vk::Device device = *device_;
		return device.waitForFences(1u, &handle_, VK_TRUE, timeout);
	}

	auto Fence::reset() const -> vk::Result {
		vk::Device device = *device_;
		return device.resetFences(1u, &handle_);
	}

	auto Semaphore::create(vk::SemaphoreType type, uint64_t initial_value) -> Semaphore {
		vk::Semaphore handle;
		vk::SemaphoreTypeCreateInfo semaphore_type_create_info{};
		semaphore_type_create_info.semaphoreType = type;
		vk::SemaphoreCreateInfo create_info{};
		create_info.pNext = &semaphore_type_create_info;
		auto result = device_->createSemaphore(&create_info, allocator_, &handle);
		EDGE_FATAL_VK_ERROR(result, "createSemaphore");

		return Semaphore{ handle };
	}

#undef EDGE_LOGGER_SCOPE // Queue
#define EDGE_LOGGER_SCOPE "gfx::Image"

	auto Image::create(const ImageCreateInfo& create_info) -> Image {
		VmaAllocationCreateInfo allocation_create_info{};
		allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO;

		vk::ImageCreateInfo image_create_info{};
		image_create_info.extent = create_info.extent;
		image_create_info.arrayLayers = create_info.layer_count * create_info.face_count;
		image_create_info.mipLevels = create_info.level_count;
		image_create_info.format = create_info.format;
		image_create_info.flags = (create_info.face_count == 6u) ? vk::ImageCreateFlagBits::eCubeCompatible : vk::ImageCreateFlagBits::eExtendedUsage;
		image_create_info.imageType = (create_info.extent.depth > 1u) ? vk::ImageType::e3D : (create_info.extent.height > 1u) ? vk::ImageType::e2D : vk::ImageType::e1D;
		image_create_info.sharingMode = vk::SharingMode::eExclusive;

		if (create_info.flags & EDGE_GFX_IMAGE_FLAG_SAMPLE) {
			image_create_info.usage |= vk::ImageUsageFlagBits::eSampled;
		}

		if (create_info.flags & EDGE_GFX_IMAGE_FLAG_SAMPLE) {
			image_create_info.usage |= vk::ImageUsageFlagBits::eStorage;
		}

		if (create_info.flags & EDGE_GFX_IMAGE_FLAG_COPY_SOURCE) {
			image_create_info.usage |= vk::ImageUsageFlagBits::eTransferSrc;
		}

		if (create_info.flags & EDGE_GFX_IMAGE_FLAG_COPY_TARGET) {
			image_create_info.usage |= vk::ImageUsageFlagBits::eTransferDst;
		}

		if (create_info.flags & EDGE_GFX_IMAGE_FLAG_WRITE_COLOR) {
			image_create_info.usage |= ((util::is_depth_stencil_format(create_info.format) || util::is_depth_format(create_info.format)) ? vk::ImageUsageFlagBits::eDepthStencilAttachment : vk::ImageUsageFlagBits::eColorAttachment);
			allocation_create_info.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
			allocation_create_info.priority = 1.0f;
		}

		auto queue_family_properties = util::get_queue_family_properties(*adapter_);
		mi::Vector<uint32_t> queue_family_indices(queue_family_properties.size());
		std::iota(queue_family_indices.begin(), queue_family_indices.end(), 0);

		if (queue_family_indices.size() > 1) {
			image_create_info.queueFamilyIndexCount = static_cast<uint32_t>(queue_family_indices.size());
			image_create_info.pQueueFamilyIndices = queue_family_indices.data();
			image_create_info.sharingMode = vk::SharingMode::eConcurrent;
		}

		return memory_allocator_.allocate_image(image_create_info, allocation_create_info);
	}

	auto Image::create_view(const vk::ImageSubresourceRange& range, vk::ImageViewType type) -> ImageView {
		vk::ImageViewCreateInfo create_info{};
		create_info.image = handle_;
		create_info.format = create_info_.format;
		create_info.components.r = vk::ComponentSwizzle::eR;
		create_info.components.g = vk::ComponentSwizzle::eG;
		create_info.components.b = vk::ComponentSwizzle::eB;
		create_info.components.a = vk::ComponentSwizzle::eA;
		create_info.subresourceRange = range;
		create_info.viewType = type;

		vk::ImageView image_view;
		auto result = device_->createImageView(&create_info, allocator_, &image_view);
		EDGE_FATAL_VK_IMAGE_ERROR(result == vk::Result::eSuccess, "createImageView", create_info_.extent, create_info_.format, range.levelCount, range.layerCount);

		return ImageView{ image_view, range };
	}

	auto Image::get_face_count() const noexcept -> uint32_t {
		if (create_info_.flags & vk::ImageCreateFlagBits::eCubeCompatible) {
			return 6u;
		}
		return 1u;
	}

	auto Image::get_layer_count() const noexcept -> uint32_t {
		if (create_info_.flags & vk::ImageCreateFlagBits::eCubeCompatible) {
			return create_info_.arrayLayers / 6u;
		}
		return create_info_.arrayLayers;
	}

#undef EDGE_LOGGER_SCOPE // Image
#define EDGE_LOGGER_SCOPE "gfx::Buffer"

	auto Buffer::create(const BufferCreateInfo& create_info) -> Buffer {
		vk::PhysicalDeviceProperties properties;
		adapter_->getProperties(&properties);

		uint64_t minimal_alignment{ create_info.minimal_alignment };
		vk::BufferCreateInfo buffer_create_info{};
		buffer_create_info.usage |= vk::BufferUsageFlagBits::eShaderDeviceAddressKHR;

		VmaAllocationCreateInfo allocation_create_info{};
		allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO;

		if (create_info.flags & EDGE_GFX_BUFFER_FLAG_DYNAMIC) {
			allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
			allocation_create_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
				VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
				VMA_ALLOCATION_CREATE_MAPPED_BIT;
		}
		else if (create_info.flags & EDGE_GFX_BUFFER_FLAG_READBACK) {
			buffer_create_info.usage |= vk::BufferUsageFlagBits::eTransferDst;
			allocation_create_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
		}
		else if (create_info.flags & EDGE_GFX_BUFFER_FLAG_STAGING) {
			buffer_create_info.usage |= vk::BufferUsageFlagBits::eTransferSrc;
			allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
			allocation_create_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
		}

		if (create_info.flags & EDGE_GFX_BUFFER_FLAG_UNIFORM) {
			buffer_create_info.usage |= vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst;
			minimal_alignment = std::lcm(properties.limits.minUniformBufferOffsetAlignment, properties.limits.nonCoherentAtomSize);
		}
		else if (create_info.flags & EDGE_GFX_BUFFER_FLAG_STORAGE) {
			buffer_create_info.usage |= vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst;
			minimal_alignment = std::max(minimal_alignment, properties.limits.minStorageBufferOffsetAlignment);
		}
		else if (create_info.flags & EDGE_GFX_BUFFER_FLAG_VERTEX) {
			buffer_create_info.usage |= vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst;
			minimal_alignment = std::max<uint64_t>(minimal_alignment, 4ull);
		}
		else if (create_info.flags & EDGE_GFX_BUFFER_FLAG_INDEX) {
			buffer_create_info.usage |= vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst;
			minimal_alignment = std::max<uint64_t>(minimal_alignment, 1ull);
		}
		else if (create_info.flags & EDGE_GFX_BUFFER_FLAG_INDIRECT) {
			buffer_create_info.usage |= vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eTransferDst;
		}
		else if (create_info.flags & EDGE_GFX_BUFFER_FLAG_ACCELERATION_BUILD) {
			buffer_create_info.usage |= vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst;
		}
		else if (create_info.flags & EDGE_GFX_BUFFER_FLAG_ACCELERATION_STORE) {
			buffer_create_info.usage |= vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eTransferDst;
		}
		else if (create_info.flags & EDGE_GFX_BUFFER_FLAG_SHADER_BINDING_TABLE) {
			buffer_create_info.usage |= vk::BufferUsageFlagBits::eShaderBindingTableKHR | vk::BufferUsageFlagBits::eTransferDst;
		}

		buffer_create_info.size = aligned_size(create_info.size, minimal_alignment) * create_info.count;

		return memory_allocator_.allocate_buffer(buffer_create_info, allocation_create_info);
	}

	auto Buffer::create_view(vk::DeviceSize size, vk::DeviceSize offset, vk::Format format) -> BufferView {
		vk::BufferViewCreateInfo create_info{};
		create_info.buffer = handle_;
		create_info.format = format;
		create_info.offset = offset;
		create_info.range = size;

		vk::BufferView buffer_view;
		auto result = device_->createBufferView(&create_info, allocator_, &buffer_view);
		EDGE_FATAL_VK_BUFFER_ERROR(result != vk::Result::eSuccess, "createBufferView", create_info_.size, offset, size);

		return BufferView{ buffer_view, offset, size, format };
	}

	auto Buffer::get_device_address() const -> vk::DeviceAddress {
		vk::BufferDeviceAddressInfoKHR address_info{};
		address_info.buffer = handle_;

		return device_->getBufferAddressKHR(&address_info);
	}

#undef EDGE_LOGGER_SCOPE
#define EDGE_LOGGER_SCOPE "gfx::MemoryAllocator"

	MemoryAllocator::~MemoryAllocator() {
		if (handle_) {
			vmaDestroyAllocator(handle_);
			handle_ = VK_NULL_HANDLE;
		}
	}

	auto MemoryAllocator::allocate_image(const vk::ImageCreateInfo& create_info, const VmaAllocationCreateInfo& allocation_create_info) const -> Image {
		VkImage image;
		VmaAllocationInfo allocation_info;
		VmaAllocation allocation;

		VkImageCreateInfo image_create_info = static_cast<VkImageCreateInfo>(create_info);
		auto result = vmaCreateImage(handle_, &image_create_info, &allocation_create_info, &image, &allocation, &allocation_info);
		EDGE_FATAL_VK_ERROR(static_cast<vk::Result>(result), "vmaCreateImage");

		return Image(vk::Image(image), allocation, allocation_info, create_info);
	}

	auto MemoryAllocator::allocate_buffer(const vk::BufferCreateInfo& create_info, const VmaAllocationCreateInfo& allocation_create_info) const -> Buffer {
		VkBuffer buffer;
		VmaAllocationInfo allocation_info;
		VmaAllocation allocation;

		VkBufferCreateInfo buffer_create_info = static_cast<VkBufferCreateInfo>(create_info);
		auto result = vmaCreateBuffer(handle_, &buffer_create_info, &allocation_create_info, &buffer, &allocation, &allocation_info);
		EDGE_FATAL_VK_ERROR(static_cast<vk::Result>(result), "vmaCreateBuffer");

		return Buffer(vk::Buffer(buffer), allocation, allocation_info, create_info);
	}

	auto BufferRange::create(Buffer const* buffer, vk::DeviceSize offset, vk::DeviceSize size) -> BufferRange {
		BufferRange self{ buffer->get_handle(), offset};
		self._construct(buffer, size);
		return self;
	}

	auto BufferRange::make_buffer_region_update(vk::DeviceSize src_offset, vk::DeviceSize dst_offset, vk::DeviceSize size) const -> vk::BufferCopy2KHR {
		return { offset_ + src_offset, dst_offset, size };
	}

	auto BufferRange::make_image_region_update(vk::DeviceSize src_offset, vk::ImageSubresourceLayers subresource_layers, vk::Offset3D offset, vk::Extent3D extent) const -> vk::BufferImageCopy2KHR {
		return { offset_ + src_offset, 0u, 0u, subresource_layers, offset, extent };
	}

	auto BufferRange::write(const void* data, vk::DeviceSize size, vk::DeviceSize offset) -> void {
		std::memcpy(range_.data() + offset, data, size);
	}

	auto BufferRange::_construct(Buffer const* buffer, vk::DeviceSize size) -> void {
		auto mapped_range = buffer->map();
		EDGE_FATAL_ERROR(mapped_range.size() - offset_ > size, "Not enough mapped memory.");

		range_ = mapped_range.subspan(offset_, size);
	}

#undef EDGE_LOGGER_SCOPE
#define EDGE_LOGGER_SCOPE "gfx::Swapchain"

	auto Swapchain::reset() -> void {
		handle_ = VK_NULL_HANDLE;
	}

	auto Swapchain::get_images() const -> mi::Vector<Image> {
		vk::Device device = *device_;

		mi::Vector<Image> images{};
		for (auto& image : util::get_swapchain_images(device, handle_)) {
			vk::ImageCreateInfo image_create_info{};
			image_create_info.imageType = vk::ImageType::e2D;
			image_create_info.format = state_.format.format;
			image_create_info.extent.width = state_.extent.width;
			image_create_info.extent.height = state_.extent.height;
			image_create_info.extent.depth = 1u;
			image_create_info.mipLevels = 1u;
			image_create_info.arrayLayers = 1u;
			image_create_info.samples = vk::SampleCountFlagBits::e1;
			image_create_info.tiling = vk::ImageTiling::eOptimal;
			image_create_info.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst;
			image_create_info.sharingMode = vk::SharingMode::eExclusive;
			image_create_info.initialLayout = vk::ImageLayout::eUndefined;

			images.emplace_back(image, image_create_info);
		}

		return images;
	}

#undef EDGE_LOGGER_SCOPE // Swapchain

#define EDGE_LOGGER_SCOPE "gfx::SwapchainBuilder"

	auto SwapchainBuilder::build() -> Swapchain {
		vk::PresentModeKHR present_mode = (requested_state_.vsync) ? vk::PresentModeKHR::eFifo : vk::PresentModeKHR::eImmediate;
		std::array<vk::PresentModeKHR, 3ull> present_mode_priority_list{
#ifdef VK_USE_PLATFORM_ANDROID_KHR
			vk::PresentModeKHR::eFifo, vk::PresentModeKHR::eMailbox,
#else
			vk::PresentModeKHR::eImmediate, vk::PresentModeKHR::eMailbox,
#endif
			vk::PresentModeKHR::eFifo };

		auto surface_formats = util::get_surface_formats(adapter_, *surface_);
		auto present_modes = util::get_surface_present_modes(adapter_, *surface_);

		// Choose best properties based on surface capabilities
		vk::SurfaceCapabilitiesKHR surface_capabilities;
		auto result = adapter_->getSurfaceCapabilitiesKHR(*surface_, &surface_capabilities);
		EDGE_FATAL_VK_ERROR(result, "getSurfaceCapabilitiesKHR");

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
		vk::FormatProperties format_properties;
		adapter_->getFormatProperties(create_info.imageFormat, &format_properties);
		create_info.imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst;

		//create_info.preTransform = ((requested_state_.transform & surface_capabilities.supportedTransforms) == requested_state_.transform) ? requested_state_.transform : surface_capabilities.currentTransform;
        create_info.preTransform = vk::SurfaceTransformFlagBitsKHR::eIdentity;
		create_info.compositeAlpha = choose_suitable_composite_alpha(vk::CompositeAlphaFlagBitsKHR::eInherit, surface_capabilities.supportedCompositeAlpha);
		create_info.presentMode = choose_suitable_present_mode(present_mode, present_modes, present_mode_priority_list);

		create_info.surface = surface_.get_handle();
		create_info.clipped = VK_TRUE;
		create_info.imageSharingMode = vk::SharingMode::eExclusive;

		auto queue_family_properties = util::get_queue_family_properties(adapter_);
		mi::Vector<uint32_t> queue_family_indices(queue_family_properties.size());
		std::iota(queue_family_indices.begin(), queue_family_indices.end(), 0);

		if (queue_family_indices.size() > 1) {
			create_info.queueFamilyIndexCount = static_cast<uint32_t>(queue_family_indices.size());
			create_info.pQueueFamilyIndices = queue_family_indices.data();
			create_info.imageSharingMode = vk::SharingMode::eConcurrent;
		}

		vk::Device device = *device_;

		vk::SwapchainKHR swapchain{ VK_NULL_HANDLE };
		result = device.createSwapchainKHR(&create_info, allocator_, &swapchain);
		EDGE_FATAL_VK_ERROR(result, "createSwapchainKHR");

		Swapchain::State new_state{
			.image_count = create_info.minImageCount,
			.format = surface_format,
			.extent = create_info.imageExtent,
			.transform = create_info.preTransform,
			.vsync = requested_state_.vsync,
			.hdr = requested_state_.hdr && util::is_hdr_format(create_info.imageFormat) && util::is_hdr_color_space(create_info.imageColorSpace)
		};
		return Swapchain(swapchain, new_state);
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

	auto SwapchainBuilder::choose_surface_format(const vk::SurfaceFormatKHR requested_surface_format, const mi::Vector<vk::SurfaceFormatKHR>& available_surface_formats, bool prefer_hdr) -> vk::SurfaceFormatKHR {
		// Separate formats by hdr support
		mi::Vector<vk::SurfaceFormatKHR> sdr_fromats{};
		mi::Vector<vk::SurfaceFormatKHR> hdr_fromats{};
		for (const auto& surface_format : available_surface_formats) {
			if (util::is_hdr_format(surface_format.format) && util::is_hdr_color_space(surface_format.colorSpace)) {
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

		static const std::array<vk::CompositeAlphaFlagBitsKHR, 4> composite_alpha_priority_list = {
			vk::CompositeAlphaFlagBitsKHR::eOpaque, vk::CompositeAlphaFlagBitsKHR::ePreMultiplied,
			vk::CompositeAlphaFlagBitsKHR::ePostMultiplied, vk::CompositeAlphaFlagBitsKHR::eInherit
		};

		auto const chosen_composite_alpha_it = std::find_if(composite_alpha_priority_list.begin(), composite_alpha_priority_list.end(),
			[&supported_composite_alpha](vk::CompositeAlphaFlagBitsKHR composite_alpha) { return composite_alpha & supported_composite_alpha; });

		if (chosen_composite_alpha_it == composite_alpha_priority_list.end()) {
			GFX_ASSERT_MSG(false, "No compatible composite alpha found.");
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

#define EDGE_LOGGER_SCOPE "gfx::CommandBuffer"

	auto CommandBuffer::begin() const -> vk::Result {
		if (auto result = handle_.reset(vk::CommandBufferResetFlagBits::eReleaseResources); result != vk::Result::eSuccess) {
			return result;
		}

		vk::CommandBufferBeginInfo begin_info{};
		begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

		return handle_.begin(&begin_info);
	}

	auto CommandBuffer::end() const -> vk::Result {
		return handle_.end();
	}

	auto CommandBuffer::push_barrier(const Barrier& barrier) const -> void {
		mi::Vector<vk::MemoryBarrier2KHR> memory_barriers{};

		mi::Vector<vk::BufferMemoryBarrier2KHR> buffer_barriers{};
		if (!barrier.buffer_barriers.empty()) {
			buffer_barriers.resize(barrier.buffer_barriers.size());
		}

		for (int32_t i = 0; i < static_cast<int32_t>(barrier.buffer_barriers.size()); ++i) {
			auto const& src_barrier = barrier.buffer_barriers[i];
			auto& dst_barrier = buffer_barriers[i];
			dst_barrier.buffer = src_barrier.buffer->get_handle();
			dst_barrier.offset = 0ull;
			dst_barrier.size = src_barrier.buffer->get_size();

			auto src_state = util::get_resource_state(src_barrier.src_state);
			dst_barrier.srcAccessMask = src_state.access_flags;
			dst_barrier.srcStageMask = src_state.stage_flags;

			auto new_state = util::get_resource_state(src_barrier.dst_state);
			dst_barrier.dstAccessMask = new_state.access_flags;
			dst_barrier.dstStageMask = new_state.stage_flags;
		}

		mi::Vector<vk::ImageMemoryBarrier2KHR> image_barriers{};
		if (!barrier.image_barriers.empty()) {
			image_barriers.resize(barrier.image_barriers.size());
		}

		for (int32_t i = 0; i < static_cast<int32_t>(barrier.image_barriers.size()); ++i) {
			auto const& src_barrier = barrier.image_barriers[i];
			auto& dst_barrier = image_barriers[i];
			dst_barrier.image = src_barrier.image->get_handle();
			dst_barrier.subresourceRange = src_barrier.subresource_range;

			auto src_state = util::get_resource_state(src_barrier.src_state);
			dst_barrier.srcAccessMask = src_state.access_flags;
			dst_barrier.srcStageMask = src_state.stage_flags;
			dst_barrier.oldLayout = util::get_image_layout(src_barrier.src_state);

			auto new_state = util::get_resource_state(src_barrier.dst_state);
			dst_barrier.dstAccessMask = new_state.access_flags;
			dst_barrier.dstStageMask = new_state.stage_flags;
			dst_barrier.newLayout = util::get_image_layout(src_barrier.dst_state);
		}

		vk::DependencyInfoKHR dependency_info{};
		dependency_info.pMemoryBarriers = memory_barriers.data();
		dependency_info.memoryBarrierCount = static_cast<uint32_t>(memory_barriers.size());
		dependency_info.pBufferMemoryBarriers = buffer_barriers.data();
		dependency_info.bufferMemoryBarrierCount = static_cast<uint32_t>(buffer_barriers.size());
		dependency_info.pImageMemoryBarriers = image_barriers.data();
		dependency_info.imageMemoryBarrierCount = static_cast<uint32_t>(barrier.image_barriers.size());

		handle_.pipelineBarrier2KHR(&dependency_info);
	}

	auto CommandBuffer::push_barrier(const ImageBarrier& image_barrier) const -> void {
		Barrier barrier{};
		barrier.image_barriers = { &image_barrier, 1ull };
		push_barrier(barrier);
	}

	auto CommandBuffer::push_barrier(const BufferBarrier& buffer_barrier) const -> void {
		Barrier barrier{};
		barrier.buffer_barriers = { &buffer_barrier, 1ull };
		push_barrier(barrier);
	}

	inline auto make_color(uint32_t color) -> std::array<float, 4> {
		std::array<float, 4> out_color{};
		out_color[0] = ((color >> 24) & 0xFF) / 255.0f;
		out_color[1] = ((color >> 16) & 0xFF) / 255.0f;
		out_color[2] = ((color >> 8) & 0xFF) / 255.0f;
		out_color[3] = (color & 0xFF) / 255.0f;

		return out_color;
	}

	auto CommandBuffer::begin_marker(std::string_view name, uint32_t color) const -> void {
#if defined(EDGE_DEBUG)
		vk::DebugUtilsLabelEXT label{};
		label.pLabelName = name.data();
		label.color = make_color(color);
		handle_.beginDebugUtilsLabelEXT(&label);
#else
		(void)name;
		(void)color;
#endif
	}

	auto CommandBuffer::end_marker() const -> void {
#if defined(EDGE_DEBUG)
		handle_.endDebugUtilsLabelEXT();
#endif
	}
	

#undef EDGE_LOGGER_SCOPE // CommandBuffer
#define EDGE_LOGGER_SCOPE "gfx::CommandPool"

	auto CommandPool::allocate_command_buffer(vk::CommandBufferLevel level) const -> CommandBuffer {
		vk::CommandBufferAllocateInfo allocate_info{};
		allocate_info.commandPool = handle_;
		allocate_info.level = level;
		allocate_info.commandBufferCount = 1u;

		vk::CommandBuffer command_buffer;
		auto result = device_->allocateCommandBuffers(&allocate_info, &command_buffer);
		EDGE_FATAL_VK_ERROR(result, "allocateCommandBuffers");

		return CommandBuffer{ handle_, command_buffer };
	}

#undef EDGE_LOGGER_SCOPE
#define EDGE_LOGGER_SCOPE "gfx::Sampler"

	auto Sampler::create(const vk::SamplerCreateInfo& create_info) -> Sampler {
		vk::Sampler sampler;
		auto result = device_->createSampler(&create_info, allocator_, &sampler);
		EDGE_FATAL_VK_ERROR(result, "createSampler");

		return Sampler{ sampler, create_info };
	}

#undef EDGE_LOGGER_SCOPE
#define EDGE_LOGGER_SCOPE "gfx::QueryPool"

	auto QueryPool::create(vk::QueryType type, uint32_t query_count) -> QueryPool {
		vk::QueryPoolCreateInfo create_info{};
		create_info.queryType = type;
		create_info.queryCount = query_count;

		if (type == vk::QueryType::eTimestamp) {
			create_info.queryCount *= 2;
		}

		vk::QueryPool query_pool;
		auto result = device_->createQueryPool(&create_info, allocator_, &query_pool);
		EDGE_FATAL_VK_ERROR(result, "createQueryPool");
		return QueryPool(query_pool, type, create_info.queryCount);
	}

	auto QueryPool::get_data(uint32_t query_index, void* data) const -> vk::Result {
		auto result = vk::Result::eSuccess;
		switch (type_)
		{
		case vk::QueryType::eOcclusion: {
			result = device_->getQueryPoolResults(handle_, query_index, 1u, sizeof(uint64_t), data, sizeof(uint64_t), vk::QueryResultFlagBits::e64);
			break;
		}
		case vk::QueryType::ePipelineStatistics: {
			assert(false && "NOT IMPLEMENTED");
			break;
		}
		case vk::QueryType::eTimestamp: {
			result = device_->getQueryPoolResults(handle_, query_index * 2u, 2u, sizeof(uint64_t) * 2u, data, sizeof(uint64_t), vk::QueryResultFlagBits::e64);
			break;
		}
		default:
			assert(false && "NOT IMPLEMENTED");
			break;
		}

		return result;
	}

	auto QueryPool::get_data(uint32_t first_query, uint32_t query_count, void* data) const -> vk::Result {
		assert(handle_ && "QueryPool handle is null.");
		assert(data && "Output data buffer is null.");

		auto result = vk::Result::eSuccess;
		switch (type_) {
		case vk::QueryType::eOcclusion:
		case vk::QueryType::eTimestamp: {
			result = device_->getQueryPoolResults(handle_, first_query, query_count, static_cast<uint32_t>(sizeof(uint64_t) * query_count), data, sizeof(uint64_t), vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWait);
			break;
		}
		case vk::QueryType::ePipelineStatistics: {
			assert(false && "NOT IMPLEMENTED");
			break;
		}
		default:
			assert(false && "NOT IMPLEMENTED");
			break;
		}
		return result;
	}

	auto QueryPool::reset(uint32_t start_query, uint32_t query_count) const -> void {
		assert(handle_ && "QueryPool handle is null.");
		device_->resetQueryPoolEXT(handle_, start_query, query_count ? query_count : max_query_);
	}

#undef EDGE_LOGGER_SCOPE
#define EDGE_LOGGER_SCOPE "gfx::PipelineCache"

	auto PipelineCache::create(Span<const uint8_t> data) -> PipelineCache {
		vk::PipelineCacheCreateInfo create_info{};

		vk::PipelineCache pipeline_cache;
		auto result = device_->createPipelineCache(&create_info, allocator_, &pipeline_cache);
		EDGE_FATAL_VK_ERROR(result, "createPipelineCache");
		return PipelineCache{ pipeline_cache };
	}

	auto PipelineCache::get_data(mi::Vector<uint8_t>& data) const -> vk::Result {
		size_t cache_size{ 0ull };
		if (auto result = device_->getPipelineCacheData(handle_, &cache_size, nullptr); result != vk::Result::eSuccess) {
			return result;
		}

		data.resize(cache_size, '\0');
		return device_->getPipelineCacheData(handle_, &cache_size, data.data());
	}

	auto PipelineCache::get_data(void*& data, size_t& size) const -> vk::Result {
		return device_->getPipelineCacheData(handle_, &size, data);
	}

#undef EDGE_LOGGER_SCOPE
#define EDGE_LOGGER_SCOPE "gfx::ShaderModule"

	auto ShaderModule::create(Span<const uint8_t> code) -> ShaderModule {
		vk::ShaderModuleCreateInfo create_info{};
		create_info.codeSize = static_cast<uint32_t>(code.size());
		create_info.pCode = reinterpret_cast<const uint32_t*>(code.data());

		vk::ShaderModule shader_module;
		auto result = device_->createShaderModule(&create_info, allocator_, &shader_module);
		EDGE_FATAL_VK_ERROR(result, "createShaderModule");

		return ShaderModule{ shader_module };
	}

#undef EDGE_LOGGER_SCOPE
#define EDGE_LOGGER_SCOPE "gfx::DescriptorSetLayoutBuilder"

	auto DescriptorSetLayoutBuilder::build(vk::DescriptorSetLayoutCreateFlags flags) -> DescriptorSetLayout {
		vk::DescriptorSetLayoutBindingFlagsCreateInfoEXT ext_create_info{};
		ext_create_info.bindingCount = static_cast<uint32_t>(binding_flags_.size());
		ext_create_info.pBindingFlags = binding_flags_.data();

		vk::DescriptorSetLayoutCreateInfo create_info{};
		create_info.bindingCount = static_cast<uint32_t>(layout_bindings_.size());
		create_info.pBindings = layout_bindings_.data();
		create_info.flags = flags;
		create_info.pNext = &ext_create_info;

		vk::DescriptorSetLayout descriptor_set_layout;
		auto result = device_->createDescriptorSetLayout(&create_info, allocator_, &descriptor_set_layout);
		EDGE_FATAL_VK_ERROR(result, "createDescriptorSetLayout");

		return DescriptorSetLayout{ descriptor_set_layout, pool_sizes_ };
	}

#undef EDGE_LOGGER_SCOPE
#define EDGE_LOGGER_SCOPE "gfx::PipelineLayoutBuilder"

	auto PipelineLayoutBuilder::add_set_layout(DescriptorSetLayout const& set_layout) -> PipelineLayoutBuilder& {
		descriptor_set_layouts_.push_back(set_layout.get_handle());
		return *this;
	}

	auto PipelineLayoutBuilder::build() -> PipelineLayout {
		vk::PipelineLayoutCreateInfo create_info{};
		create_info.setLayoutCount = static_cast<uint32_t>(descriptor_set_layouts_.size());
		create_info.pSetLayouts = descriptor_set_layouts_.data();
		create_info.pushConstantRangeCount = static_cast<uint32_t>(push_constant_ranges_.size());
		create_info.pPushConstantRanges = push_constant_ranges_.data();

		vk::PipelineLayout pipeline_layout;
		auto result = device_->createPipelineLayout(&create_info, allocator_, &pipeline_layout);
		EDGE_FATAL_VK_ERROR(result, "createPipelineLayout");

		return PipelineLayout{ pipeline_layout };
	}

#undef EDGE_LOGGER_SCOPE
#define EDGE_LOGGER_SCOPE "gfx::DescriptorPool"

	auto DescriptorPool::create(PoolSizes const& requested_sizes, uint32_t max_descriptor_sets, vk::DescriptorPoolCreateFlags flags) -> DescriptorPool {
		Array<vk::DescriptorPoolSize, PoolSizes::k_max_sizes> pool_sizes{};
		for (int32_t i = 0; i < pool_sizes.size(); ++i) {
			auto& size = pool_sizes[i];
			size.type = static_cast<vk::DescriptorType>(i);
			size.descriptorCount = std::max(1u, static_cast<uint32_t>(requested_sizes[i]));
		}

		vk::DescriptorPoolCreateInfo create_info{};
		create_info.poolSizeCount = static_cast<uint32_t>(PoolSizes::k_max_sizes);
		create_info.pPoolSizes = pool_sizes.data();
		create_info.maxSets = max_descriptor_sets;
		create_info.flags = flags;

		vk::DescriptorPool descriptor_pool;
		auto result = device_->createDescriptorPool(&create_info, allocator_, &descriptor_pool);
		EDGE_FATAL_VK_ERROR(result, "createDescriptorPool");

		return DescriptorPool{ descriptor_pool };
	}

	auto DescriptorPool::allocate_descriptor_set(DescriptorSetLayout const& layout) const -> DescriptorSet {
		vk::DescriptorSetAllocateInfo alloc_info{};
		alloc_info.descriptorPool = handle_;
		alloc_info.descriptorSetCount = 1u;
		auto set_layout_handle = layout.get_handle();
		alloc_info.pSetLayouts = &set_layout_handle;

		vk::DescriptorSet descriptor_set;
		auto result = device_->allocateDescriptorSets(&alloc_info, &descriptor_set);
		EDGE_FATAL_VK_ERROR(result, "allocateDescriptorSets");

		return DescriptorSet{ descriptor_set, layout.get_pool_sizes() };
	}

	auto DescriptorPool::free_descriptor_set(DescriptorSet const& set) const -> void {
		auto const& descriptor_set = set.get_handle();
		if (auto result = device_->freeDescriptorSets(handle_, 1u, &descriptor_set); result != vk::Result::eSuccess) {
			GFX_ASSERT_MSG(false, "Failed to free descriptor set.");
		}
	}

#undef EDGE_LOGGER_SCOPE // DescriptorPool

#define EDGE_LOGGER_SCOPE "gfx::Context"

	auto initialize_graphics(const ContextInfo& info) -> void {
		allocator_ = VulkanLifetime::get_instance().get_allocator();

		instance_ = InstanceBuilder{}
			.set_app_name(info.application_name)
			.set_app_version(1, 0, 0)
			.set_engine_name(info.engine_name)
			.set_engine_version(1, 0, 0)
			.set_api_version(info.minimal_api_version)

			.enable_surface()

#if defined(USE_VALIDATION_LAYERS)
			.add_layer("VK_LAYER_KHRONOS_validation")
#if defined(EDGE_VK_USE_SYNCHRONIZATION_VALIDATION)
			.add_layer("VK_LAYER_KHRONOS_synchronization2")
#endif // VULKAN_VALIDATION_LAYERS_SYNCHRONIZATION
#endif // USE_VALIDATION_LAYERS

			// Enable validation features
#if defined(USE_VALIDATION_LAYER_FEATURES)
			//.add_validation_feature_enable(vk::ValidationFeatureEnableEXT::eDebugPrintf)
#if defined(EDGE_VK_USE_GPU_ASSISTED_VALIDATION)
			.add_validation_feature_enable(vk::ValidationFeatureEnableEXT::eGpuAssistedReserveBindingSlot)
			.add_validation_feature_enable(vk::ValidationFeatureEnableEXT::eGpuAssisted)
#endif
#if defined(EDGE_VK_USE_BEST_PRACTICES_VALIDATION)
			.add_validation_feature_enable(vk::ValidationFeatureEnableEXT::eBestPractices)
#endif
#if defined(EDGE_VK_USE_SYNCHRONIZATION_VALIDATION)
			.add_validation_feature_enable(vk::ValidationFeatureEnableEXT::eSynchronizationValidation)
#endif
#endif

			.add_extension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME)

#if defined(EDGE_DEBUG)
			.enable_debug_utils()
#endif

#if defined(VKW_ENABLE_PORTABILITY)
			.enable_portability()
#endif
			.build();

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
		auto surface_create_info = static_cast<vk::AndroidSurfaceCreateInfoKHR*>(info.window->get_native_handle());
#elif defined(VK_USE_PLATFORM_WIN32_KHR)
		auto surface_create_info = *static_cast<vk::Win32SurfaceCreateInfoKHR*>(info.window->get_native_handle());
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
		auto surface_create_info = *static_cast<vk::XlibSurfaceCreateInfoKHR*>(info.window->get_native_handle());
#endif

		surface_ = Surface::create(surface_create_info);

		// Select device
		std::tie(adapter_, device_) = DeviceSelector{}
			.set_surface(surface_.get_handle())
			.set_api_version(1, 2, 0)
			.set_preferred_device_type(vk::PhysicalDeviceType::eDiscreteGpu)
			.add_extension(VK_KHR_SWAPCHAIN_EXTENSION_NAME)
			.add_extension(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME)
			.add_extension(VK_KHR_MAINTENANCE_4_EXTENSION_NAME)
			.add_extension(VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME)
			.add_extension(VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME)
			.add_extension(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME)
			.add_extension(VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME)
			.add_extension(VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME)
			.add_extension(VK_KHR_8BIT_STORAGE_EXTENSION_NAME)
			.add_extension(VK_KHR_16BIT_STORAGE_EXTENSION_NAME)
			.add_extension(VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME)
			.add_extension(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME)
			.add_extension(VK_KHR_SPIRV_1_4_EXTENSION_NAME)
			.add_extension(VK_KHR_SEPARATE_DEPTH_STENCIL_LAYOUTS_EXTENSION_NAME)
			.add_extension(VK_KHR_COPY_COMMANDS_2_EXTENSION_NAME)
			.add_extension(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME)
			.add_extension(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME)
			.add_extension(VK_KHR_VULKAN_MEMORY_MODEL_EXTENSION_NAME)
			.add_extension(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME, false)
			.add_extension(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME, false)
			.add_extension(VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME, false)
			.add_extension(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME)
			.add_extension(VK_KHR_PERFORMANCE_QUERY_EXTENSION_NAME, false)
			.add_extension(VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME, false)
			.add_extension(VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME, false)
			.add_extension(VK_KHR_BIND_MEMORY_2_EXTENSION_NAME, false)
			.add_extension(VK_AMD_DEVICE_COHERENT_MEMORY_EXTENSION_NAME, false)
#if !defined(VK_USE_PLATFORM_ANDROID_KHR)
			.add_extension(VK_EXT_DEBUG_MARKER_EXTENSION_NAME, false)
			.add_extension(VK_EXT_SHADER_VIEWPORT_INDEX_LAYER_EXTENSION_NAME)
#endif
#if defined(VKW_ENABLE_PORTABILITY)
			.add_extension(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME)
#endif
			// Enable extensions for use nsight aftermath
#if USE_NSIGHT_AFTERMATH
			.add_extension(VK_NV_DEVICE_DIAGNOSTIC_CHECKPOINTS_EXTENSION_NAME)
#endif
			.add_feature<vk::PhysicalDeviceSynchronization2FeaturesKHR>()
			.add_feature<vk::PhysicalDeviceDynamicRenderingFeaturesKHR>()
			//.add_feature<vk::PhysicalDeviceShaderDemoteToHelperInvocationFeaturesEXT>()
			//.add_feature<vk::PhysicalDevice16BitStorageFeaturesKHR>()
			.add_feature<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>()
#if USE_NSIGHT_AFTERMATH
			.add_feature<vk::PhysicalDeviceDiagnosticsConfigFeaturesNV>(false)
#endif
			//.add_feature<vk::PhysicalDeviceAccelerationStructureFeaturesKHR>(create_info.require_features.ray_tracing)
			//.add_feature<vk::PhysicalDeviceRayTracingPipelineFeaturesKHR>(create_info.require_features.ray_tracing)
			//.add_feature<vk::PhysicalDeviceRayQueryFeaturesKHR>(create_info.require_features.ray_tracing)
			.select();

		// Create vulkan memory allocator
		VmaVulkanFunctions vma_vulkan_func{};
		vma_vulkan_func.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
		vma_vulkan_func.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

		VmaAllocatorCreateInfo vma_allocator_create_info{};
		vma_allocator_create_info.pVulkanFunctions = &vma_vulkan_func;
		vma_allocator_create_info.physicalDevice = adapter_;
		vma_allocator_create_info.device = device_;
		vma_allocator_create_info.instance = instance_;
		vma_allocator_create_info.pAllocationCallbacks = (VkAllocationCallbacks const*)(allocator_);

		// NOTE: Nsight graphics using VkImportMemoryHostPointerEXT that cannot be used with dedicated memory allocation
		bool is_nsignt_graphics_attached{ false };
#ifdef VK_USE_PLATFORM_WIN32_KHR
		{
			HMODULE nsight_graphics_module = GetModuleHandle("Nvda.Graphics.Interception.dll");
			is_nsignt_graphics_attached = (nsight_graphics_module != nullptr);
			if (is_nsignt_graphics_attached) {
				EDGE_SLOGI("Nsight graphics detected.");
				CloseHandle(nsight_graphics_module);
			}
		}
#endif
		bool can_get_memory_requirements = device_.is_enabled(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
		bool has_dedicated_allocation = device_.is_enabled(VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME);

		if (can_get_memory_requirements && has_dedicated_allocation && !is_nsignt_graphics_attached) {
			vma_allocator_create_info.flags |= VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;
		}

		if (device_.is_enabled(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME)) {
			vma_allocator_create_info.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
		}

		if (device_.is_enabled(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME)) {
			vma_allocator_create_info.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
		}

		if (device_.is_enabled(VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME)) {
			vma_allocator_create_info.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT;
		}

		if (device_.is_enabled(VK_KHR_BIND_MEMORY_2_EXTENSION_NAME)) {
			vma_allocator_create_info.flags |= VMA_ALLOCATOR_CREATE_KHR_BIND_MEMORY2_BIT;
		}

		if (device_.is_enabled(VK_AMD_DEVICE_COHERENT_MEMORY_EXTENSION_NAME)) {
			vma_allocator_create_info.flags |= VMA_ALLOCATOR_CREATE_AMD_DEVICE_COHERENT_MEMORY_BIT;
		}

		VmaAllocator vma_allocator;
		auto vma_result = vmaCreateAllocator(&vma_allocator_create_info, &vma_allocator);
		EDGE_FATAL_VK_ERROR(static_cast<vk::Result>(vma_result), "vmaCreateAllocator");
		memory_allocator_ = MemoryAllocator(vma_allocator);
	}

	auto shutdown_graphics() -> void {
		memory_allocator_ = {};
		device_ = {};
		adapter_ = {};
		surface_ = {};
		instance_ = {};
	}

#undef EDGE_LOGGER_SCOPE // Context
}