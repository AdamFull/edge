#include "dx12_context.h"

#include "../../platform/platform.h"

#include <spdlog/spdlog.h>

#include <comdef.h>

#ifdef _WIN32
#include <Windows.h>
#endif

#define D3D12_CHECK_RESULT(result, error_text) \
    if(FAILED(result)) { \
        spdlog::error("[D3D12 Graphics Context]: {} Reason: {:#010x}", error_text, static_cast<uint32_t>(result)); \
        return false; \
    }

#define D3D12_CHECK_RESULT_VOID(result, error_text) \
    if(FAILED(result)) { \
        spdlog::error("[D3D12 Graphics Context]: {} Reason: {:#010x}", error_text, static_cast<uint32_t>(result)); \
        return; \
    }

#if defined(ENGINE_DEBUG) || defined(D3D12_VALIDATION)
#define USE_DEBUG_LAYER
#pragma comment(lib, "dxguid.lib")
#endif

#if defined(USE_DEBUG_LAYER) && defined(_WIN64)
#define USE_PIX
#include <pix3.h>
#endif

namespace edge::gfx {
	auto get_error_string(HRESULT hr) -> const char* {
		switch (hr) {
		case S_OK: return "S_OK";
		case E_FAIL: return "E_FAIL";
		case E_INVALIDARG: return "E_INVALIDARG";
		case E_OUTOFMEMORY: return "E_OUTOFMEMORY";
		case E_NOTIMPL: return "E_NOTIMPL";
		case DXGI_ERROR_INVALID_CALL: return "DXGI_ERROR_INVALID_CALL";
		case DXGI_ERROR_DEVICE_REMOVED: return "DXGI_ERROR_DEVICE_REMOVED";
		case DXGI_ERROR_DEVICE_HUNG: return "DXGI_ERROR_DEVICE_HUNG";
		case DXGI_ERROR_DEVICE_RESET: return "DXGI_ERROR_DEVICE_RESET";
		case DXGI_ERROR_DRIVER_INTERNAL_ERROR: return "DXGI_ERROR_DRIVER_INTERNAL_ERROR";
		case D3D12_ERROR_ADAPTER_NOT_FOUND: return "D3D12_ERROR_ADAPTER_NOT_FOUND";
		case D3D12_ERROR_DRIVER_VERSION_MISMATCH: return "D3D12_ERROR_DRIVER_VERSION_MISMATCH";
		default: return "Unknown Error";
		}
	}

	auto get_feature_level_string(D3D_FEATURE_LEVEL level) -> const char* {
		switch (level) {
		case D3D_FEATURE_LEVEL_12_2: return "12.2";
		case D3D_FEATURE_LEVEL_12_1: return "12.1";
		case D3D_FEATURE_LEVEL_12_0: return "12.0";
		case D3D_FEATURE_LEVEL_11_1: return "11.1";
		case D3D_FEATURE_LEVEL_11_0: return "11.0";
		default: return "Unknown";
		}
	}

	auto get_adapter_type_string(const DXGI_ADAPTER_DESC3& desc) -> const char* {
		if (desc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE) {
			return "Software";
		}

		// Check for integrated vs discrete based on dedicated video memory
		if (desc.DedicatedVideoMemory > 512 * 1024 * 1024) { // > 512MB
			return "Discrete";
		}
		else {
			return "Integrated";
		}
	}

	auto string_to_wstring(std::string_view str) -> std::wstring {
		if (str.empty()) return {};

		int size = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.length()), nullptr, 0);
		std::wstring result(size, 0);
		MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.length()), result.data(), size);
		return result;
	}

