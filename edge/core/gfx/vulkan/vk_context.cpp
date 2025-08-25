#include "vk_context.h"

#include "../../platform/platform.h"

#include <cstdlib>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include "vk_util.h"

#if defined(ENGINE_DEBUG) || defined(VULKAN_VALIDATION_LAYERS)
#define USE_VALIDATION_LAYERS
#endif

#if defined(USE_VALIDATION_LAYERS) && (defined(VULKAN_VALIDATION_LAYERS_GPU_ASSISTED) || defined(VULKAN_VALIDATION_LAYERS_BEST_PRACTICES) || defined(VULKAN_VALIDATION_LAYERS_SYNCHRONIZATION))
#define USE_VALIDATION_LAYER_FEATURES
#endif

namespace edge::gfx {
	namespace
	{
#ifdef USE_VALIDATION_LAYERS
		VKAPI_ATTR VkBool32 VKAPI_CALL debug_utils_messenger_callback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity, VkDebugUtilsMessageTypeFlagsEXT message_type, const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data) {
			// Log debug message
			if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
				spdlog::info("{} - {}: {}", callback_data->messageIdNumber, callback_data->pMessageIdName, callback_data->pMessage);
			}
			else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
				spdlog::info("{} - {}: {}", callback_data->messageIdNumber, callback_data->pMessageIdName, callback_data->pMessage);
			}
			else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
				spdlog::warn("{} - {}: {}", callback_data->messageIdNumber, callback_data->pMessageIdName, callback_data->pMessage);
			}
			else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
				spdlog::error("{} - {}: {}", callback_data->messageIdNumber, callback_data->pMessageIdName, callback_data->pMessage);
			}
			return VK_FALSE; 
		}

		static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT /*type*/,
			uint64_t /*object*/, size_t /*location*/, int32_t /*message_code*/,
			const char* layer_prefix, const char* message, void* /*user_data*/) {
			if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
				spdlog::error("{}: {}", layer_prefix, message);
			}
			else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) {
				spdlog::warn("{}: {}", layer_prefix, message);
			}
			else if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT) {
				spdlog::warn("{}: {}", layer_prefix, message);
			}
			else if (flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT) {
				spdlog::debug("{}: {}", layer_prefix, message);
			}
			else {
				spdlog::info("{}: {}", layer_prefix, message);
			}
			return VK_FALSE;
		}
