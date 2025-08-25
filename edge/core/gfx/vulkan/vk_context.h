#pragma once

#include <atomic>
#include <array>
#include <vector>
#include <thread>
#include <mutex>
#include <string_view>
#include <unordered_map>

#include "../gfx_context.h"

#include <vulkan/vulkan.h>
#include <volk.h>

#include <vk_mem_alloc.h>

namespace edge::gfx {
	class VulkanGraphicsContext;
	class VulkanQueue;

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

	struct VkQueueFamily {
		uint32_t index;
		std::vector<std::shared_ptr<VulkanQueue>> queues;
	};

    struct VkDeviceHandle {
        VkPhysicalDevice physical;
        VkDevice logical;

        VkPhysicalDeviceProperties properties;
        std::vector<VkExtensionProperties> extensions;
        std::vector<VkQueueFamilyProperties> queue_family_props;

		std::array<std::vector<VkQueueFamily>, 3ull> queue_families{};
    };

	struct VkPhysicalDeviceDesc {
		VkPhysicalDevice handle;

		VkPhysicalDeviceVulkan12Features features_12;
		VkPhysicalDeviceFeatures2 features;

		VkPhysicalDeviceProperties properties;
		VkPhysicalDeviceMemoryProperties memory_properties;
	};


	class VulkanSemaphore final : public IGFXSemaphore {
	public:
		~VulkanSemaphore() override;

		static auto construct(const VulkanGraphicsContext& ctx, uint64_t initial_value) -> std::unique_ptr<VulkanSemaphore>;

		auto signal(uint64_t value) -> SyncResult override;
		auto wait(uint64_t value, std::chrono::nanoseconds timeout = std::chrono::nanoseconds::max()) -> SyncResult override;

		auto is_completed(uint64_t value) const -> bool override;
		auto get_value() const -> uint64_t override;

		auto set_value(uint64_t value) -> void {
			value_ = std::max(value_, value);
		}

		auto get_handle() const -> VkSemaphore {
			return handle_;
		}
	private:
		auto _construct(const VulkanGraphicsContext& ctx, uint64_t initial_value) -> bool;

		VkDevice device_{ VK_NULL_HANDLE };
		VkAllocationCallbacks const* allocator_{ nullptr };
		VkSemaphore handle_{ VK_NULL_HANDLE };
		uint64_t value_{ 0ull };
	};

	class VulkanQueue final : public IGFXQueue {
	public:
		~VulkanQueue() override;

		static auto construct(const VulkanGraphicsContext& ctx, uint32_t family_index, uint32_t queue_index) -> std::unique_ptr<VulkanQueue>;

		auto submit(const SignalQueueInfo& submit_info) -> SyncResult override;
		auto wait_idle() -> void override;

		auto get_handle() const -> VkQueue {
			return handle_;
		}
	private:
		auto _construct(const VulkanGraphicsContext& ctx, uint32_t family_index, uint32_t queue_index) -> bool;

		VkDevice device_{ VK_NULL_HANDLE };
		VkQueue handle_{ VK_NULL_HANDLE };
		uint32_t family_index_{ 0u };
		uint32_t queue_index_{ 0u };
	};

	class VulkanGraphicsContext final : public IGFXContext {
	public:
		~VulkanGraphicsContext() override;

		static auto construct() -> std::unique_ptr<VulkanGraphicsContext>;

		auto create(const GraphicsContextCreateInfo& create_info) -> bool override;

		auto get_queue_count(QueueType queue_type) -> uint32_t override;
		auto get_queue(QueueType queue_type, uint32_t queue_index) -> std::expected<std::shared_ptr<IGFXQueue>, bool> override;

		auto create_semaphore(uint64_t value) const -> std::shared_ptr<IGFXSemaphore> override;

        auto is_instance_extension_enabled(const char* extension_name) const noexcept -> bool;
        auto is_device_extension_enabled(const char* extension_name) const noexcept -> bool;

        void set_debug_name(VkObjectType object_type, uint64_t object_handle, std::string_view name) const;
        void set_debug_tag(VkObjectType object_type, uint64_t object_handle, uint64_t tag_name, const void* tag_data, size_t tag_data_size) const;
        void begin_label(VkCommandBuffer command_buffer, std::string_view name, uint32_t color) const;
        void end_label(VkCommandBuffer command_buffer) const;
        void insert_label(VkCommandBuffer command_buffer, std::string_view name, uint32_t color) const;

		auto get_physical_device() const -> VkPhysicalDevice {
			return devices_[selected_device_index_].physical;
		}

		auto get_logical_device() const -> VkDevice {
			return devices_[selected_device_index_].logical;
		}

		auto get_allocation_callbacks() const -> VkAllocationCallbacks const* {
			return &vk_alloc_callbacks_;
		}
	private:
        auto is_device_extension_supported(const VkDeviceHandle& device, const char* extension_name) const -> bool;

		template<typename T, typename FeatureCheckFn>
		auto try_enable_device_feature(const VkDeviceHandle& device, const char* feature_name, T& features, VkDeviceCreateInfo& device_create_info, FeatureCheckFn&& feature_check) -> bool {
			VkPhysicalDeviceFeatures2KHR physical_device_features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR };
			physical_device_features.pNext = &features;
			vkGetPhysicalDeviceFeatures2KHR(device.physical, &physical_device_features);

			if (feature_check(features)) {
				device_extensions_.push_back(feature_name);
				features.pNext = (void*)device_create_info.pNext;
				device_create_info.pNext = &features;
				return true;
			}

			return false;
		}
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
		bool VK_EXT_debug_report_enabled{ false };
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