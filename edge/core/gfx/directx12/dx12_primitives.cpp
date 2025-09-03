#include "dx12_context.h"

#include "dx12_util.h"

namespace edge::gfx {
	// Semaphore
	D3D12Semaphore::~D3D12Semaphore() {
		if (event_) {
			CloseHandle(event_);
		}
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

		DWORD wait_result = WaitForSingleObject(event_, timeout_ms);
		switch (wait_result) {
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

	//Queue
	D3D12Queue::~D3D12Queue() {
		wait_idle();

		if (fence_event_) {
			CloseHandle(fence_event_);
		}
	}

	auto D3D12Queue::construct(const DirectX12GraphicsContext& ctx, D3D12_COMMAND_LIST_TYPE list_type) -> std::unique_ptr<D3D12Queue> {
		auto self = std::make_unique<D3D12Queue>();
		self->_construct(ctx, list_type);
		return self;
	}

	auto D3D12Queue::create_command_allocator() const -> std::shared_ptr<IGFXCommandAllocator> {
		return D3D12CommandAllocator::construct(*this);
	}

	auto D3D12Queue::submit(const SubmitQueueInfo& submit_info) -> void {
		std::array<ID3D12CommandList*, 16ull> command_lists{};
		uint32_t command_list_count{ static_cast<uint32_t>(submit_info.command_lists.size()) };

		// At first wait semahpores
		for (auto& semaphore : submit_info.wait_semaphores) {
			auto d3d_semaphore = std::static_pointer_cast<D3D12Semaphore>(semaphore.semaphore);
			if (HRESULT hr = handle_->Wait(d3d_semaphore->get_handle().Get(), semaphore.value); FAILED(hr)) {
				spdlog::error("[D3D12 Queue]: Failed to wait semaphore. Reason: {}.", get_error_string(hr));
			}
		}

		// Prepare command lists for execute
		for (int32_t i = 0; i < static_cast<int32_t>(command_list_count); ++i) {
			auto d3d_cmd_list = std::static_pointer_cast<D3D12CommandList>(submit_info.command_lists[i]);
			command_lists[i] = d3d_cmd_list->get_handle().Get();
		}

		handle_->ExecuteCommandLists(command_list_count, command_lists.data());

		// Signal semaphores
		for (auto& semaphore : submit_info.signal_semaphores) {
			auto d3d_semaphore = std::static_pointer_cast<D3D12Semaphore>(semaphore.semaphore);
			if (HRESULT hr = handle_->Signal(d3d_semaphore->get_handle().Get(), semaphore.value); FAILED(hr)) {
				spdlog::error("[D3D12 Queue]: Failed to signal semaphore. Reason: {}.", get_error_string(hr));
				continue;
			}
			d3d_semaphore->set_value(semaphore.value);
		}
	}

	auto D3D12Queue::wait_idle() -> SyncResult {
		constexpr uint64_t fence_value{ 1ull };
		HRESULT hr = handle_->Signal(fence_.Get(), fence_value);
		if (FAILED(hr)) {
			spdlog::error("[D3D12 Queue]: Failed to signal wait idle semaphore.");
			return (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
				? SyncResult::eDeviceLost : SyncResult::eError;
		}

		if (fence_->GetCompletedValue() >= fence_value) {
			return SyncResult::eSuccess;
		}

		hr = fence_->SetEventOnCompletion(fence_value, fence_event_);
		if (FAILED(hr)) {
			return SyncResult::eError;
		}

		DWORD waitResult = WaitForSingleObject(fence_event_, INFINITE);
		switch (waitResult) {
		case WAIT_OBJECT_0: return SyncResult::eSuccess;
		case WAIT_TIMEOUT: return SyncResult::eTimeout;
		default: return SyncResult::eError;
		}
	}

	auto D3D12Queue::_construct(const DirectX12GraphicsContext& ctx, D3D12_COMMAND_LIST_TYPE list_type) -> bool {
		D3D12_COMMAND_QUEUE_DESC queue_desc{};
		queue_desc.Type = list_type;
		queue_desc.Priority = 0.5f;

		auto device = ctx.get_device();
		D3D12_CHECK_RESULT(device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&handle_)),
			"Failed to create command queue.");

		// Create fence for wait idle
		D3D12_CHECK_RESULT(device->CreateFence(0ull, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)),
			"Failed to create fence object for queue wait idle");

		fence_event_ = CreateEvent(NULL, FALSE, FALSE, NULL);

		return true;
	}

