#pragma once

#include <atomic>
#include <vector>
#include <thread>
#include <mutex>
#include <string_view>
#include <unordered_map>

#include "../context.h"

#include <vulkan/vulkan.h>

#include <vk_mem_alloc.h>

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

    struct VkDeviceHandle {
        VkPhysicalDevice physical;
        VkDevice logical;

        VkPhysicalDeviceProperties properties;
        std::vector<VkExtensionProperties> extensions;
        std::vector<VkQueueFamilyProperties> queue_family_props;
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

        void set_debug_name(VkObjectType object_type, uint64_t object_handle, std::string_view name) const;
        void set_debug_tag(VkObjectType object_type, uint64_t object_handle, uint64_t tag_name, const void* tag_data, size_t tag_data_size) const;
        void begin_label(VkCommandBuffer command_buffer, std::string_view name, uint32_t color) const;
        void end_label(VkCommandBuffer command_buffer) const;
        void insert_label(VkCommandBuffer command_buffer, std::string_view name, uint32_t color) const;
	private:
        auto is_device_extension_supported(const VkDeviceHandle& device, const char* extension_name) const -> bool;
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

#if defined(VULKAN_DEBUG)
        bool VK_EXT_debug_utils_enabled{ false };
        bool VK_EXT_debug_marker_enabled{ false };
#endif

		VkSurfaceKHR vk_surface_{ VK_NULL_HANDLE };

        // Device
        int32_t selected_device_index_{ -1 };
        std::vector<const char*> device_extensions_{};
        std::vector<VkDeviceHandle> devices_{};

        VmaAllocator vma_allocator_{ VK_NULL_HANDLE };
	};
}