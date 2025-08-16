#pragma once

#include <atomic>
#include <vector>
#include <thread>
#include <unordered_map>

#include "../context.h"

#include <vulkan/vulkan.h>

namespace edge::gfx {
	struct VkMemoryAllocationDesc {
		size_t size;
		size_t align;
		VkSystemAllocationScope scope;
        std::thread::id thread_id;
	};

	struct VkMemoryAllocationStats {
        std::atomic<size_t> total_bytes_allocated;
        std::atomic<size_t> allocation_count;
        std::atomic<size_t> deallocation_count;
        std::mutex mutex;
		std::unordered_map<void*, VkMemoryAllocationDesc> allocation_map;
	};

	struct VkPhysicalDeviceDesc {
		VkPhysicalDevice handle;

		VkPhysicalDeviceVulkan12Features features_12;
		VkPhysicalDeviceFeatures2 features;

		VkPhysicalDeviceProperties properties;
		VkPhysicalDeviceMemoryProperties memory_properties;
	};

	class VulkanGraphicsContext final : public GraphicsContextInterface {
	public:
		~VulkanGraphicsContext() override;

		static auto construct() -> std::unique_ptr<VulkanGraphicsContext>;

		auto create(const GraphicsContextCreateInfo& create_info) -> bool override;

        auto is_instance_extension_enabled(const char* extension_name) const noexcept -> bool;
        auto is_device_extension_enabled(const char* extension_name) const noexcept -> bool;
	private:

		bool volk_initialized_{ false };

		VkAllocationCallbacks vk_alloc_callbacks_{};
		VkMemoryAllocationStats memalloc_stats_{};

		VkInstance vk_instance_{ VK_NULL_HANDLE };
        std::vector<const char*> instance_extensions_;

#if defined(ENGINE_DEBUG) || defined(VULKAN_VALIDATION_LAYERS)
		/**
		 * @brief Debug utils messenger callback for VK_EXT_Debug_Utils
		 */
		VkDebugUtilsMessengerEXT vk_debug_utils_messenger_{ VK_NULL_HANDLE };

		/**
		 * @brief The debug report callback
		 */
		VkDebugReportCallbackEXT vk_debug_report_callback_{ VK_NULL_HANDLE };
#endif

		std::vector<VkPhysicalDevice> physical_devices_{};

		VkSurfaceKHR vk_surface_{ VK_NULL_HANDLE };

        int32_t selected_device_index_{ -1 };
        VkDevice logical_device_{ VK_NULL_HANDLE };
        std::vector<const char*> device_extensions_{};
	};
}