#pragma once

#include "../gfx_context.h"

#include <vector>

#include <d3d12.h>
#include <dxgi1_6.h>
#if defined(ENGINE_DEBUG) || defined(D3D12_VALIDATION)
#include <dxgidebug.h>
#include <d3d12sdklayers.h>
#endif

#include <wrl/client.h>

#include <D3D12MemAlloc.h>

namespace edge::gfx {
	struct D3D12DeviceHandle {
		Microsoft::WRL::ComPtr<IDXGIAdapter4> physical;
		Microsoft::WRL::ComPtr<ID3D12Device> logical;
		Microsoft::WRL::ComPtr<ID3D12Device5> logical_rtx;
		Microsoft::WRL::ComPtr<ID3D12Device6> logical_mesh;

		GraphicsDeviceType device_type{};
		D3D_FEATURE_LEVEL max_supported_feature_level;
		DXGI_ADAPTER_DESC3 desc;
		bool supports_ray_tracing{ false };
		bool supports_mesh_shaders{ false };
		bool supports_variable_rate_shading{ false };
	};

	class DirectX12GraphicsContext final : public IGFXContext {
	public:
		~DirectX12GraphicsContext() override;

		static auto construct() -> std::unique_ptr<DirectX12GraphicsContext>;

		auto create(const GraphicsContextCreateInfo& create_info) -> bool override;

		auto set_debug_name(ID3D12Object* object, std::string_view name) const -> void;
		auto begin_event(ID3D12CommandList* command_list, std::string_view name, uint32_t color) const -> void;
		auto end_event(ID3D12CommandList* command_list) const -> void;
		auto set_marker(ID3D12CommandList* command_list, std::string_view name, uint32_t color) const -> void;
	private:
		HWND window_handle_;
#if defined(ENGINE_DEBUG) || defined(D3D12_VALIDATION)
		bool debug_layer_enabled_{ false };
		bool gpu_based_validation_enabled_{ false };
		Microsoft::WRL::ComPtr<ID3D12InfoQueue1> debug_validation_;
		DWORD debug_callback_cookie_;
#endif

		Microsoft::WRL::ComPtr<IDXGIFactory7> dxgi_factory_;
		std::vector<D3D12DeviceHandle> devices_;
		int32_t selected_adapter_index_{ -1 };

		Microsoft::WRL::ComPtr<D3D12MA::Allocator> d3d12ma_allocator_;
	};
}