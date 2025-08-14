#pragma once

#include <vector>
#include <unordered_map>

#include "../context.h"

#include <vulkan/vulkan.h>

namespace edge::gfx {
	struct VkMemoryAllocationDesc {
		size_t size;
		size_t align;
		VkSystemAllocationScope scope;
	};

	struct VkMemoryAllocationStats {
		size_t total_bytes_allocated;
		size_t allocation_count;
		size_t deallocation_count;
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
	private:

		bool volk_initialized_{ false };

		VkAllocationCallbacks vk_alloc_callbacks_{};
		VkMemoryAllocationStats memalloc_stats_{};

		VkInstance vk_instance_;

#if defined(ENGINE_DEBUG) || defined(VULKAN_VALIDATION_LAYERS)
		/**
		 * @brief Debug utils messenger callback for VK_EXT_Debug_Utils
		 */
		VkDebugUtilsMessengerEXT vk_debug_utils_messenger_;

		/**
		 * @brief The debug report callback
		 */
		VkDebugReportCallbackEXT vk_debug_report_callback_;
#endif

		std::vector<VkPhysicalDevice> physical_devices_;

		VkSurfaceKHR vk_surface_;
	};
}