#ifdef USE_DEBUG_LAYER
	void debug_message_callback(D3D12_MESSAGE_CATEGORY category, D3D12_MESSAGE_SEVERITY severity,
		D3D12_MESSAGE_ID id, LPCSTR description, void* context) {
		// Filter out some common non-critical messages
		if (id == D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE ||
			id == D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE ||
			id == D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE ||
			id == D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE) {
			return;
		}

		const char* category_str = "Unknown";
		switch (category) {
		case D3D12_MESSAGE_CATEGORY_APPLICATION_DEFINED: category_str = "Application"; break;
		case D3D12_MESSAGE_CATEGORY_MISCELLANEOUS: category_str = "Miscellaneous"; break;
		case D3D12_MESSAGE_CATEGORY_INITIALIZATION: category_str = "Initialization"; break;
		case D3D12_MESSAGE_CATEGORY_CLEANUP: category_str = "Cleanup"; break;
		case D3D12_MESSAGE_CATEGORY_COMPILATION: category_str = "Compilation"; break;
		case D3D12_MESSAGE_CATEGORY_STATE_CREATION: category_str = "State Creation"; break;
		case D3D12_MESSAGE_CATEGORY_STATE_SETTING: category_str = "State Setting"; break;
		case D3D12_MESSAGE_CATEGORY_STATE_GETTING: category_str = "State Getting"; break;
		case D3D12_MESSAGE_CATEGORY_RESOURCE_MANIPULATION: category_str = "Resource Manipulation"; break;
		case D3D12_MESSAGE_CATEGORY_EXECUTION: category_str = "Execution"; break;
		case D3D12_MESSAGE_CATEGORY_SHADER: category_str = "Shader"; break;
		}

		switch (severity) {
		case D3D12_MESSAGE_SEVERITY_CORRUPTION:
			spdlog::critical("[D3D12] {}: {}", category_str, description);
			break;
		case D3D12_MESSAGE_SEVERITY_ERROR:
			spdlog::error("[D3D12] {}: {}", category_str, description);
			break;
		case D3D12_MESSAGE_SEVERITY_WARNING:
			spdlog::warn("[D3D12] {}: {}", category_str, description);
			break;
		case D3D12_MESSAGE_SEVERITY_INFO:
			spdlog::info("[D3D12] {}: {}", category_str, description);
			break;
		case D3D12_MESSAGE_SEVERITY_MESSAGE:
			spdlog::debug("[D3D12] {}: {}", category_str, description);
			break;
		}
	}
#endif

	DirectX12GraphicsContext::~DirectX12GraphicsContext() {
		if (debug_layer_enabled_) {
			debug_validation_->UnregisterMessageCallback(debug_callback_cookie_);
		}
	}

	auto DirectX12GraphicsContext::construct() -> std::unique_ptr<DirectX12GraphicsContext> {
		auto self = std::make_unique<DirectX12GraphicsContext>();
		return self;
	}

	auto DirectX12GraphicsContext::create(const GraphicsContextCreateInfo& create_info) -> bool {
		window_handle_ = static_cast<HWND>(create_info.window->get_native_handle());
		if (!window_handle_) {
			spdlog::error("[D3D12 Graphics Context]: Invalid window handle");
			return false;
		}

#ifdef USE_DEBUG_LAYER
		Microsoft::WRL::ComPtr<ID3D12Debug> debug_controller;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller)))) {
			debug_controller->EnableDebugLayer();
			debug_layer_enabled_ = true;

			// TODO: move this constant somewhere
			const bool gpu_based_validation = true;
			Microsoft::WRL::ComPtr<ID3D12Debug1> debug_controller1;
			if (gpu_based_validation && SUCCEEDED(debug_controller->QueryInterface(IID_PPV_ARGS(&debug_controller1)))) {
				debug_controller1->SetEnableGPUBasedValidation(gpu_based_validation);
				gpu_based_validation_enabled_ = true;
			}
		}
#endif

		UINT factory_flags = 0;
#ifdef USE_DEBUG_LAYER
		if (debug_layer_enabled_) {
			factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
		}
