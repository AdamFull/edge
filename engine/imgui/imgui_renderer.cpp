#include "imgui_renderer.h"

#include "../gfx_context.h"
#include "../gfx_renderer.h"

#include "imgui_vs.h"
#include "imgui_fs.h"
#include "imgui_shdr.h"

#include <imgui.h>

namespace edge::gfx {
	constexpr u64 k_initial_vertex_count = 2048ull;
	constexpr u64 k_initial_index_count = 4096ull;
	constexpr BufferFlags k_vertex_buffer_flags = BUFFER_FLAG_DYNAMIC | BUFFER_FLAG_DEVICE_ADDRESS | BUFFER_FLAG_VERTEX;
	constexpr BufferFlags k_index_buffer_flags = BUFFER_FLAG_DYNAMIC | BUFFER_FLAG_DEVICE_ADDRESS | BUFFER_FLAG_VERTEX;

	inline void update_buffer_resource(ImGuiRenderer* imgui_renderer, Handle handle, u64 size, BufferFlags flags) {
		BufferCreateInfo buffer_create_info = {
			.size = size,
			.flags = flags
		};

		Buffer buffer;
		if (!buffer_create(&buffer_create_info, &buffer)) {
			return;
		}
		renderer_update_buffer_resource(imgui_renderer->renderer, handle, buffer);
	}

	inline void update_imgui_texture(ImGuiRenderer* imgui_renderer, ImTextureData* tex) {
		if (!imgui_renderer || !tex) {
			return;
		}


	}

	ImGuiRenderer* imgui_renderer_create(const ImGuiRendererCreateInfo* create_info) {
		if (!create_info) {
			return nullptr;
		}

		ImGuiRenderer* imgui_renderer = allocate<ImGuiRenderer>(create_info->allocator);
		if (!imgui_renderer) {
			return nullptr;
		}

		if (!shader_module_create(imgui_vs, imgui_vs_size, &imgui_renderer->vertex_shader)) {
			imgui_renderer_destroy(imgui_renderer);
			return nullptr;
		}

		if (!shader_module_create(imgui_fs, imgui_fs_size, &imgui_renderer->fragment_shader)) {
			imgui_renderer_destroy(imgui_renderer);
			return nullptr;
		}

		imgui_renderer->allocator = create_info->allocator;
		imgui_renderer->renderer = create_info->renderer;

		imgui_renderer->vertex_buffer = renderer_add_resource(imgui_renderer->renderer);
		imgui_renderer->vertex_buffer_capacity = k_initial_vertex_count;
		update_buffer_resource(imgui_renderer, imgui_renderer->vertex_buffer, k_initial_vertex_count * sizeof(ImDrawVert), k_vertex_buffer_flags);

		imgui_renderer->index_buffer = renderer_add_resource(imgui_renderer->renderer);
		imgui_renderer->index_buffer_capacity = k_initial_index_count;
		update_buffer_resource(imgui_renderer, imgui_renderer->index_buffer, k_initial_index_count * sizeof(ImDrawIdx), k_index_buffer_flags);

		return imgui_renderer;
	}

	void imgui_renderer_destroy(ImGuiRenderer* imgui_renderer) {
		if (!imgui_renderer) {
			return;
		}

		shader_module_destroy(&imgui_renderer->fragment_shader);
		shader_module_destroy(&imgui_renderer->vertex_shader);

		renderer_free_resource(imgui_renderer->renderer, imgui_renderer->index_buffer);
		renderer_free_resource(imgui_renderer->renderer, imgui_renderer->vertex_buffer);

		const Allocator* allocator = imgui_renderer->allocator;
		deallocate(allocator, imgui_renderer);
	}

	void imgui_renderer_execute(ImGuiRenderer* imgui_renderer) {
		if (!imgui_renderer || !ImGui::GetCurrentContext()) {
			return;
		}

		ImDrawData* draw_data = ImGui::GetDrawData();
		if (draw_data && draw_data->Textures) {
			for (ImTextureData* tex : *draw_data->Textures) {
				update_imgui_texture(imgui_renderer, tex);
			}
		}
	}
}