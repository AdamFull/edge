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

		static auto construct(const DirectX12GraphicsContext& ctx, D3D12_COMMAND_LIST_TYPE list_type) -> std::unique_ptr<D3D12Queue>;

		auto create_command_allocator() const -> std::shared_ptr<IGFXCommandAllocator> override;

		auto submit(const SubmitQueueInfo& submit_info) -> void override;
		auto wait_idle() -> SyncResult override;

		auto get_handle() const -> Microsoft::WRL::ComPtr<ID3D12CommandQueue> {
			return handle_;
		}
	private:
		auto _construct(const DirectX12GraphicsContext& ctx, D3D12_COMMAND_LIST_TYPE list_type) -> bool;

		Microsoft::WRL::ComPtr<ID3D12CommandQueue> handle_;
		Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
		HANDLE fence_event_;
	};

	class D3D12CommandAllocator final : public IGFXCommandAllocator {
	public:
		~D3D12CommandAllocator() override;

		static auto construct(const D3D12Queue& queue) -> std::unique_ptr<D3D12CommandAllocator>;

		auto allocate_command_list() const -> std::shared_ptr<IGFXCommandList> override;
		auto reset() -> void override;

		auto get_handle() const -> Microsoft::WRL::ComPtr<ID3D12CommandAllocator> {
			return handle_;
		}

		auto get_queue() const -> Microsoft::WRL::ComPtr<ID3D12CommandQueue> {
			return queue_;
		}
	private:
		auto _construct(const D3D12Queue& queue) -> bool;

		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> handle_;
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue_;
	};

	class D3D12CommandList final : public IGFXCommandList {
	public:
		~D3D12CommandList() override;

		static auto construct(const D3D12CommandAllocator& cmd_alloc) -> std::unique_ptr<D3D12CommandList>;

		auto reset() -> void override;

		auto begin() -> bool override;
		auto end() -> bool override;

		auto set_viewport(float x, float y, float width, float height, float min_depth, float max_depth) const -> void override;
		auto set_scissor(uint32_t x, uint32_t y, uint32_t width, uint32_t height) const -> void override;

		 auto draw(uint32_t vertex_count, uint32_t first_vertex, uint32_t first_instance, uint32_t instance_count) const -> void override;
		auto draw_indexed(uint32_t index_count, uint32_t first_index, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance) const -> void override;

		auto dispatch(uint32_t group_x, uint32_t group_y, uint32_t group_z) const -> void override;

		auto begin_marker(std::string_view name, uint32_t color) const -> void override;
		auto insert_marker(std::string_view name, uint32_t color) const -> void override;
		auto end_marker() const -> void override;

		auto get_handle() const -> Microsoft::WRL::ComPtr<ID3D12CommandList> {
			return handle_;
		}
	private:
		auto _construct(const D3D12CommandAllocator& cmd_alloc) -> bool;

		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList1> handle_;
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator_;
	};

	class DirectX12GraphicsContext final : public IGFXContext {
	public:
		~DirectX12GraphicsContext() override;

		static auto construct() -> std::unique_ptr<DirectX12GraphicsContext>;

		auto create(const GraphicsContextCreateInfo& create_info) -> bool override;

		auto create_queue(QueueType queue_type) -> std::shared_ptr<IGFXQueue> override;
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