#endif

		D3D12_CHECK_RESULT(CreateDXGIFactory2(factory_flags, IID_PPV_ARGS(&dxgi_factory_)), "Failed to create DXGI factory");

		D3D_FEATURE_LEVEL device_feature_levels[] = {
			D3D_FEATURE_LEVEL_12_2,
			D3D_FEATURE_LEVEL_12_1,
			D3D_FEATURE_LEVEL_12_0,
			D3D_FEATURE_LEVEL_11_1,
			D3D_FEATURE_LEVEL_11_0,
		};

		// Enumerate devices
		Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
		for (UINT adapter_index = 0; SUCCEEDED(dxgi_factory_->EnumAdapterByGpuPreference(adapter_index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter))); ++adapter_index) {
			D3D12DeviceHandle new_device{};

			if (FAILED(adapter->QueryInterface(IID_PPV_ARGS(&new_device.physical)))) {
				continue;
			}

			new_device.physical->GetDesc3(&new_device.desc);

			for (auto feature_level : device_feature_levels) {
				if (SUCCEEDED(D3D12CreateDevice(new_device.physical.Get(), feature_level, IID_PPV_ARGS(&new_device.logical)))) {
					new_device.max_supported_feature_level = feature_level;
					break;
				}
			}

			if (!new_device.logical) {
				continue;
			}

			// Ignore dublicates
			if (auto found = std::find_if(devices_.begin(), devices_.end(), [&new_device](const D3D12DeviceHandle& h) {
				return h.desc.VendorId == new_device.desc.VendorId && 
					h.desc.DeviceId == new_device.desc.DeviceId && 
					h.desc.DedicatedVideoMemory == new_device.desc.DedicatedVideoMemory;
				}); found != devices_.end()) {
				continue;
			}

			D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
			new_device.logical->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5));
			D3D12_FEATURE_DATA_D3D12_OPTIONS6 options6 = {};
			new_device.logical->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS6, &options6, sizeof(options6));
			D3D12_FEATURE_DATA_D3D12_OPTIONS7 options7 = {};
			new_device.logical->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &options7, sizeof(options7));
			D3D12_FEATURE_DATA_ARCHITECTURE data_architecture = {};
			new_device.logical->CheckFeatureSupport(D3D12_FEATURE_ARCHITECTURE, &data_architecture, sizeof(data_architecture));

			new_device.supports_ray_tracing = options5.RaytracingTier >= D3D12_RAYTRACING_TIER_1_0;
			new_device.supports_variable_rate_shading = options6.VariableShadingRateTier >= D3D12_VARIABLE_SHADING_RATE_TIER_1;
			new_device.supports_mesh_shaders = options7.MeshShaderTier >= D3D12_MESH_SHADER_TIER_1;

			// Detect device type
			if (new_device.desc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE) {
				new_device.device_type = GraphicsDeviceType::eSoftware;
			}
			else if (data_architecture.UMA || data_architecture.TileBasedRenderer) {
				new_device.device_type = GraphicsDeviceType::eIntegrated;
			}
			else {
				new_device.device_type = GraphicsDeviceType::eDiscrete;
			}

			std::string adapter_name;
			int size = WideCharToMultiByte(CP_UTF8, 0, new_device.desc.Description, -1, nullptr, 0, nullptr, nullptr);
			if (size > 0) {
				adapter_name.resize(size - 1);
				WideCharToMultiByte(CP_UTF8, 0, new_device.desc.Description, -1, adapter_name.data(), size, nullptr, nullptr);
			}

			spdlog::info("  [{}] {} (Feature Level: {}, Type: {}, VRAM: {} MB, RT: {}, MS: {}, VRS: {})",
				adapter_index, adapter_name,
				get_feature_level_string(D3D_FEATURE_LEVEL_12_0),
				get_adapter_type_string(new_device.desc),
				new_device.desc.DedicatedVideoMemory / (1024 * 1024),
				new_device.supports_ray_tracing ? "Yes" : "No",
				new_device.supports_mesh_shaders ? "Yes" : "No",
				new_device.supports_variable_rate_shading ? "Yes" : "No");

			devices_.push_back(std::move(new_device));
		}

		if (devices_.empty()) {
			spdlog::error("[D3D12 Graphics Context]: No suitable D3D12 adapters found");
			return false;
		}

		int32_t fallback_adapter_index = -1;

		for (int32_t device_index = 0; device_index < static_cast<int32_t>(devices_.size()); ++device_index) {
			auto& device = devices_[device_index];

			bool requested_features_supported{ true };
			// Check for requested features support
			if (create_info.require_features.ray_tracing && !device.supports_ray_tracing) {
				spdlog::warn("[D3D12 Graphics Context]: Adapter {} doesn't support ray tracing", device_index);
				requested_features_supported = false;
			}

			if (create_info.require_features.mesh_shading && !device.supports_mesh_shaders) {
				spdlog::warn("[D3D12 Graphics Context]: Adapter {} doesn't support mesh shaders", device_index);
				requested_features_supported = false;
			}

			// Minimal feature level is 12
			if (device.max_supported_feature_level < D3D_FEATURE_LEVEL_12_0) {
				continue;
			}

			if (!requested_features_supported) {
				continue;
			}

			if (device.device_type != create_info.physical_device_type) {
				fallback_adapter_index = device_index;
				continue;
			}

			selected_adapter_index_ = device_index;
			break;
		}

		if (selected_adapter_index_ == -1) {
			if (fallback_adapter_index == -1) {
				spdlog::error("[D3D12 Graphics Context]: No suitable adapter found");
				return false;
			}
			selected_adapter_index_ = fallback_adapter_index;
		}

		auto& device = devices_[selected_adapter_index_];

		std::string adapter_name;
		int size = WideCharToMultiByte(CP_UTF8, 0, device.desc.Description, -1, nullptr, 0, nullptr, nullptr);
		if (size > 0) {
			adapter_name.resize(size - 1);
			WideCharToMultiByte(CP_UTF8, 0, device.desc.Description, -1, adapter_name.data(), size, nullptr, nullptr);
		}

		spdlog::info("[D3D12 Graphics Context]: Selected adapter [{}]: {}", selected_adapter_index_, adapter_name);

		if (device.supports_ray_tracing) {
			device.logical->QueryInterface(IID_PPV_ARGS(&device.logical_rtx));
		}
		
		if (device.supports_mesh_shaders) {
			device.logical->QueryInterface(IID_PPV_ARGS(&device.logical_mesh));
		}