	// Command allocator
	D3D12CommandAllocator::~D3D12CommandAllocator() {

	}

	auto D3D12CommandAllocator::construct(const D3D12Queue& queue) -> std::unique_ptr<D3D12CommandAllocator> {
		auto self = std::make_unique<D3D12CommandAllocator>();
		self->_construct(queue);
		return self;
	}

	auto D3D12CommandAllocator::allocate_command_list() const -> std::shared_ptr<IGFXCommandList> {
		return D3D12CommandList::construct(*this);
	}

	auto D3D12CommandAllocator::reset() -> void {
		assert(false && "NOT IMPLEMENTED");
	}
	
	auto D3D12CommandAllocator::_construct(const D3D12Queue& queue) -> bool {
		queue_ = queue.get_handle();

		Microsoft::WRL::ComPtr<ID3D12Device> device;
		D3D12_CHECK_RESULT(queue_->GetDevice(IID_PPV_ARGS(&device)),
			"Failed to get device.");

		D3D12_COMMAND_QUEUE_DESC queue_desc = queue_->GetDesc();
		D3D12_CHECK_RESULT(device->CreateCommandAllocator(queue_desc.Type, IID_PPV_ARGS(&handle_)),
			"Failed to create command allocator.");

		return true;
	}

	// Command list

	D3D12CommandList::~D3D12CommandList() {

	}

	auto D3D12CommandList::construct(const D3D12CommandAllocator& cmd_alloc) -> std::unique_ptr<D3D12CommandList> {
		auto self = std::make_unique<D3D12CommandList>();
		self->_construct(cmd_alloc);
		return self;
	}

	auto D3D12CommandList::reset() -> void {
		assert(false && "NOT IMPLEMENTED");
	}

	auto D3D12CommandList::begin() -> bool {
		D3D12_CHECK_RESULT(handle_->Reset(allocator_.Get(), nullptr),
			"Failed to reset command list.");

		return true;
	}

	auto D3D12CommandList::end() -> bool {
		D3D12_CHECK_RESULT(handle_->Close(),
			"Failed to close command list.");

		return true;
	}

	auto D3D12CommandList::set_viewport(float x, float y, float width, float height, float min_depth, float max_depth) const -> void {
		D3D12_VIEWPORT viewport{ x, y, width, height, min_depth, max_depth };
		handle_->RSSetViewports(1, &viewport);
	}

	auto D3D12CommandList::set_scissor(uint32_t x, uint32_t y, uint32_t width, uint32_t height) const -> void {
		D3D12_RECT scissor{ x, y, x + width, y + height };
		handle_->RSSetScissorRects(1, &scissor);
	}

	auto D3D12CommandList::draw(uint32_t vertex_count, uint32_t first_vertex, uint32_t first_instance, uint32_t instance_count) const -> void {
		handle_->DrawInstanced(vertex_count, instance_count, first_vertex, first_instance);
	}

	auto D3D12CommandList::draw_indexed(uint32_t index_count, uint32_t first_index, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance) const -> void {
		handle_->DrawIndexedInstanced(index_count, instance_count, first_index, first_vertex, first_instance);
	}

	auto D3D12CommandList::dispatch(uint32_t group_x, uint32_t group_y, uint32_t group_z) const -> void {
		handle_->Dispatch(group_x, group_y, group_z);
	}

	auto D3D12CommandList::begin_marker(std::string_view name, uint32_t color) const -> void {
		assert(false && "NOT IMPLEMENTED");
	}

	auto D3D12CommandList::insert_marker(std::string_view name, uint32_t color) const -> void {
		assert(false && "NOT IMPLEMENTED");
	}

	auto D3D12CommandList::end_marker() const -> void {
		assert(false && "NOT IMPLEMENTED");
	}
	
	auto D3D12CommandList::_construct(const D3D12CommandAllocator& cmd_alloc) -> bool {
		allocator_ = cmd_alloc.get_handle();
		auto command_queue = cmd_alloc.get_queue();

		Microsoft::WRL::ComPtr<ID3D12Device> device;
		D3D12_CHECK_RESULT(allocator_->GetDevice(IID_PPV_ARGS(&device)),
			"Failed to get device.");

		D3D12_COMMAND_QUEUE_DESC queue_desc = command_queue->GetDesc();

		D3D12_CHECK_RESULT(device->CreateCommandList(0, queue_desc.Type, allocator_.Get(), nullptr, IID_PPV_ARGS(&handle_)),
			"Failed to allocate command list.");

		return true;
	}
}