#endif
	}

	auto validate_layers(const std::vector<const char*>& required, const std::vector<VkLayerProperties>& available) -> bool {
		for (auto layer : required) {
			bool found = false;
			for (auto& available_layer : available) {
				if (strcmp(available_layer.layerName, layer) == 0) {
					found = true;
					break;
				}
			}

			if (!found) {
				spdlog::error("[Vulkan Graphics Context]: Validation Layer {} not found", layer);
				return false;
			}
		}

		return true;
	}

	auto get_optimal_validation_layers(const std::vector<VkLayerProperties>& supported_instance_layers) -> std::vector<const char*> {
		std::vector<std::vector<const char*>> validation_layer_priority_list = {
			// The preferred validation layer is "VK_LAYER_KHRONOS_validation"
			{"VK_LAYER_KHRONOS_validation"},

			// Otherwise we fallback to using the LunarG meta layer
			{"VK_LAYER_LUNARG_standard_validation"},

			// Otherwise we attempt to enable the individual layers that compose the LunarG meta layer since it doesn't exist
			{
				"VK_LAYER_GOOGLE_threading",
				"VK_LAYER_LUNARG_parameter_validation",
				"VK_LAYER_LUNARG_object_tracker",
				"VK_LAYER_LUNARG_core_validation",
				"VK_LAYER_GOOGLE_unique_objects",
			},

			// Otherwise as a last resort we fallback to attempting to enable the LunarG core layer
			{"VK_LAYER_LUNARG_core_validation"}
		};

		for (auto& validation_layers : validation_layer_priority_list) {
			if (validate_layers(validation_layers, supported_instance_layers)) {
				return validation_layers;
			}

			spdlog::warn("[Vulkan Graphics Context]: Couldn't enable validation layers (see log for error) - falling back");
		}

		// Else return nothing
		return {};
	}

    extern "C" void* VKAPI_CALL vkmemalloc(void* user_data, size_t size, size_t alignment, VkSystemAllocationScope allocation_scope) {
		auto* stats = static_cast<VkMemoryAllocationStats*>(user_data);

		void* ptr = nullptr;

#ifdef EDGE_PLATFORM_WINDOWS
		ptr = _aligned_malloc(size, alignment);
#else
        // Ensure alignment meets POSIX requirements
        if (alignment < sizeof(void*)) {
            alignment = sizeof(void*);
        }

        // Ensure alignment is power of 2
        if (alignment & (alignment - 1)) {
            alignment = 1;
            while (alignment < size) alignment <<= 1;
        }

		// POSIX aligned allocation
		if (posix_memalign(&ptr, alignment, size) != 0) {
            spdlog::error("[Vulkan Graphics Context]: Failed to allocate {} bytes with {} bytes alignment in {} scope.",
                          size, alignment, get_allocation_scope_str(allocation_scope));
			ptr = nullptr;
		}
#endif
		if (stats && ptr) {
			stats->total_bytes_allocated.fetch_add(size);
			stats->allocation_count.fetch_add(1ull);

            std::lock_guard<std::mutex> lock(stats->mutex);
			stats->allocation_map[ptr] = { size, alignment, allocation_scope, std::this_thread::get_id() };

#if VULKAN_DEBUG && !EDGE_PLATFORM_WINDOWS
			spdlog::trace("[Vulkan Graphics Context]: Allocation({:#010x}, {} bytes, {} byte alignment, scope - {}, in thread - {})",
				reinterpret_cast<uintptr_t>(ptr), size, alignment, get_allocation_scope_str(allocation_scope), std::this_thread::get_id());
#endif
		}

		return ptr;
	}

    extern "C" void VKAPI_CALL vkmemfree(void* user_data, void* mem) {
		if (!mem) {
			return;
		}

		auto* stats = static_cast<VkMemoryAllocationStats*>(user_data);
		if (stats) {
            std::lock_guard<std::mutex> lock(stats->mutex);
			if (auto found = stats->allocation_map.find(mem); found != stats->allocation_map.end()) {
				stats->total_bytes_allocated.fetch_sub(found->second.size);
				stats->deallocation_count.fetch_add(1ull);

#if VULKAN_DEBUG && !EDGE_PLATFORM_WINDOWS
				spdlog::trace("[Vulkan Graphics Context]: Deallocation({:#010x}, {} bytes, {} byte alignment, scope - {}, in thread - {})",
					reinterpret_cast<uintptr_t>(mem), found->second.size, found->second.align, get_allocation_scope_str(found->second.scope), std::this_thread::get_id());
#endif
				stats->allocation_map.erase(found);
			}
			else {
				spdlog::error("[Vulkan Graphics Context]: Found invalid memory allocation: {:#010x}.", reinterpret_cast<uintptr_t>(mem));
			}
		}

#ifdef EDGE_PLATFORM_WINDOWS
		_aligned_free(mem);
#else
		free(mem);
#endif
	}

    extern "C" void* VKAPI_CALL vkmemrealloc(void* user_data, void* old, size_t size, size_t alignment, VkSystemAllocationScope allocation_scope) {
		if (!old) {
			return vkmemalloc(user_data, size, alignment, allocation_scope);
		}

		if (size == 0ull) {
			// Behave like free
			vkmemfree(user_data, old);
			return nullptr;
		}

#if EDGE_PLATFORM_WINDOWS
        auto* stats = static_cast<VkMemoryAllocationStats*>(user_data);
        auto* new_ptr = _aligned_realloc(old, size, alignment);
        if (stats && new_ptr) {
            std::lock_guard<std::mutex> lock(stats->mutex);
            if (auto found = stats->allocation_map.find(old); found != stats->allocation_map.end()) {
                stats->total_bytes_allocated.fetch_sub(found->second.size);
                stats->deallocation_count.fetch_add(1ull);
                stats->allocation_map.erase(found);
            }

            stats->total_bytes_allocated.fetch_add(size);
            stats->allocation_count.fetch_add(1ull);
            stats->allocation_map[new_ptr] = { size, alignment, allocation_scope, std::this_thread::get_id() };

#if VULKAN_DEBUG && !EDGE_PLATFORM_WINDOWS
            spdlog::trace("[Vulkan Graphics Context]: Allocation({:#010x}, {} bytes, {} byte alignment, scope - {}, in thread - {})",
                reinterpret_cast<uintptr_t>(new_ptr), size, alignment, get_allocation_scope_str(allocation_scope), std::this_thread::get_id());
#endif
        }
#else
		void* new_ptr = vkmemalloc(user_data, size, alignment, allocation_scope);
		if (new_ptr && old) {
			memcpy(new_ptr, old, size);
			vkmemfree(user_data, old);
		}
#endif
		return new_ptr;
	}

    extern "C" void VKAPI_CALL vkinternalmemalloc(void* user_data, size_t size, VkInternalAllocationType allocation_type, VkSystemAllocationScope allocation_scope) {

    }

    extern "C" void VKAPI_CALL vkinternalmemfree(void* user_data, size_t size, VkInternalAllocationType allocation_type, VkSystemAllocationScope allocation_scope) {

    }

    VulkanGraphicsContext::~VulkanGraphicsContext() {
        if(vma_allocator_) {
            spdlog::debug("[Vulkan Graphics Context]: Destroying VMA allocator");
            vmaDestroyAllocator(vma_allocator_);
        }

        if(selected_device_index_ != -1) {
            auto& device = devices_[selected_device_index_];
            spdlog::debug("[Vulkan Graphics Context]: Destroying logical device: {}", device.properties.deviceName);
            vkDestroyDevice(device.logical, &vk_alloc_callbacks_);
        }

        if(vk_surface_) {
            spdlog::debug("[Vulkan Graphics Context]: Destroying surface");
            vkDestroySurfaceKHR(vk_instance_, vk_surface_, &vk_alloc_callbacks_);
        }

		if (vk_debug_utils_messenger_) {
            spdlog::debug("[Vulkan Graphics Context]: Destroying debug utils messenger");
			vkDestroyDebugUtilsMessengerEXT(vk_instance_, vk_debug_utils_messenger_, &vk_alloc_callbacks_);
		}

		if (vk_debug_report_callback_) {
            spdlog::debug("[Vulkan Graphics Context]: Destroying debug report callback");
			vkDestroyDebugReportCallbackEXT(vk_instance_, vk_debug_report_callback_, &vk_alloc_callbacks_);
		}

		if (vk_instance_) {
            spdlog::debug("[Vulkan Graphics Context]: Destroying Vulkan instance");
			vkDestroyInstance(vk_instance_, &vk_alloc_callbacks_);
		}

		if (volk_initialized_) {
            spdlog::debug("[Vulkan Graphics Context]: Finalizing Volk");
			volkFinalize();
		}

        // Check that all allocated vulkan objects was deallocated
        if (memalloc_stats_.allocation_count != memalloc_stats_.deallocation_count) {
            spdlog::error("[Vulkan Graphics Context]: Memory leaks detected!\n Allocated: {}, Deallocated: {} objects. Leaked {} bytes.",
                memalloc_stats_.allocation_count.load(), memalloc_stats_.deallocation_count.load(), memalloc_stats_.total_bytes_allocated.load());

            for (const auto& allocation : memalloc_stats_.allocation_map) {
                spdlog::warn("{:#010x} : {} bytes, {} byte alignment, {} scope",
                    reinterpret_cast<uintptr_t>(allocation.first), allocation.second.size, allocation.second.align, get_allocation_scope_str(allocation.second.scope));
            }
        }
        else {
            spdlog::info("[Vulkan Graphics Context]: All memory correctly deallocated");
        }
	}

	auto VulkanGraphicsContext::construct() -> std::unique_ptr<VulkanGraphicsContext> {
		auto self = std::make_unique<VulkanGraphicsContext>();
		return self;
	}

    // Helper function to add instance extensions with logging
    auto add_instance_extension(std::vector<const char*>& extensions, const char* extension_name, const std::vector<VkExtensionProperties>& available_extensions, bool required = true) -> bool {
        auto found = std::find_if(available_extensions.begin(), available_extensions.end(),
            [extension_name](const VkExtensionProperties& prop) {
                return strcmp(prop.extensionName, extension_name) == 0;
            });

        if (found != available_extensions.end()) {
            extensions.push_back(extension_name);
            spdlog::debug("[Vulkan Graphics Context]: Added instance extension: {}", extension_name);
            return true;
        }
        else if (required) {
            spdlog::error("[Vulkan Graphics Context]: Required instance extension not available: {}", extension_name);
            return false;
        }
        else {
            spdlog::warn("[Vulkan Graphics Context]: Optional instance extension not available: {}", extension_name);
            return false;
        }
    }

    // Helper function to add device extensions with logging
    auto add_device_extension(std::vector<const char*>& extensions, const char* extension_name, const VkDeviceHandle& device, bool required = true) -> bool {
        auto found = std::find_if(device.extensions.begin(), device.extensions.end(),
            [extension_name](const VkExtensionProperties& prop) {
                return strcmp(prop.extensionName, extension_name) == 0;
            });

        if (found != device.extensions.end()) {
            extensions.push_back(extension_name);
            spdlog::debug("[Vulkan Graphics Context]: Added device extension: {}", extension_name);
            return true;
        }
        else if (required) {
            spdlog::warn("[Vulkan Graphics Context]: Required device extension not available: {}", extension_name);
            return false;
        }
        else {
            spdlog::debug("[Vulkan Graphics Context]: Optional device extension not available: {}", extension_name);
            return false;
        }
    }

	auto VulkanGraphicsContext::create(const GraphicsContextCreateInfo& create_info) -> bool {
		memalloc_stats_.total_bytes_allocated = 0ull;
		memalloc_stats_.allocation_count = 0ull;
		memalloc_stats_.deallocation_count = 0ull;

		// Create allocation callbacks
		vk_alloc_callbacks_.pfnAllocation = (PFN_vkAllocationFunction)vkmemalloc;
		vk_alloc_callbacks_.pfnFree = (PFN_vkFreeFunction)vkmemfree;
		vk_alloc_callbacks_.pfnReallocation = (PFN_vkReallocationFunction)vkmemrealloc;
		vk_alloc_callbacks_.pfnInternalAllocation = (PFN_vkInternalAllocationNotification) vkinternalmemalloc;
		vk_alloc_callbacks_.pfnInternalFree = (PFN_vkInternalFreeNotification)vkinternalmemfree;
		vk_alloc_callbacks_.pUserData = &memalloc_stats_;

		VK_CHECK_RESULT(volkInitialize(), "Failed to initialize volk.");

		// Enumerate instance extensions
		uint32_t instance_extension_property_count{ 0u };
		VK_CHECK_RESULT(vkEnumerateInstanceExtensionProperties(nullptr, &instance_extension_property_count, nullptr),
			"Failed to request instance extension properties count.");

		std::vector<VkExtensionProperties> instance_extension_properties(instance_extension_property_count);
		VK_CHECK_RESULT(vkEnumerateInstanceExtensionProperties(nullptr, &instance_extension_property_count, instance_extension_properties.data()),
			"Failed to request instance extension properties.");

		// Collect all required instance extensions
        if (!add_instance_extension(instance_extensions_, VK_KHR_SURFACE_EXTENSION_NAME, instance_extension_properties)) {
            return false;
        }

		// Add platform dependent surface extensions
#if EDGE_PLATFORM_ANDROID
        if (!add_instance_extension(instance_extensions_, VK_KHR_ANDROID_SURFACE_EXTENSION_NAME, instance_extension_properties)) {
            return false;
        }
#elif EDGE_PLATFORM_WINDOWS
        if (!add_instance_extension(instance_extensions_, VK_KHR_WIN32_SURFACE_EXTENSION_NAME, instance_extension_properties)) {
            return false;
        }
#endif

#if defined(VULKAN_DEBUG) && defined(USE_VALIDATION_LAYERS)
        if (add_instance_extension(instance_extensions_, VK_EXT_DEBUG_UTILS_EXTENSION_NAME, instance_extension_properties, false)) {
            VK_EXT_debug_utils_enabled = true;
        }
        else if (add_instance_extension(instance_extensions_, VK_EXT_DEBUG_REPORT_EXTENSION_NAME, instance_extension_properties, false)) {
            VK_EXT_debug_report_enabled = true;
        }
#endif

#if defined(VULKAN_ENABLE_PORTABILITY)
        add_instance_extension(instance_extensions_, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME, instance_extension_properties, false);
        add_instance_extension(instance_extensions_, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME, instance_extension_properties, false);
#endif

        // VK_KHR_get_physical_device_properties2 is a prerequisite of VK_KHR_performance_query
        // which will be used for stats gathering where available.
        add_instance_extension(instance_extensions_, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME, instance_extension_properties, false);

#ifdef USE_VALIDATION_LAYER_FEATURES
        bool validation_features = false;
        uint32_t available_layer_instance_extension_count{ 0u };
        if (vkEnumerateInstanceExtensionProperties("VK_LAYER_KHRONOS_validation", &available_layer_instance_extension_count, nullptr) == VK_SUCCESS) {
            std::vector<VkExtensionProperties> available_layer_instance_extensions(available_layer_instance_extension_count);
            if (vkEnumerateInstanceExtensionProperties("VK_LAYER_KHRONOS_validation", &available_layer_instance_extension_count, available_layer_instance_extensions.data()) == VK_SUCCESS) {
                if (add_instance_extension(instance_extensions_, VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME, available_layer_instance_extensions, false)) {
                    validation_features = true;
                }
            }
        }
#endif

		uint32_t supported_validation_layer_count{ 0u };
		vkEnumerateInstanceLayerProperties(&supported_validation_layer_count, nullptr);

		std::vector<VkLayerProperties> supported_validation_layers(supported_validation_layer_count);
		vkEnumerateInstanceLayerProperties(&supported_validation_layer_count, supported_validation_layers.data());

		std::vector<const char*> requested_validation_layers{};

#ifdef USE_VALIDATION_LAYERS
		// Determine the optimal validation layers to enable that are necessary for useful debugging
		auto optimal_validation_layers = get_optimal_validation_layers(supported_validation_layers);
		requested_validation_layers.insert(requested_validation_layers.end(), optimal_validation_layers.begin(), optimal_validation_layers.end());

#if defined(VULKAN_VALIDATION_LAYERS_SYNCHRONIZATION)
		requested_validation_layers.emplace_back("VK_LAYER_KHRONOS_synchronization2");
#endif // VULKAN_VALIDATION_LAYERS_SYNCHRONIZATION
#endif // USE_VALIDATION_LAYERS

		if (validate_layers(requested_validation_layers, supported_validation_layers)) {
			spdlog::info("[Vulkan Graphics Context]: Enabled Validation Layers:");
			for (const auto& layer : requested_validation_layers) {
				spdlog::info("	\t{}", layer);
			}
		}
		else {
			spdlog::error("[Vulkan Graphics Context]: Required validation layers are missing.");
			return false;
		}

		VkApplicationInfo application_info{};
		application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		application_info.applicationVersion = 1;
		application_info.engineVersion = 1;
		application_info.apiVersion = VK_API_VERSION_1_2;
		application_info.pApplicationName = "EdgeVulkanApp";
		application_info.pEngineName = "EdgeGameEngine";

		VkInstanceCreateInfo instance_create_info{};
		instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		instance_create_info.pApplicationInfo = &application_info;
		instance_create_info.enabledExtensionCount = static_cast<uint32_t>(instance_extensions_.size());
		instance_create_info.ppEnabledExtensionNames = instance_extensions_.data();
		instance_create_info.enabledLayerCount = static_cast<uint32_t>(requested_validation_layers.size());
		instance_create_info.ppEnabledLayerNames = requested_validation_layers.data();

#ifdef USE_VALIDATION_LAYERS
		VkDebugUtilsMessengerCreateInfoEXT debug_utils_create_info{};
		VkDebugReportCallbackCreateInfoEXT debug_report_create_info{};

		if (VK_EXT_debug_utils_enabled) {
            spdlog::debug("[Vulkan Graphics Context]: Setting up debug utils messenger");
            debug_utils_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            debug_utils_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
            debug_utils_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            debug_utils_create_info.pfnUserCallback = debug_utils_messenger_callback;
            instance_create_info.pNext = &debug_utils_create_info;
		}
		else if (VK_EXT_debug_report_enabled)
		{
            spdlog::debug("[Vulkan Graphics Context]: Setting up debug report callback");
            debug_report_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
            debug_report_create_info.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
            debug_report_create_info.pfnCallback = debug_callback;
            instance_create_info.pNext = &debug_report_create_info;
		}
#endif

#if (defined(VULKAN_ENABLE_PORTABILITY))
		if (portability_enumeration_available) {
			instance_info.flags |= vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
		}
#endif

#ifdef USE_VALIDATION_LAYER_FEATURES
		VkValidationFeaturesEXT validation_features_info{};
		validation_features_info.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;

		std::vector<VkValidationFeatureEnableEXT> enable_features{};
		enable_features.emplace_back(VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT);

		if (validation_features) {
            spdlog::debug("[Vulkan Graphics Context]: Enabling validation features");
#if defined(VULKAN_VALIDATION_LAYERS_GPU_ASSISTED)
			enable_features.emplace_back(VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_RESERVE_BINDING_SLOT_EXT);
			enable_features.emplace_back(VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT);
#endif
#if defined(VULKAN_VALIDATION_LAYERS_BEST_PRACTICES)
			enable_features.emplace_back(VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT);
#endif
#if defined(VULKAN_VALIDATION_LAYERS_SYNCHRONIZATION)
			enable_features.emplace_back(VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT);
#endif
			validation_features_info.enabledValidationFeatureCount = static_cast<uint32_t>(enable_features.size());
			validation_features_info.pEnabledValidationFeatures = enable_features.data();
			validation_features_info.pNext = instance_create_info.pNext;
			instance_create_info.pNext = &validation_features_info;
		}
#endif

		// TODO: Not needed for now
		//VkLayerSettingsCreateInfoEXT layer_settings_create_info{};
		//layer_settings_create_info.sType = VK_STRUCTURE_TYPE_LAYER_SETTINGS_CREATE_INFO_EXT;
		//
		//// If layer settings extension enabled by sample, then activate layer settings during instance creation
		//if (is_enabled(VK_EXT_LAYER_SETTINGS_EXTENSION_NAME)) {
		//	layer_settings_create_info.settingCount = static_cast<uint32_t>(builder.layer_settings.size());
		//	layer_settings_create_info.pSettings = builder.layer_settings.data();
		//	layer_settings_create_info.pNext = instance_create_info.pNext;
		//	instance_create_info.pNext = &layer_settings_create_info;
		//}

		VK_CHECK_RESULT(vkCreateInstance(&instance_create_info, &vk_alloc_callbacks_, &vk_instance_),
			"Failed to create VkInstance.");

		// Need to load volk for all the not-yet Vulkan calls
		volkLoadInstance(vk_instance_);

#ifdef USE_VALIDATION_LAYERS
		if (VK_EXT_debug_utils_enabled) {
			VK_CHECK_RESULT(vkCreateDebugUtilsMessengerEXT(vk_instance_, &debug_utils_create_info, &vk_alloc_callbacks_, &vk_debug_utils_messenger_), 
				"Failed to create debug utils messager.");
            spdlog::debug("[Vulkan Graphics Context]: Debug utils messenger created");
		}
		else if (VK_EXT_debug_report_enabled) {
			VK_CHECK_RESULT(vkCreateDebugReportCallbackEXT(vk_instance_, &debug_report_create_info, &vk_alloc_callbacks_, &vk_debug_report_callback_), 
				"Failed to create debug report callback.");
            spdlog::debug("[Vulkan Graphics Context]: Debug report callback created");
		}
#endif

		// Get available physical devices
        spdlog::debug("[Vulkan Graphics Context]: Enumerating physical devices");
        uint32_t physical_device_count{ 0u };
        VK_CHECK_RESULT(vkEnumeratePhysicalDevices(vk_instance_, &physical_device_count, nullptr),
            "Failed to get physical device count.");

        if (physical_device_count == 0) {
            spdlog::error("[Vulkan Graphics Context]: No Vulkan-capable devices found");
            return false;
        }

		std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
		VK_CHECK_RESULT(vkEnumeratePhysicalDevices(vk_instance_, &physical_device_count, physical_devices.data()),
			"Failed to get physical devices.");

        spdlog::info("[Vulkan Graphics Context]: Found {} physical devices:", physical_device_count);

        devices_.resize(physical_device_count);
        for(int32_t i = 0; i < physical_device_count; ++i) {
            auto& device = devices_[i];
            device.physical = physical_devices[i];
            vkGetPhysicalDeviceProperties(device.physical, &device.properties);
            
            // Get queue family properties
            uint32_t queue_family_properties_count{ 0u };
            vkGetPhysicalDeviceQueueFamilyProperties(device.physical, &queue_family_properties_count, nullptr);
            device.queue_family_props.resize(queue_family_properties_count);
            vkGetPhysicalDeviceQueueFamilyProperties(device.physical, &queue_family_properties_count, device.queue_family_props.data());

            // Enumerate device extensions
            uint32_t device_extension_count{ 0u };
            VK_CHECK_RESULT(vkEnumerateDeviceExtensionProperties(device.physical, nullptr, &device_extension_count, nullptr),
                "Failed to request device extension count.");
            device.extensions.resize(device_extension_count);
            VK_CHECK_RESULT(vkEnumerateDeviceExtensionProperties(device.physical, nullptr, &device_extension_count, device.extensions.data()),
                "Failed to request device extensions.");

            spdlog::info("  [{}] {} (API: {}.{}.{}, Type: {}, Extensions: {})",
                i, device.properties.deviceName,
                VK_VERSION_MAJOR(device.properties.apiVersion),
                VK_VERSION_MINOR(device.properties.apiVersion),
                VK_VERSION_PATCH(device.properties.apiVersion),
                device.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? "Discrete" :
                device.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? "Integrated" :
                device.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU ? "Virtual" : "Other",
                device_extension_count);
        }

		// Create surface
#if EDGE_PLATFORM_ANDROID
		VkAndroidSurfaceCreateInfoKHR surface_create_info{};
		surface_create_info.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
		surface_create_info.window = static_cast<ANativeWindow*>(create_info.window->get_native_handle());
		VK_CHECK_RESULT(vkCreateAndroidSurfaceKHR(vk_instance_ , &surface_create_info, &vk_alloc_callbacks_, &vk_surface_),
			"Failed to create surface.");
#elif EDGE_PLATFORM_WINDOWS
		auto hWnd = static_cast<HWND>(create_info.window->get_native_handle());
		HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE);

		VkWin32SurfaceCreateInfoKHR surface_create_info{};
		surface_create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		surface_create_info.hwnd = hWnd;
		surface_create_info.hinstance = hInstance;

		VK_CHECK_RESULT(vkCreateWin32SurfaceKHR(vk_instance_, &surface_create_info, &vk_alloc_callbacks_, &vk_surface_), 
			"Failed to create surface.");
