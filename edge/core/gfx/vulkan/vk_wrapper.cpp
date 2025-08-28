#include "vk_wrapper.h"

#include <atomic>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include <spdlog/spdlog.h>

static bool g_volk_initialized{ false };
static std::atomic<uint64_t> g_volk_ref_counter{ 0ull };

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
	namespace
	{
#define VKW_SCOPE "Vulkan Validation"
		VKAPI_ATTR VkBool32 VKAPI_CALL debug_utils_messenger_callback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity, VkDebugUtilsMessageTypeFlagsEXT message_type, const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data) {
			// Log debug message
			if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
				VKW_LOGT("{} - {}: {}", callback_data->messageIdNumber, callback_data->pMessageIdName, callback_data->pMessage);
			}
			else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
				VKW_LOGI("{} - {}: {}", callback_data->messageIdNumber, callback_data->pMessageIdName, callback_data->pMessage); 
			}
			else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
				VKW_LOGW("{} - {}: {}", callback_data->messageIdNumber, callback_data->pMessageIdName, callback_data->pMessage);
			}
			else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
				VKW_LOGE("{} - {}: {}", callback_data->messageIdNumber, callback_data->pMessageIdName, callback_data->pMessage);
			}
			return VK_FALSE;
		}

		static VKAPI_ATTR VkBool32 VKAPI_CALL debug_report_callback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT /*type*/,
			uint64_t /*object*/, size_t /*location*/, int32_t /*message_code*/,
			const char* layer_prefix, const char* message, void* /*user_data*/) {
			if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
				VKW_LOGE("{}: {}", layer_prefix, message);
			}
			else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) {
				VKW_LOGW("{}: {}", layer_prefix, message);
			}
			else if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT) {
				VKW_LOGW("{}: {}", layer_prefix, message);
			}
			else if (flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT) {
				VKW_LOGD("{}: {}", layer_prefix, message);
			}
			else {
				VKW_LOGI("{}: {}", layer_prefix, message);
			}
			return VK_FALSE;
		}
#undef VKW_SCOPE // Vulkan validation
	}
#endif

