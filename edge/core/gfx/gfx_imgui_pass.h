#pragma once

#include "gfx_shader_pass.h"

struct ImDrawData;
struct ImTextureData;

namespace edge::gfx {
	class Renderer;
	class ResourceUpdater;
	class ResourceUploader;

	class ImGuiPass final : public IShaderPass {
	public:
		~ImGuiPass() override = default;

		static auto create(Renderer& renderer, ResourceUpdater& updater, ResourceUploader& uploader, Pipeline const* pipeline) -> Owned<ImGuiPass>;

		auto execute(CommandBuffer const& cmd, float delta_time) -> void override;
	private:
		auto push_image_barrier(uint32_t resource_id, ResourceStateFlags required_state) -> void;
		auto update_imgui_texture(ImTextureData* tex) -> void;
		auto collect_external_resource_barriers(ImDrawData* draw_data) -> void;
		auto update_buffer_resource(uint32_t resource_id, vk::DeviceSize element_count, vk::DeviceSize element_size, BufferFlags usage) -> void;

		Renderer* renderer_{ nullptr };
		ResourceUpdater* updater_{ nullptr };
		ResourceUploader* uploader_{ nullptr };
		Pipeline const* pipeline_{ nullptr };

		uint32_t vertex_buffer_render_resource_id_{ ~0u };
		uint32_t index_buffer_render_resource_id_{ ~0u };

		static constexpr BufferFlags kVertexBufferFlags = kDynamicVertexBuffer;
		static constexpr BufferFlags kIndexBufferFlags = kDynamicIndexBuffer;

		static constexpr uint32_t kInitialVertexCount = 2048u;
		static constexpr uint32_t kInitialIndexCount = 4096u;
		uint32_t current_vertex_capacity_{ kInitialVertexCount };
		uint32_t current_index_capacity_{ kInitialIndexCount };

		mi::HashMap<uint32_t, uint32_t> pending_image_uploads_{};

		mi::Vector<vk::BufferMemoryBarrier2KHR> buffer_barriers_{};
		mi::Vector<vk::ImageMemoryBarrier2KHR> image_barriers_{};
	};
}