#endif

        spdlog::debug("[Vulkan Graphics Context]: Surface created successfully");

		// Apply required device extensions
		device_extensions_.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
		device_extensions_.push_back(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME);
		device_extensions_.push_back(VK_KHR_MAINTENANCE_4_EXTENSION_NAME);
		device_extensions_.push_back(VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME);
		device_extensions_.push_back(VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME);
		device_extensions_.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
		device_extensions_.push_back(VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME);
		device_extensions_.push_back(VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME);
		device_extensions_.push_back(VK_KHR_8BIT_STORAGE_EXTENSION_NAME);
		device_extensions_.push_back(VK_KHR_16BIT_STORAGE_EXTENSION_NAME);
		device_extensions_.push_back(VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME);
		device_extensions_.push_back(VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME);
		device_extensions_.push_back(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);
		device_extensions_.push_back(VK_KHR_SPIRV_1_4_EXTENSION_NAME);
		device_extensions_.push_back(VK_KHR_SEPARATE_DEPTH_STENCIL_LAYOUTS_EXTENSION_NAME);
		device_extensions_.push_back(VK_KHR_COPY_COMMANDS_2_EXTENSION_NAME);
		device_extensions_.push_back(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME);
		device_extensions_.push_back(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME);
		device_extensions_.push_back(VK_KHR_VULKAN_MEMORY_MODEL_EXTENSION_NAME);
		device_extensions_.push_back(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);

