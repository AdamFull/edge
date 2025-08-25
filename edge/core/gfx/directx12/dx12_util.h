#pragma once

#include <spdlog/spdlog.h>

#include <d3d12.h>
#include <Windows.h>

#define D3D12_CHECK_RESULT(result, error_text) \
    if(FAILED(result)) { \
        spdlog::error("[D3D12 Graphics Context]: {} Reason: {:#010x}", error_text, static_cast<uint32_t>(result)); \
        return false; \
    }

namespace edge::gfx {
	inline auto get_error_string(HRESULT hr) -> const char* {
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

	inline auto get_feature_level_string(D3D_FEATURE_LEVEL level) -> const char* {
		switch (level) {
		case D3D_FEATURE_LEVEL_12_2: return "12.2";
		case D3D_FEATURE_LEVEL_12_1: return "12.1";
		case D3D_FEATURE_LEVEL_12_0: return "12.0";
		case D3D_FEATURE_LEVEL_11_1: return "11.1";
		case D3D_FEATURE_LEVEL_11_0: return "11.0";
		default: return "Unknown";
		}
	}
}