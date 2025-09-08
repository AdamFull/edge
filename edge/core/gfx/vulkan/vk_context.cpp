#include "vk_context.h"

#include "../../platform/platform.h"

#include <cstdlib>

namespace edge::gfx {
    extern "C" void* VKAPI_CALL vkmemalloc(void* user_data, size_t size, size_t alignment, vk::SystemAllocationScope allocation_scope) {
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

    extern "C" void* VKAPI_CALL vkmemrealloc(void* user_data, void* old, size_t size, size_t alignment, vk::SystemAllocationScope allocation_scope) {
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

    extern "C" void VKAPI_CALL vkinternalmemalloc(void* user_data, size_t size, vk::InternalAllocationType allocation_type, vk::SystemAllocationScope allocation_scope) {

    }

    extern "C" void VKAPI_CALL vkinternalmemfree(void* user_data, size_t size, vk::InternalAllocationType allocation_type, vk::SystemAllocationScope allocation_scope) {

    }

#define GFX_SCOPE "Vulkan GFX Context"

    VulkanGraphicsContext::~VulkanGraphicsContext() {
        if(vma_allocator_) {
            spdlog::debug("[Vulkan Graphics Context]: Destroying VMA allocator");
            vmaDestroyAllocator(vma_allocator_);
        }

        // Check that all allocated vulkan objects was deallocated
        if (memalloc_stats_.allocation_count != memalloc_stats_.deallocation_count) {
            spdlog::error("[Vulkan Graphics Context]: Memory leaks detected!\n Allocated: {}, Deallocated: {} objects. Leaked {} bytes.",
                memalloc_stats_.allocation_count.load(), memalloc_stats_.deallocation_count.load(), memalloc_stats_.total_bytes_allocated.load());

            for (const auto& allocation : memalloc_stats_.allocation_map) {
                spdlog::warn("{:#010x} : {} bytes, {} byte alignment, {} scope",
                    reinterpret_cast<uintptr_t>(allocation.first), allocation.second.size, allocation.second.align, vk::to_string(allocation.second.scope));
            }
        }
        else {
            spdlog::info("[Vulkan Graphics Context]: All memory correctly deallocated");
        }

        volkFinalize();
	}

    auto VulkanGraphicsContext::construct() -> std::unique_ptr<VulkanGraphicsContext> {
        volkInitialize();

        auto self = std::make_unique<VulkanGraphicsContext>();
        VULKAN_HPP_DEFAULT_DISPATCHER.init(self->vk_dynamic_loader_.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr"));
        return self;
    }

	auto VulkanGraphicsContext::create(const GraphicsContextCreateInfo& create_info) -> bool {
		memalloc_stats_.total_bytes_allocated = 0ull;
		memalloc_stats_.allocation_count = 0ull;
		memalloc_stats_.deallocation_count = 0ull;

		// Create allocation callbacks
		vk_alloc_callbacks_.pfnAllocation = (vk::PFN_AllocationFunction)vkmemalloc;
		vk_alloc_callbacks_.pfnFree = (vk::PFN_FreeFunction)vkmemfree;
		vk_alloc_callbacks_.pfnReallocation = (vk::PFN_ReallocationFunction)vkmemrealloc;
		vk_alloc_callbacks_.pfnInternalAllocation = (vk::PFN_InternalAllocationNotification) vkinternalmemalloc;
		vk_alloc_callbacks_.pfnInternalFree = (vk::PFN_InternalFreeNotification)vkinternalmemfree;
		vk_alloc_callbacks_.pUserData = &memalloc_stats_;

        auto instance_result = vkw::InstanceBuilder{ &vk_alloc_callbacks_ }
            .set_app_name("MyApp")
            .set_app_version(1, 0, 0)
            .set_engine_name("EdgeGameEngine")
            .set_engine_version(1, 0, 0)
            .set_api_version(1, 2, 0)

            .enable_surface()

#if defined(USE_VALIDATION_LAYERS)
            .add_layer("VK_LAYER_KHRONOS_validation")
#if defined(VKW_VALIDATION_LAYERS_SYNCHRONIZATION)
            .add_layer("VK_LAYER_KHRONOS_synchronization2")
#endif // VULKAN_VALIDATION_LAYERS_SYNCHRONIZATION
#endif // USE_VALIDATION_LAYERS

            // Enable validation features
#if defined(USE_VALIDATION_LAYER_FEATURES)
            .add_validation_feature_enable(vk::ValidationFeatureEnableEXT::eDebugPrintf)
#if defined(VKW_VALIDATION_LAYERS_GPU_ASSISTED)
            .add_validation_feature_enable(vk::ValidationFeatureEnableEXT::eGpuAssistedReserveBindingSlot)
            .add_validation_feature_enable(vk::ValidationFeatureEnableEXT::eGpuAssisted)
#endif
#if defined(VKW_VALIDATION_LAYERS_BEST_PRACTICES)
            .add_validation_feature_enable(vk::ValidationFeatureEnableEXT::eBestPractices)
#endif
#if defined(VKW_VALIDATION_LAYERS_SYNCHRONIZATION)
            .add_validation_feature_enable(vk::ValidationFeatureEnableEXT::eSynchronizationValidation)
#endif
#endif

            .add_extension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME)

#if defined(VKW_DEBUG)
            .enable_debug_utils()
#endif

#if defined(VKW_ENABLE_PORTABILITY)
            .enable_portability()
#endif
            .build();

        if (!instance_result) {
            GFX_LOGE("Failed to create instance. Reason: {}.", vk::to_string(instance_result.error()));
            return false;
        }
        
        vk_instance_ = std::move(instance_result.value());

        // Create surface
        {
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
            vk::AndroidSurfaceCreateInfoKHR surface_create_info{};
            surface_create_info.window = static_cast<ANativeWindow*>(create_info.window->get_native_handle());
            auto result = instance_.createWin32SurfaceKHR(&surface_create_info, &vk_alloc_callbacks_, &vk_surface_);
#elif defined(VK_USE_PLATFORM_WIN32_KHR)
            auto hWnd = static_cast<HWND>(create_info.window->get_native_handle());
            HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE);

            vk::Win32SurfaceCreateInfoKHR surface_create_info{};
            surface_create_info.hwnd = hWnd;
            surface_create_info.hinstance = hInstance;
            auto result = vk_instance_.createWin32SurfaceKHR(&surface_create_info, &vk_alloc_callbacks_, &vk_surface_);
#endif
            if (result != vk::Result::eSuccess) {
                GFX_LOGE("Failed to create surface.");
                return false;
            }
        }

        // Select device
        auto device_selector = vkw::DeviceSelector{ vk_instance_, &vk_alloc_callbacks_ }
            .set_surface(vk_surface_)
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
            .add_extension(VK_EXT_DEBUG_MARKER_EXTENSION_NAME, false)
            .add_extension(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME, false)
            .add_extension(VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME, false)
            .add_extension(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME)
            .add_extension(VK_KHR_PERFORMANCE_QUERY_EXTENSION_NAME, false)
            .add_extension(VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME, false)
            .add_extension(VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME, false)
            .add_extension(VK_KHR_BIND_MEMORY_2_EXTENSION_NAME, false)
            .add_extension(VK_AMD_DEVICE_COHERENT_MEMORY_EXTENSION_NAME, false)
#if !defined(VK_USE_PLATFORM_ANDROID_KHR)
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
            .add_feature<vk::PhysicalDeviceShaderDemoteToHelperInvocationFeaturesEXT>()
            .add_feature<vk::PhysicalDevice16BitStorageFeaturesKHR>()
            .add_feature<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>()
#if USE_NSIGHT_AFTERMATH
            .add_feature<vk::PhysicalDeviceDiagnosticsConfigFeaturesNV>(false)
#endif
            .add_feature<vk::PhysicalDeviceAccelerationStructureFeaturesKHR>(create_info.require_features.ray_tracing)
            .add_feature<vk::PhysicalDeviceRayTracingPipelineFeaturesKHR>(create_info.require_features.ray_tracing)
            .add_feature<vk::PhysicalDeviceRayQueryFeaturesKHR>(create_info.require_features.ray_tracing)
            .select();

        if (!device_selector) {
            GFX_LOGE("Failed to find suitable device.");
            return false;
        }

        vkw_device_ = std::move(device_selector.value());

        // Create debug interface
        //if (auto result = vkw::DebugUtils::create_unique(instance_, device_); result.has_value()) {
        //    debug_interface_ = std::move(result.value());
        //}
        //else if (auto result = vkw::DebugReport::create_unique(instance_, device_); result.has_value()) {
        //    debug_interface_ = std::move(result.value());
        //}
        //else {
        //    debug_interface_ = std::move(vkw::DebugNone::create_unique().value());
        //}
        
#if USE_NSIGHT_AFTERMATH
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

        vk::DeviceDiagnosticsConfigCreateInfoNV aftermath_info{};
		aftermath_info.flags =
            vk::DeviceDiagnosticsConfigFlagBitsNV::eEnableResourceTracking |
            vk::DeviceDiagnosticsConfigFlagBitsNV::eEnableAutomaticCheckpoints |
            vk::DeviceDiagnosticsConfigFlagBitsNV::eEnableShaderDebugInfo;

		if (is_device_extension_enabled(VK_NV_DEVICE_DIAGNOSTICS_CONFIG_EXTENSION_NAME))
		{
            aftermath_info.pNext = (void*)device_create_info.pNext;
            device_create_info.pNext = &aftermath_info;
		}
#endif

        // Create vulkan memory allocator
        VmaVulkanFunctions vma_vulkan_func{};
        vma_vulkan_func.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
        vma_vulkan_func.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

        VmaAllocatorCreateInfo vma_allocator_create_info{};
        vma_allocator_create_info.pVulkanFunctions = &vma_vulkan_func;
        vma_allocator_create_info.physicalDevice = vkw_device_;
        vma_allocator_create_info.device = vkw_device_;
        vma_allocator_create_info.instance = vk_instance_;
        vma_allocator_create_info.pAllocationCallbacks = (VkAllocationCallbacks*)(&vk_alloc_callbacks_);

        // NOTE: Nsight graphics using VkImportMemoryHostPointerEXT that cannot be used with dedicated memory allocation
        bool is_nsignt_graphics_attached{ false };
#ifdef VK_USE_PLATFORM_WIN32_KHR
        {
            HMODULE nsight_graphics_module = GetModuleHandle("Nvda.Graphics.Interception.dll");
            is_nsignt_graphics_attached = (nsight_graphics_module != nullptr);
        }
#endif
        bool can_get_memory_requirements = vkw_device_.is_enabled(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
        bool has_dedicated_allocation = vkw_device_.is_enabled(VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME);

        if (can_get_memory_requirements && has_dedicated_allocation && !is_nsignt_graphics_attached) {
            vma_allocator_create_info.flags |= VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;
        }

        if (vkw_device_.is_enabled(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME)) {
            vma_allocator_create_info.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
        }

        if (vkw_device_.is_enabled(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME)) {
            vma_allocator_create_info.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
        }

        if (vkw_device_.is_enabled(VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME)) {
            vma_allocator_create_info.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT;
        }
        
        if (vkw_device_.is_enabled(VK_KHR_BIND_MEMORY_2_EXTENSION_NAME)) {
            vma_allocator_create_info.flags |= VMA_ALLOCATOR_CREATE_KHR_BIND_MEMORY2_BIT;
        }
        
        if (vkw_device_.is_enabled(VK_AMD_DEVICE_COHERENT_MEMORY_EXTENSION_NAME)) {
            vma_allocator_create_info.flags |= VMA_ALLOCATOR_CREATE_AMD_DEVICE_COHERENT_MEMORY_BIT;
        }

        if (auto result = vmaCreateAllocator(&vma_allocator_create_info, &vma_allocator_); result != VK_SUCCESS) {
            return false;
        }

		return true;
	}

    auto VulkanGraphicsContext::create_queue(QueueType queue_type) const -> std::shared_ptr<IGFXQueue> {
        //auto& device = devices_[selected_device_index_];
        //auto& family_groups = device.queue_type_to_family_map[static_cast<uint32_t>(queue_type)];
        //
        //// Lookup for free queue slots
        //for (auto family_index : family_groups) {
        //    auto& family_properties = device.queue_family_props[family_index];
        //    auto& queues_used = device.queue_family_queue_usages[family_index];
        //
        //    if (queues_used >= family_properties.queueCount) {
        //        // TODO: try to check freed indices
        //        return nullptr;
        //    }
        //
        //    return VulkanQueue::construct(*this, family_index, queues_used++);
        //}

        return nullptr;
    }

    auto VulkanGraphicsContext::create_semaphore(uint64_t value) const -> std::shared_ptr<IGFXSemaphore> {
        return VulkanSemaphore::construct(*this, value);
    }

    auto VulkanGraphicsContext::create_swapchain(const SwapchainCreateInfo& create_info) -> std::shared_ptr<IGFXSemaphore> {
        return nullptr;
    }
}