#ifndef EDGE_PLATFORM_ANDROID
        device_extensions_.push_back(VK_EXT_SHADER_VIEWPORT_INDEX_LAYER_EXTENSION_NAME);
#endif

#ifdef VULKAN_ENABLE_PORTABILITY
		// VK_KHR_portability_subset must be enabled if present in the implementation (e.g on macOS/iOS with beta extensions enabled)
		// Optional
		device_extensions_.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
#endif

		// Enable extensions for use nsight aftermath
#if USE_NSIGHT_AFTERMATH
        device_extensions_.push_back(VK_NV_DEVICE_DIAGNOSTIC_CHECKPOINTS_EXTENSION_NAME);
#endif

		if (create_info.require_features.ray_tracing) {
			device_extensions_.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
		}

		// Select suitable gpu
        int32_t fallback_device_index{ -1 };
        for(int32_t device_index = 0; device_index < physical_device_count; ++device_index) {
            auto& device = devices_[device_index];

            if(device.properties.apiVersion < VK_API_VERSION_1_2) {
                spdlog::warn("[Vulkan Graphics Context]: Device is not supported. Required API version: {}, but device supporting {}.", VK_API_VERSION_1_2, device.properties.apiVersion);
                fallback_device_index = device_index;
                continue;
            }

            // Check device extension support
            bool all_extension_supported{ true };
            for (const auto& extension_name : device_extensions_) {
                auto supported = std::find_if(device.extensions.begin(), device.extensions.end(),
                             [extension_name](auto& device_extension) {
                    return std::strcmp(device_extension.extensionName, extension_name) == 0;
                }) != device.extensions.end();

                if (!supported) {
                    spdlog::warn("[Vulkan Graphics Context]: Device {} is not supporting required extension: {}.", device.properties.deviceName, extension_name);
                    all_extension_supported = false;
                    break;
                }
            }

            // Looke like that current device is not supporting some extensions
            if(!all_extension_supported) {
                fallback_device_index = device_index;
                continue;
            }

            VkPhysicalDeviceType requested_device_type{ VK_PHYSICAL_DEVICE_TYPE_OTHER };
            switch (create_info.physical_device_type) {
                case GraphicsDeviceType::eDiscrete: requested_device_type = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU; break;
                case GraphicsDeviceType::eIntegrated: requested_device_type = VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU; break;
                case GraphicsDeviceType::eSoftware: requested_device_type = VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU; break;
            }

            // Check that any device queue family is support present
            if (vk_surface_) {
                VkBool32 support_present{ VK_FALSE };

                uint32_t queue_family_properties_count{ 0u };
                vkGetPhysicalDeviceQueueFamilyProperties(device.physical, &queue_family_properties_count, nullptr);

                device.queue_family_props.resize(queue_family_properties_count);
                vkGetPhysicalDeviceQueueFamilyProperties(device.physical, &queue_family_properties_count, device.queue_family_props.data());

                for(int32_t family_index = 0; family_index < queue_family_properties_count; ++family_index) {
                    VK_CHECK_RESULT(vkGetPhysicalDeviceSurfaceSupportKHR(device.physical, family_index, vk_surface_, &support_present),
                                    "Failed to get physical device surface support.");
                    if(support_present) {
                        break;
                    }
                }

                // This device is not support present. Check next one
                if(!support_present) {
                    continue;
                }
            }

            // Check that device is same that requested type
            if(requested_device_type != device.properties.deviceType) {
                spdlog::warn("[Vulkan Graphics Context]: Device {} is not required type extension.", device.properties.deviceName);
                fallback_device_index = device_index;
                continue;
            }

            // Found best device
            selected_device_index_ = device_index;
            break;
        }

        // No device found.
        if(selected_device_index_ < 0) {
            if (fallback_device_index == -1) {
                spdlog::error("[Vulkan Graphics Context]: No suitable physical device found");
                return false;
            }
            selected_device_index_ = fallback_device_index;
            spdlog::warn("[Vulkan Graphics Context]: Using fallback device [{}]: {}", fallback_device_index, devices_[fallback_device_index].properties.deviceName);
        }

        auto& device = devices_[selected_device_index_];

