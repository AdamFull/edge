#pragma once

#include <memory>

#include "gfx_structures.h"

namespace edge::gfx {
	class IGFXSemaphore {
	public:
		virtual ~IGFXSemaphore() = default;
	};

	class IGFXFence {
	public:
		virtual ~IGFXFence() = default;

		virtual auto wait(uint64_t timeout = std::numeric_limits<uint64_t>::max()) -> void = 0;
		virtual auto reset() -> void = 0;
	};

	class IGFXQueue {
	public:
		virtual ~IGFXQueue() = default;

		virtual auto submit(const SubmitInfo& submit_info, IGFXFence* fence) const -> void = 0;
		virtual auto present(const PresentInfo& present_info) const -> void = 0;
		virtual auto wait_idle() const -> void = 0;
	};

	class IGFXCommandList {
	public:
		virtual ~IGFXCommandList() = default;

		//virtual auto reset(CommandPoolResetMode reset_mode) -> void = 0;

		virtual auto begin() -> void = 0;
		virtual auto end() -> void = 0;

		// Dynamic states
		virtual auto set_viewport(float x, float y, float width, float height, float min_depth, float max_depth) const -> void = 0;
		virtual auto set_scissor(uint32_t x, uint32_t y, uint32_t width, uint32_t height) const -> void = 0;

		//virtual auto bind_pipeline(IPipeline* pipeline) const -> void = 0;
		//virtual auto bind_descriptors(const BindDescriptorsInfo& bind_info) const -> void = 0;
		//virtual auto push_constants(IShader* shader, IRootSignature* root_signature, std::span<const char> constant_data) const -> void = 0;

		//virtual auto bind_index_buffer(IBuffer* index_buffer, IndexType index_type, uint64_t offset) const -> void = 0;
		//virtual auto bind_vertex_buffers(std::span<IBuffer*> vertex_buffers, std::span<const uint32_t> strides, std::span<const uint64_t> offsets) const -> void = 0;

		virtual auto draw(uint32_t vertex_count, uint32_t first_vertex, uint32_t first_instance, uint32_t instance_count) const -> void = 0;
		virtual auto draw_indexed(uint32_t index_count, uint32_t first_index, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance) const -> void = 0;
		//virtual auto draw_indirect(const IndirectDrawCommand& draw_command) const -> void = 0;

		//virtual void write_timestamp(IQueryPool* query_pool, uint32_t pool_index, StageFlag stage_flag) = 0;

		virtual auto dispatch(uint32_t group_x, uint32_t group_y, uint32_t group_z) const -> void = 0;
		//virtual auto dispatch_indirect(IBuffer* argument_buffer, uint64_t argument_offset = 0ull) const -> void = 0;

		//virtual auto copy_buffer(const CopyBufferInfo& copy_info) const -> void = 0;
		//virtual auto copy_texture(const CopyTextureInfo& copy_info) const -> void = 0;

		//virtual auto resource_barrier(const ResourceBarrierInfo& barrier) const -> void = 0;

		//virtual auto reset_query(IQueryPool* query_pool, uint32_t first_query, uint32_t query_count) const -> void = 0;
		//virtual auto begin_query(IQueryPool* query_pool, uint32_t query) const -> void = 0;
		//virtual auto end_query(IQueryPool* query_pool, uint32_t query) const -> void = 0;

		//virtual auto begin_rendering(const RenderingInfo& rendering_info) const -> void = 0;
		virtual auto end_rendering() const -> void = 0;

		virtual auto begin_marker(std::string_view name, uint32_t color) const -> void = 0;
		virtual auto insert_marker(std::string_view name, uint32_t color) const -> void = 0;
		virtual auto end_marker() const -> void = 0;
	};

	class IGFXCommandPool {
	public:
		virtual ~IGFXCommandPool() = default;

		[[nodiscard]] virtual auto allocate(bool secondary = false) -> std::shared_ptr<IGFXCommandList> = 0;
		virtual auto reset() noexcept -> void = 0;
	};

	class IGFXContext {
	public:
		virtual ~IGFXContext() = default;

		virtual auto create(const GraphicsContextCreateInfo& create_info) -> bool = 0;


	};
}