#ifdef USE_DEBUG_LAYER
		if (debug_layer_enabled_ && SUCCEEDED(device.logical->QueryInterface(IID_PPV_ARGS(&debug_validation_)))) {
			debug_validation_->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
			debug_validation_->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
			debug_validation_->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, false);
			debug_validation_->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_INFO, false);
			debug_validation_->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_MESSAGE, false);

			// Filter out some common non-critical messages
			D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_INFO };
			D3D12_MESSAGE_ID denied_ids[] = {
				D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
				D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE,
				D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
				D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,
			};

			D3D12_INFO_QUEUE_FILTER filter = {};
			filter.DenyList.NumSeverities = _countof(severities);
			filter.DenyList.pSeverityList = severities;
			filter.DenyList.NumIDs = _countof(denied_ids);
			filter.DenyList.pIDList = denied_ids;

			//debug_validation_->PushStorageFilter(&filter);
			debug_validation_->AddStorageFilterEntries(&filter);

			if (FAILED(debug_validation_->RegisterMessageCallback(debug_message_callback, D3D12_MESSAGE_CALLBACK_FLAG_NONE, this, &debug_callback_cookie_))) {
				spdlog::warn("[D3D12 Graphics Context]: Failed to create debug messager.");
			}
		}
#endif

		// Create memory allocator
		D3D12MA::ALLOCATOR_DESC allocator_desc = {};
		allocator_desc.pDevice = device.logical.Get();
		allocator_desc.pAdapter = device.physical.Get();
		allocator_desc.Flags = D3D12MA::ALLOCATOR_FLAG_DEFAULT_POOLS_NOT_ZEROED;

		D3D12_CHECK_RESULT(D3D12MA::CreateAllocator(&allocator_desc, &d3d12ma_allocator_), "Failed to create D3D12MA allocator");

		return true;
	}

	auto DirectX12GraphicsContext::set_debug_name(ID3D12Object* object, std::string_view name) const -> void {
		if (!object) return;

		auto wide_name = string_to_wstring(name);
		object->SetName(wide_name.c_str());
	}

	auto DirectX12GraphicsContext::begin_event(ID3D12CommandList* command_list, std::string_view name, uint32_t color) const -> void {
		if (!command_list) return;

#ifdef USE_DEBUG_LAYER
		std::wstring wide_name = string_to_wstring(name);
		//PIXBeginEvent(command_list, color, wide_name.c_str());
#endif
	}

	auto DirectX12GraphicsContext::end_event(ID3D12CommandList* command_list) const -> void {
		if (!command_list) return;

#ifdef USE_DEBUG_LAYER
		//PIXEndEvent(command_list);
#endif
	}

	auto DirectX12GraphicsContext::set_marker(ID3D12CommandList* command_list, std::string_view name, uint32_t color) const -> void {
		if (!command_list) return;

#ifdef USE_DEBUG_LAYER
		std::wstring wide_name = string_to_wstring(name);
		//PIXSetMarker(command_list, color, wide_name.c_str());
#endif
	}
}