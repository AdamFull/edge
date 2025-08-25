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
	class DirectX12GraphicsContext;

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

	class D3D12Semaphore final : public IGFXSemaphore {
	public:
		~D3D12Semaphore() override;

		static auto construct(const DirectX12GraphicsContext& ctx, uint64_t initial_value) -> std::unique_ptr<D3D12Semaphore>;

		auto signal(uint64_t value) -> SyncResult override;
		auto wait(uint64_t value, std::chrono::nanoseconds timeout = std::chrono::nanoseconds::max()) -> SyncResult override;

		auto is_completed(uint64_t value) const -> bool override;
		auto get_value() const -> uint64_t override;

		auto set_value(uint64_t value) -> void {
			value_ = std::max(value_, value);
		}

		auto get_handle() const -> Microsoft::WRL::ComPtr<ID3D12Fence> {
			return handle_;
		}
	private:
		auto _construct(const DirectX12GraphicsContext& ctx, uint64_t initial_value) -> bool;

		Microsoft::WRL::ComPtr<ID3D12Fence> handle_;
		HANDLE event_;
		uint64_t value_{ 0ull };
	};

	class D3D12Queue final : public IGFXQueue {
	public:
		~D3D12Queue() override;

		static auto construct(const DirectX12GraphicsContext& ctx) -> std::unique_ptr<D3D12Queue>;

		auto submit(const SignalQueueInfo& submit_info) -> SyncResult override;
		auto wait_idle() -> void override;

		//auto get_handle() const -> VkQueue {
		//	return handle_;
		//}
	private:
		auto _construct(const DirectX12GraphicsContext& ctx) -> bool;
	};

	class DirectX12GraphicsContext final : public IGFXContext {
	public:
		~DirectX12GraphicsContext() override;

		static auto construct() -> std::unique_ptr<DirectX12GraphicsContext>;

		auto create(const GraphicsContextCreateInfo& create_info) -> bool override;

		auto get_queue_count(QueueType queue_type) -> uint32_t override;
		auto get_queue(QueueType queue_type, uint32_t queue_index) -> std::expected<std::shared_ptr<IGFXQueue>, bool> override;

		auto create_semaphore(uint64_t value) const -> std::shared_ptr<IGFXSemaphore> override;

		auto get_device() const -> Microsoft::WRL::ComPtr<ID3D12Device>;

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