#define VKW_SCOPE "vkw::Instance"

	auto enumerate_instance_extension_properties(const char* layer_name) -> std::expected<std::vector<VkExtensionProperties>, VkResult> {
		uint32_t count;
		VKW_CHECK_RESULT(vkEnumerateInstanceExtensionProperties(layer_name, &count, nullptr));

		std::vector<VkExtensionProperties> output(count);
		VKW_CHECK_RESULT(vkEnumerateInstanceExtensionProperties(layer_name, &count, output.data()));

		return output;
	}

	auto enumerate_instance_layer_properties() -> std::expected<std::vector<VkLayerProperties>, VkResult> {
		uint32_t count;
		VKW_CHECK_RESULT(vkEnumerateInstanceLayerProperties(&count, nullptr));

		std::vector<VkLayerProperties> output(count);
		VKW_CHECK_RESULT(vkEnumerateInstanceLayerProperties(&count, output.data()));

		return output;
	}

	Instance::Instance() {
		VKW_TRY_INIT_VOLK();

		{
			auto request = enumerate_instance_extension_properties();
			if (!request.has_value()) {
				VKW_LOGE("Failed to enumerate instance extension properties. Reason: {}.", to_string(request.error()));
				return;
			}

			available_extensions_ = std::move(request.value());
		}

		if (auto request = enumerate_instance_layer_properties(); request.has_value()) {
			available_layers_ = std::move(request.value());
		}
	}

	Instance::~Instance() {
		VKW_TRY_DEINIT_VOLK();

		if (handle_) {
			vkDestroyInstance(handle_, allocator_);
		}
	}

	auto Instance::create(const VkApplicationInfo& app_info, VkAllocationCallbacks const* allocator) -> VkResult {
		allocator_ = allocator;

		VkApplicationInfo application_info{ app_info };
		VkInstanceCreateInfo create_info{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
		create_info.pApplicationInfo = &application_info;
		create_info.enabledExtensionCount = static_cast<uint32_t>(enabled_extensions_.size());
		create_info.ppEnabledExtensionNames = enabled_extensions_.data();
		create_info.enabledLayerCount = static_cast<uint32_t>(enabled_layers_.size());
		create_info.ppEnabledLayerNames = enabled_layers_.data();

#if (defined(VKW_ENABLE_PORTABILITY))
		if (portability_enumeration_available) {
			create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
		}
#endif

#ifdef USE_VALIDATION_LAYER_FEATURES
		bool validation_features{ false };
		{
			if (auto request = enumerate_instance_extension_properties("VK_LAYER_KHRONOS_validation"); request.has_value()) {
				for (auto& available_extension : *request) {
					if (strcmp(available_extension.extensionName, VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME) == 0) {
						validation_features = true;
						(void)try_enable_extension(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);
					}
				}
			}
		}

		VkValidationFeaturesEXT validation_features_info{ VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT };
		enabled_validation_features_.push_back(VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT);

		if (validation_features) {
			VKW_LOGD("Enabling validation features");
#if defined(VKW_VALIDATION_LAYERS_GPU_ASSISTED)
			enabled_validation_features_.emplace_back(VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_RESERVE_BINDING_SLOT_EXT);
			enabled_validation_features_.emplace_back(VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT);
#endif
#if defined(VKW_VALIDATION_LAYERS_BEST_PRACTICES)
			enabled_validation_features_.emplace_back(VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT);
#endif
#if defined(VKW_VALIDATION_LAYERS_SYNCHRONIZATION)
			enabled_validation_features_.emplace_back(VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT);
#endif
			validation_features_info.pEnabledValidationFeatures = enabled_validation_features_.data();
			validation_features_info.enabledValidationFeatureCount = static_cast<uint32_t>(enabled_validation_features_.size());
			validation_features_info.pNext = create_info.pNext;
			create_info.pNext = &validation_features_info;
		}
#endif

		auto result = vkCreateInstance(&create_info, allocator_, &handle_);
		if (result != VK_SUCCESS) {
			return result;
		}

		// Need to load volk for all the not-yet Vulkan calls
		volkLoadInstance(handle_);

		return result;
	}

	auto Instance::enumerate_devices() const -> Result<std::vector<Device>> {
		std::vector<Device> devices{};

		uint32_t physical_device_count{ 0u };
		if (auto result = vkEnumeratePhysicalDevices(handle_, &physical_device_count, nullptr); result != VK_SUCCESS) {
			return std::unexpected(result);
		}

		std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
		if (auto result = vkEnumeratePhysicalDevices(handle_, &physical_device_count, physical_devices.data()); result != VK_SUCCESS) {
			return std::unexpected(result);
		}

		for (auto& physical_device : physical_devices) {
			devices.emplace_back(*this, physical_device);
		}

		return devices;
	}

	auto Instance::is_extension_enabled(const char* extension_name) const noexcept -> bool {
		return std::find_if(enabled_extensions_.begin(), enabled_extensions_.end(),
			[extension_name](auto& instance_extension) {
				return std::strcmp(instance_extension, extension_name) == 0;
			}) != enabled_extensions_.end();
	}

	auto Instance::is_extension_supported(const char* extension_name) const noexcept -> bool {
		return std::find_if(available_extensions_.begin(), available_extensions_.end(), 
			[extension_name](auto& extension_props) {
				return std::strcmp(extension_props.extensionName, extension_name) == 0;
			}) != available_extensions_.end();
	}

	auto Instance::try_enable_extension(const char* extension_name) -> bool {
		for (const auto& extension_props : available_extensions_) {
			if (strcmp(extension_props.extensionName, extension_name) == 0) {
				if (is_extension_enabled(extension_name)) {
					VKW_LOGW("Extension \"{}\" is already enabled.", extension_name);
				}
				else {
					VKW_LOGD("Extension \"{}\" is supported and enabled.", extension_name);
					enabled_extensions_.push_back(extension_name);
				}

				return true;
			}
		}

		VKW_LOGE("Extension \"{}\" is not supported and can't be enabled.", extension_name);
		return false;
	}

	auto Instance::is_layer_enabled(const char* layer_name) const noexcept -> bool {
		return std::find_if(enabled_layers_.begin(), enabled_layers_.end(),
			[layer_name](auto& layer) {
				return std::strcmp(layer, layer_name) == 0;
			}) != enabled_layers_.end();
	}

	auto Instance::try_enable_layer(const char* layer_name) -> bool {
		for (const auto& layer_props : available_layers_) {
			if (strcmp(layer_props.layerName, layer_name) == 0) {
				if (is_layer_enabled(layer_name)) {
					VKW_LOGW("Layer \"{}\" is already enabled.", layer_name);
				}
				else {
					VKW_LOGD("Layer \"{}\" is supported and enabled.", layer_name);
					enabled_layers_.push_back(layer_name);
				}

				return true;
			}
		}

		VKW_LOGE("Layer \"{}\" is not supported and can't be enabled.", layer_name);
		return false;
	}

#undef VKW_SCOPE // Instance

#define VKW_SCOPE "Device"
	Device::Device(Instance const& instance, VkPhysicalDevice physical) {
		VKW_TRY_INIT_VOLK();

		physical_ = physical;
		allocator_ = instance.get_allocator();
	}

	Device::~Device() {
		VKW_TRY_DEINIT_VOLK();

		if (logical_) {
			vkDestroyDevice(logical_, allocator_);
		}
	}
#undef VKW_SCOPE // Device

#define VKW_SCOPE "DebugUtils"
	DebugUtils::~DebugUtils() {
		if (handle_) {
			vkDestroyDebugUtilsMessengerEXT(instance_, handle_, allocator_);
		}
	}

	auto DebugUtils::create_unique(Instance const& instance) -> Result<std::unique_ptr<DebugUtils>> {
		auto self = std::make_unique<DebugUtils>();
		if (auto result = self->_create(instance); result != VK_SUCCESS) {
			return std::unexpected(result);
		}
		return self;
	}

	auto DebugUtils::set_name(VkObjectType object_type, uint64_t object_handle, std::string_view name) const -> void {
		VkDebugUtilsObjectNameInfoEXT object_name_info{ VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
		object_name_info.objectType = object_type;
		object_name_info.objectHandle = object_handle;
		object_name_info.pObjectName = name.data();
		auto result = vkSetDebugUtilsObjectNameEXT(device_, &object_name_info);
		if (result != VK_SUCCESS) {
			VKW_LOGW("Failed to set name \"{}\" for object handle {:#x} with type \"{}\". Reason: {}.", 
				name, object_handle, to_string(object_type), to_string(result));
		}
	}

	auto DebugUtils::set_tag(VkObjectType object_type, uint64_t object_handle, uint64_t tag_name, const void* tag_data, size_t tag_data_size) const -> void {
		VkDebugUtilsObjectTagInfoEXT object_tag_info{ VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_TAG_INFO_EXT };
		object_tag_info.objectHandle = object_handle;
		object_tag_info.tagName = tag_name;
		object_tag_info.pTag = tag_data;
		object_tag_info.tagSize = tag_data_size;
		auto result = vkSetDebugUtilsObjectTagEXT(device_, &object_tag_info);
		if (result != VK_SUCCESS) {
			VKW_LOGW("Failed to set tag {:#x} with data {:#x} for object handle {:#x} with type \"{}\". Reason: {}.",
				tag_name, reinterpret_cast<uintptr_t>(tag_data), object_handle, to_string(object_type), to_string(result));
		}
	}

	auto DebugUtils::push_label(VkCommandBuffer command_buffer, std::string_view name, std::array<float, 4ull> color) const -> void {
		VkDebugUtilsLabelEXT debug_label{ VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT };
		debug_label.pLabelName = name.data();
		memcpy(debug_label.color, color.data(), sizeof(float) * color.size());
		vkCmdBeginDebugUtilsLabelEXT(command_buffer, &debug_label);
	}

	auto DebugUtils::pop_label(VkCommandBuffer command_buffer) const -> void {
		vkCmdEndDebugUtilsLabelEXT(command_buffer);
	}

	auto DebugUtils::insert_label(VkCommandBuffer command_buffer, std::string_view name, std::array<float, 4ull> color) const -> void {
		VkDebugUtilsLabelEXT debug_label{ VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT };
		debug_label.pLabelName = name.data();
		memcpy(debug_label.color, color.data(), sizeof(float) * color.size());
		vkCmdInsertDebugUtilsLabelEXT(command_buffer, &debug_label);
	}

	auto DebugUtils::_create(Instance const& instance) -> VkResult {
		instance_ = instance.get_handle();
		allocator_ = instance.get_allocator();

		VkDebugUtilsMessengerCreateInfoEXT create_info{ VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
		create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
		create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		create_info.pfnUserCallback = debug_utils_messenger_callback;

		return vkCreateDebugUtilsMessengerEXT(instance_, &create_info, allocator_, &handle_);
	}
#undef VKW_SCOPE // DebugUtils

#define VKW_SCOPE "DebugReport"
	DebugReport::~DebugReport() {
		if (handle_) {
			vkDestroyDebugReportCallbackEXT(instance_, handle_, allocator_);
		}
	}

	auto DebugReport::create_unique(Instance const& instance) -> Result< std::unique_ptr<DebugReport>> {
		auto self = std::make_unique<DebugReport>();
		if (auto result = self->_create(instance); result != VK_SUCCESS) {
			return std::unexpected(result);
		}
		return self;
	}

	auto DebugReport::set_name(VkObjectType object_type, uint64_t object_handle, std::string_view name) const -> void {
		VkDebugMarkerObjectNameInfoEXT object_name_info{ VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT };
		object_name_info.objectType = (VkDebugReportObjectTypeEXT)object_type;
		object_name_info.object = object_handle;
		object_name_info.pObjectName = name.data();
		auto result = vkDebugMarkerSetObjectNameEXT(device_, &object_name_info);
		if (result != VK_SUCCESS) {
			VKW_LOGW("Failed to set name \"{}\" for object handle {:#x} with type \"{}\". Reason: {}.",
				name, object_handle, to_string(object_type), to_string(result));
		}
	}

	auto DebugReport::set_tag(VkObjectType object_type, uint64_t object_handle, uint64_t tag_name, const void* tag_data, size_t tag_data_size) const -> void {
		VkDebugMarkerObjectTagInfoEXT object_tag_info{ VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_TAG_INFO_EXT };
		object_tag_info.objectType = (VkDebugReportObjectTypeEXT)object_type;
		object_tag_info.object = object_handle;
		object_tag_info.tagName = tag_name;
		object_tag_info.pTag = tag_data;
		object_tag_info.tagSize = tag_data_size;
		auto result = vkDebugMarkerSetObjectTagEXT(device_, &object_tag_info);
		if (result != VK_SUCCESS) {
			VKW_LOGW("Failed to set tag {:#x} with data {:#x} for object handle {:#x} with type \"{}\". Reason: {}.",
				tag_name, reinterpret_cast<uintptr_t>(tag_data), object_handle, to_string(object_type), to_string(result));
		}
	}

	auto DebugReport::push_label(VkCommandBuffer command_buffer, std::string_view name, std::array<float, 4ull> color) const -> void {
		VkDebugMarkerMarkerInfoEXT debug_marker{ VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT };
		debug_marker.pMarkerName = name.data();
		memcpy(debug_marker.color, color.data(), sizeof(float) * color.size());
		vkCmdDebugMarkerBeginEXT(command_buffer, &debug_marker);
	}

	auto DebugReport::pop_label(VkCommandBuffer command_buffer) const -> void {
		vkCmdDebugMarkerEndEXT(command_buffer);
	}

	auto DebugReport::insert_label(VkCommandBuffer command_buffer, std::string_view name, std::array<float, 4ull> color) const -> void {
		VkDebugMarkerMarkerInfoEXT debug_marker{ VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT };
		debug_marker.pMarkerName = name.data();
		memcpy(debug_marker.color, color.data(), sizeof(float) * color.size());
		vkCmdDebugMarkerInsertEXT(command_buffer, &debug_marker);
	}

	auto DebugReport::_create(Instance const& instance) -> VkResult {
		instance_ = instance.get_handle();
		allocator_ = instance.get_allocator();

		VkDebugReportCallbackCreateInfoEXT create_info{ VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT };
		create_info.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
		create_info.pfnCallback = debug_report_callback;

		return vkCreateDebugReportCallbackEXT(instance_, &create_info, allocator_, &handle_);
	}

#undef VKW_SCOPE // DebugReport

	// DebugNone
	auto DebugNone::set_name(VkObjectType object_type, uint64_t object_handle, std::string_view name) const -> void {
		(void)object_type;
		(void)object_handle;
		(void)name;
	}

	auto DebugNone::set_tag(VkObjectType object_type, uint64_t object_handle, uint64_t tag_name, const void* tag_data, size_t tag_data_size) const -> void {
		(void)object_type;
		(void)object_handle;
		(void)tag_name;
		(void)tag_data;
		(void)tag_data_size;
	}

	auto DebugNone::push_label(VkCommandBuffer command_buffer, std::string_view name, std::array<float, 4ull> color) const -> void {
		(void)command_buffer;
		(void)name;
		(void)color;
	}

	auto DebugNone::pop_label(VkCommandBuffer command_buffer) const -> void {
		(void)command_buffer;
	}

	auto DebugNone::insert_label(VkCommandBuffer command_buffer, std::string_view name, std::array<float, 4ull> color) const -> void {
		(void)command_buffer;
		(void)name;
		(void)color;
	}
}