#ifdef VULKAN_DEBUG
        if (!VK_EXT_debug_utils_enabled) {
            add_device_extension(device_extensions_, VK_EXT_DEBUG_MARKER_EXTENSION_NAME, device, false);
            if (is_device_extension_supported(device, VK_EXT_DEBUG_MARKER_EXTENSION_NAME)) {
                VK_EXT_debug_marker_enabled = true;
            }
        }
#endif

        // Create device
        VkDeviceCreateInfo device_create_info{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };

        std::vector<VkDeviceQueueCreateInfo> queue_create_infos(device.queue_family_props.size());
        std::vector<std::vector<float>> queue_priorities(device.queue_family_props.size());

        // Prepare queues
        for (uint32_t queue_family_index = 0U; queue_family_index < queue_create_infos.size(); ++queue_family_index) {
            auto const& queue_family_property = device.queue_family_props[queue_family_index];
            queue_priorities[queue_family_index].resize(queue_family_property.queueCount, 0.5f);

            auto& queue_create_info = queue_create_infos[queue_family_index];
            queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_create_info.queueFamilyIndex = queue_family_index;
            queue_create_info.queueCount = queue_family_property.queueCount;
            queue_create_info.pQueuePriorities = queue_priorities[queue_family_index].data();
        }

        add_device_extension(device_extensions_, VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME, device, false);
        add_device_extension(device_extensions_, VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME, device, false);

        VkPhysicalDeviceBufferDeviceAddressFeatures physical_device_buffer_device_address_features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES };
        if (!try_enable_device_feature(device, VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME, physical_device_buffer_device_address_features,
            device_create_info, [](const VkPhysicalDeviceBufferDeviceAddressFeatures& features) -> bool { return features.bufferDeviceAddress == VK_TRUE; })) {
            return false;
        }

        VkPhysicalDevicePerformanceQueryFeaturesKHR physical_device_performance_query_features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PERFORMANCE_QUERY_FEATURES_KHR };
        try_enable_device_feature(device, VK_KHR_PERFORMANCE_QUERY_EXTENSION_NAME, physical_device_performance_query_features,
            device_create_info, [](const VkPhysicalDevicePerformanceQueryFeaturesKHR& features) -> bool { return features.performanceCounterQueryPools == VK_TRUE; });

        VkPhysicalDeviceHostQueryResetFeatures physical_device_host_query_reset_features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES };
        try_enable_device_feature(device, VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME, physical_device_host_query_reset_features,
            device_create_info, [](const VkPhysicalDeviceHostQueryResetFeatures& features) -> bool { return features.hostQueryReset == VK_TRUE; });

        VkPhysicalDeviceSynchronization2Features physical_device_synchronization2_features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES };
        if (!try_enable_device_feature(device, VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME, physical_device_synchronization2_features,
            device_create_info, [](const VkPhysicalDeviceSynchronization2Features& features) -> bool { return features.synchronization2 == VK_TRUE; })) {
            return false;
        }

        VkPhysicalDeviceDynamicRenderingFeatures physical_device_dynamic_rendering_features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES };
        if (!try_enable_device_feature(device, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME, physical_device_dynamic_rendering_features,
            device_create_info, [](const VkPhysicalDeviceDynamicRenderingFeatures& features) -> bool { return features.dynamicRendering == VK_TRUE; })) {
            return false;
        }

        VkPhysicalDeviceShaderDemoteToHelperInvocationFeaturesEXT physical_device_shader_demote_to_helper_invocation_features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DEMOTE_TO_HELPER_INVOCATION_FEATURES_EXT };
        try_enable_device_feature(device, VK_EXT_SHADER_DEMOTE_TO_HELPER_INVOCATION_EXTENSION_NAME, physical_device_shader_demote_to_helper_invocation_features,
            device_create_info, [](const VkPhysicalDeviceShaderDemoteToHelperInvocationFeaturesEXT& features) -> bool { return features.shaderDemoteToHelperInvocation == VK_TRUE; });

        VkPhysicalDevice16BitStorageFeaturesKHR physical_device_16_bit_storage_features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES };
        if (!try_enable_device_feature(device, VK_KHR_16BIT_STORAGE_EXTENSION_NAME, physical_device_16_bit_storage_features,
            device_create_info, [](const VkPhysicalDevice16BitStorageFeaturesKHR& features) -> bool { return features.storagePushConstant16; })) {
            return false;
        }

        VkPhysicalDeviceExtendedDynamicStateFeaturesEXT physical_device_extended_dynamic_state_features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT };
        try_enable_device_feature(device, VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME, physical_device_extended_dynamic_state_features,
            device_create_info, [](const VkPhysicalDeviceExtendedDynamicStateFeaturesEXT& features) -> bool { return features.extendedDynamicState; });

        VkPhysicalDeviceRayQueryFeaturesKHR physical_device_ray_query_features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR };
        VkPhysicalDeviceAccelerationStructureFeaturesKHR physical_device_acceleration_structure_features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
        VkPhysicalDeviceRayTracingPipelineFeaturesKHR physical_device_ray_tracing_pipeline_features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR };
        if (create_info.require_features.ray_tracing) {
            if (!try_enable_device_feature(device, VK_KHR_RAY_QUERY_EXTENSION_NAME, physical_device_ray_query_features,
                device_create_info, [](const VkPhysicalDeviceRayQueryFeaturesKHR& features) -> bool { return features.rayQuery; })) {
                return false;
            }

            if (!try_enable_device_feature(device, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, physical_device_acceleration_structure_features,
                device_create_info, [](const VkPhysicalDeviceAccelerationStructureFeaturesKHR& features) -> bool { return features.accelerationStructure; })) {
                return false;
            }

            if (!try_enable_device_feature(device, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, physical_device_ray_tracing_pipeline_features,
                device_create_info, [](const VkPhysicalDeviceRayTracingPipelineFeaturesKHR& features) -> bool { return features.rayTracingPipeline; })) {
                return false;
            }
        }

