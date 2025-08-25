#include "dx12_context.h"

#include "dx12_util.h"

namespace edge::gfx {
	D3D12Semaphore::~D3D12Semaphore() {

	}

	auto D3D12Semaphore::construct(const DirectX12GraphicsContext& ctx, uint64_t initial_value) -> std::unique_ptr<D3D12Semaphore> {
		auto self = std::make_unique<D3D12Semaphore>();
		self->_construct(ctx, initial_value);
		return self;
	}

	auto D3D12Semaphore::signal(uint64_t value) -> SyncResult {
		HRESULT hr = handle_->Signal(value);
		if (SUCCEEDED(hr)) {
			value_ = std::max(value_, value);
			return SyncResult::eSuccess;
		}
		return (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
			? SyncResult::eDeviceLost : SyncResult::eError;
	}

	auto D3D12Semaphore::wait(uint64_t value, std::chrono::nanoseconds timeout) -> SyncResult {
		if (handle_->GetCompletedValue() >= value) {
			return SyncResult::eSuccess;
		}

		HRESULT hr = handle_->SetEventOnCompletion(value, event_);
		if (FAILED(hr)) {
			return SyncResult::eError;
		}

		DWORD timeout_ms = (timeout == std::chrono::nanoseconds::max()) ? INFINITE :
			static_cast<DWORD>(std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count());

		DWORD waitResult = WaitForSingleObject(event_, timeout_ms);
		switch (waitResult) {
		case WAIT_OBJECT_0: return SyncResult::eSuccess;
		case WAIT_TIMEOUT: return SyncResult::eTimeout;
		default: return SyncResult::eError;
		}
	}

	auto D3D12Semaphore::is_completed(uint64_t value) const -> bool {
		return get_value() >= value;
	}

	auto D3D12Semaphore::get_value() const -> uint64_t {
		return handle_->GetCompletedValue();
	}

	auto D3D12Semaphore::_construct(const DirectX12GraphicsContext& ctx, uint64_t initial_value) -> bool {
		auto device = ctx.get_device();

		HRESULT hr = device->CreateFence(initial_value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&handle_));
		if (FAILED(hr)) {
			spdlog::error("[D3D12 Semaphore]: Failed to create semaphore. Reason: {}", get_error_string(hr));
			return false;
		}

		value_ = initial_value;

		event_ = CreateEvent(NULL, FALSE, FALSE, NULL);

		return true;
	}
}