#if USE_NSIGHT_AFTERMATH
        VkPhysicalDeviceDiagnosticsConfigFeaturesNV physical_device_diagnostics_config_features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DIAGNOSTICS_CONFIG_FEATURES_NV };
        if(is_device_extension_supported(device, VK_NV_DEVICE_DIAGNOSTICS_CONFIG_EXTENSION_NAME)) {
            VkPhysicalDeviceFeatures2KHR physical_device_features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR };
            physical_device_features.pNext = &physical_device_diagnostics_config_features;
            vkGetPhysicalDeviceFeatures2KHR(device.physical, &physical_device_features);

            if(physical_device_diagnostics_config_features.diagnosticsConfig) {
                device_extensions_.push_back(VK_NV_DEVICE_DIAGNOSTICS_CONFIG_EXTENSION_NAME);
                physical_device_diagnostics_config_features.pNext = (void*)device_create_info.pNext;
                device_create_info.pNext = &physical_device_diagnostics_config_features;
            }
        }

        // Initialize Nsight Aftermath for this device.
		//
		// * ENABLE_RESOURCE_TRACKING - this will include additional information about the
		//   resource related to a GPU virtual address seen in case of a crash due to a GPU
		//   page fault. This includes, for example, information about the size of the
		//   resource, its format, and an indication if the resource has been deleted.
		//
		// * ENABLE_AUTOMATIC_CHECKPOINTS - this will enable automatic checkpoints for
		//   all draw calls, compute dispatchs, and resource copy operations that capture
		//   CPU call stacks for those event.
		//
		//   Using this option should be considered carefully. It can cause very high CPU overhead.
		//
		// * ENABLE_SHADER_DEBUG_INFO - this instructs the shader compiler to
		//   generate debug information (line tables) for all shaders. Using this option
		//   should be considered carefully. It may cause considerable shader compilation
		//   overhead and additional overhead for handling the corresponding shader debug
		//   information callbacks.
		//

		VkDeviceDiagnosticsConfigCreateInfoNV aftermath_info{ VK_STRUCTURE_TYPE_DEVICE_DIAGNOSTICS_CONFIG_CREATE_INFO_NV };
		aftermath_info.flags =
			VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_RESOURCE_TRACKING_BIT_NV |
			VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_AUTOMATIC_CHECKPOINTS_BIT_NV |
			VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_SHADER_DEBUG_INFO_BIT_NV;

		if (is_device_extension_enabled(VK_NV_DEVICE_DIAGNOSTICS_CONFIG_EXTENSION_NAME))
		{
            aftermath_info.pNext = (void*)device_create_info.pNext;
            device_create_info.pNext = &aftermath_info;
		}
#endif

        device_create_info.enabledExtensionCount = static_cast<uint32_t>(device_extensions_.size());
        device_create_info.ppEnabledExtensionNames = device_extensions_.data();
        device_create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
        device_create_info.pQueueCreateInfos = queue_create_infos.data();

        VK_CHECK_RESULT(vkCreateDevice(device.physical, &device_create_info, &vk_alloc_callbacks_, &device.logical),
                        "Failed to create logical device.");

        // Need to load volk for all the not-yet Vulkan-Hpp calls
        volkLoadDevice(device.logical);

        // Create vulkan memory allocator
        VmaVulkanFunctions vma_vulkan_func{};
        vma_vulkan_func.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
        vma_vulkan_func.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

        VmaAllocatorCreateInfo vma_allocator_create_info{};
        vma_allocator_create_info.pVulkanFunctions = &vma_vulkan_func;
        vma_allocator_create_info.physicalDevice = device.physical;
        vma_allocator_create_info.device = device.logical;
        vma_allocator_create_info.instance = vk_instance_;
        vma_allocator_create_info.pAllocationCallbacks = &vk_alloc_callbacks_;

        // NOTE: Nsight graphics using VkImportMemoryHostPointerEXT that cannot be used with dedicated memory allocation
        bool can_get_memory_requirements = is_device_extension_supported(device, VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
        //bool has_dedicated_allocation = is_device_extension_supported(device, VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME);
        bool has_dedicated_allocation = false;
        if (can_get_memory_requirements && has_dedicated_allocation && is_device_extension_enabled(VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME)) {
            vma_allocator_create_info.flags |= VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;
        }
        
        if (is_device_extension_supported(device, VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME) && is_device_extension_enabled(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME)) {
            vma_allocator_create_info.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
        }
        
        if (is_device_extension_supported(device, VK_EXT_MEMORY_BUDGET_EXTENSION_NAME) && is_device_extension_enabled(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME)) {
            vma_allocator_create_info.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
        }

        if (is_device_extension_supported(device, VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME) && is_device_extension_enabled(VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME)) {
            vma_allocator_create_info.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT;
        }

        if (is_device_extension_supported(device, VK_KHR_BIND_MEMORY_2_EXTENSION_NAME) && is_device_extension_enabled(VK_KHR_BIND_MEMORY_2_EXTENSION_NAME)) {
            vma_allocator_create_info.flags |= VMA_ALLOCATOR_CREATE_KHR_BIND_MEMORY2_BIT;
        }

        if (is_device_extension_supported(device, VK_AMD_DEVICE_COHERENT_MEMORY_EXTENSION_NAME) && is_device_extension_enabled(VK_AMD_DEVICE_COHERENT_MEMORY_EXTENSION_NAME)) {
            vma_allocator_create_info.flags |= VMA_ALLOCATOR_CREATE_AMD_DEVICE_COHERENT_MEMORY_BIT;
        }

        VK_CHECK_RESULT(vmaCreateAllocator(&vma_allocator_create_info, &vma_allocator_),
                        "Failed to create memory allocator.");

        // Create device queue handles

        for (uint32_t queue_family_index = 0U; queue_family_index < static_cast<uint32_t>(device.queue_family_props.size()); ++queue_family_index) {
            VkQueueFamilyProperties const& queue_family_property = device.queue_family_props[queue_family_index];

            VkBool32 present_supported;
            if (vkGetPhysicalDeviceSurfaceSupportKHR(device.physical, queue_family_index, vk_surface_, &present_supported) != VK_SUCCESS) {
                present_supported = VK_FALSE;
            }

            bool is_graphics_commands_supported = (queue_family_property.queueFlags & VK_QUEUE_GRAPHICS_BIT) == VK_QUEUE_GRAPHICS_BIT;
            bool is_compute_commands_supported = (queue_family_property.queueFlags & VK_QUEUE_COMPUTE_BIT) == VK_QUEUE_COMPUTE_BIT;
            bool is_copy_commands_supported = (queue_family_property.queueFlags & VK_QUEUE_TRANSFER_BIT) == VK_QUEUE_TRANSFER_BIT;

            QueueType queue_family_type{};
            if (present_supported &&
                is_graphics_commands_supported &&
                is_compute_commands_supported &&
                is_copy_commands_supported) {
                queue_family_type = QueueType::eDirect;
            }
            else if (present_supported &&
                is_compute_commands_supported &&
                is_copy_commands_supported) {
                queue_family_type = QueueType::eCompute;
            }
            else if (is_copy_commands_supported) {
                queue_family_type = QueueType::eCopy;
            }

            auto& family_group = device.queue_families[static_cast<uint32_t>(queue_family_type)];
            auto& new_family = family_group.emplace_back();
            new_family.index = queue_family_index;

            for (uint32_t queue_index = 0U; queue_index < queue_family_property.queueCount; ++queue_index) {
                new_family.queues.push_back(VulkanQueue::construct(*this, new_family.index, queue_index));
            }

#ifndef NDEBUG
            std::string supported_commands;

            if (present_supported)
                supported_commands += "present|";
            if (is_graphics_commands_supported)
                supported_commands += "graphics|";
            if (is_compute_commands_supported)
                supported_commands += "compute|";
            if (is_copy_commands_supported)
                supported_commands += "transfer|";

            if (!supported_commands.empty())
                supported_commands.pop_back();

            spdlog::debug("[VulkanGraphicsContext] Found {} \"{}\" queue{} that support: {} commands.",
                queue_family_property.queueCount,
                (queue_family_type == QueueType::eDirect) ? "Direct" : (queue_family_type == QueueType::eCompute) ? "Compute" : "Copy",
                queue_family_property.queueCount > 1 ? "s" : "",
                supported_commands);
#endif
        }

		return true;
	}

    auto VulkanGraphicsContext::get_queue_count(QueueType queue_type) -> uint32_t {
        auto& device = devices_[selected_device_index_];
        auto& family_group = device.queue_families[static_cast<uint32_t>(queue_type)];
        if (family_group.empty()) {
            return 0u;
        }

        uint32_t queue_count{ 0u };
        for (auto& family : family_group) {
            queue_count += static_cast<uint32_t>(family.queues.size());
        }

        return queue_count;
    }

    auto VulkanGraphicsContext::get_queue(QueueType queue_type, uint32_t queue_index) -> std::expected<std::shared_ptr<IGFXQueue>, bool> {
        auto& device = devices_[selected_device_index_];
        auto& family_group = device.queue_families[static_cast<uint32_t>(queue_type)];
        if (family_group.empty()) {
            return std::unexpected(false);
        }

        // TODO: Check that index is in queue range
        return family_group.front().queues[queue_index];
    }

    auto VulkanGraphicsContext::create_semaphore(uint64_t value) const -> std::shared_ptr<IGFXSemaphore> {
        return VulkanSemaphore::construct(*this, value);
    }

    auto VulkanGraphicsContext::is_instance_extension_enabled(const char* extension_name) const noexcept -> bool {
        return std::find_if(instance_extensions_.begin(), instance_extensions_.end(),
            [extension_name](auto& instance_extension) {
                return std::strcmp(instance_extension, extension_name) == 0;
            }) != instance_extensions_.end();
    }

    auto VulkanGraphicsContext::is_device_extension_enabled(const char* extension_name) const noexcept -> bool {
        return std::find_if(device_extensions_.begin(), device_extensions_.end(),
            [extension_name](auto& device_extension) {
                return std::strcmp(device_extension, extension_name) == 0;
            }) != device_extensions_.end();
    }

    void VulkanGraphicsContext::set_debug_name(VkObjectType object_type, uint64_t object_handle, std::string_view name) const {
        auto& device = devices_[selected_device_index_];
        if(VK_EXT_debug_utils_enabled) {
            VkDebugUtilsObjectNameInfoEXT object_name_info{ VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
            object_name_info.objectType = object_type;
            object_name_info.objectHandle = object_handle;
            object_name_info.pObjectName = name.data();
            vkSetDebugUtilsObjectNameEXT(device.logical, &object_name_info);
        }
        else if(VK_EXT_debug_marker_enabled) {
            VkDebugMarkerObjectNameInfoEXT object_name_info{ VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT };
            object_name_info.objectType = (VkDebugReportObjectTypeEXT)object_type;
            object_name_info.object = object_handle;
            object_name_info.pObjectName = name.data();
            vkDebugMarkerSetObjectNameEXT(device.logical, &object_name_info);
        }
    }

    void VulkanGraphicsContext::set_debug_tag(VkObjectType object_type, uint64_t object_handle, uint64_t tag_name, const void* tag_data, size_t tag_data_size) const {
        auto& device = devices_[selected_device_index_];
        if(VK_EXT_debug_utils_enabled) {
            VkDebugUtilsObjectTagInfoEXT object_tag_info{ VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_TAG_INFO_EXT };
            object_tag_info.objectHandle = object_handle;
            object_tag_info.tagName = tag_name;
            object_tag_info.pTag = tag_data;
            object_tag_info.tagSize = tag_data_size;
            vkSetDebugUtilsObjectTagEXT(device.logical, &object_tag_info);
        }
        else if(VK_EXT_debug_marker_enabled) {
            VkDebugMarkerObjectTagInfoEXT object_tag_info{ VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_TAG_INFO_EXT };
            object_tag_info.objectType = (VkDebugReportObjectTypeEXT)object_type;
            object_tag_info.object = object_handle;
            object_tag_info.tagName = tag_name;
            object_tag_info.pTag = tag_data;
            object_tag_info.tagSize = tag_data_size;
            vkDebugMarkerSetObjectTagEXT(device.logical, &object_tag_info);
        }
    }

    void VulkanGraphicsContext::begin_label(VkCommandBuffer command_buffer, std::string_view name, uint32_t color) const {
        if(VK_EXT_debug_utils_enabled) {
            VkDebugUtilsLabelEXT debug_label{ VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT };
            debug_label.pLabelName = name.data();
            make_color(color, debug_label.color);
            vkCmdBeginDebugUtilsLabelEXT(command_buffer, &debug_label);
        }
        else if(VK_EXT_debug_marker_enabled) {
            VkDebugMarkerMarkerInfoEXT debug_marker{ VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT };
            debug_marker.pMarkerName = name.data();
            make_color(color, debug_marker.color);
            vkCmdDebugMarkerBeginEXT(command_buffer, &debug_marker);
        }
    }

    void VulkanGraphicsContext::end_label(VkCommandBuffer command_buffer) const {
        if(VK_EXT_debug_utils_enabled) {
            vkCmdEndDebugUtilsLabelEXT(command_buffer);
        }
        else if(VK_EXT_debug_marker_enabled) {
            vkCmdDebugMarkerEndEXT(command_buffer);
        }
    }

    void VulkanGraphicsContext::insert_label(VkCommandBuffer command_buffer, std::string_view name, uint32_t color) const {
        if(VK_EXT_debug_utils_enabled) {
            VkDebugUtilsLabelEXT debug_label{ VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT };
            debug_label.pLabelName = name.data();
            make_color(color, debug_label.color);
            vkCmdInsertDebugUtilsLabelEXT(command_buffer, &debug_label);
        }
        else if(VK_EXT_debug_marker_enabled) {
            VkDebugMarkerMarkerInfoEXT debug_marker{ VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT };
            debug_marker.pMarkerName = name.data();
            make_color(color, debug_marker.color);
            vkCmdDebugMarkerInsertEXT(command_buffer, &debug_marker);
        }
    }

    auto VulkanGraphicsContext::is_device_extension_supported(const VkDeviceHandle& device, const char* extension_name) const -> bool {
        return std::find_if(device.extensions.begin(), device.extensions.end(),
                            [extension_name](auto& device_extension) {
            return std::strcmp(device_extension.extensionName, extension_name) == 0;
        }) != device.extensions.end();
    }
}