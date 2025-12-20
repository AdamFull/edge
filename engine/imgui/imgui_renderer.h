#ifndef IMGUI_RENDERER_H
#define IMGUI_RENDERER_H

#include "../gfx_interface.h"
#include <handle_pool.hpp>

struct ImDrawData;
struct ImTextureData;

namespace edge {
	struct Allocator;
}

namespace edge::gfx {
	struct Renderer;

	struct ImGuiRenderer {
		Renderer* renderer;

		ShaderModule vertex_shader;
		ShaderModule fragment_shader;
		Pipeline pipeline;

		Handle vertex_buffer;
		u64 vertex_buffer_capacity;

		Handle index_buffer;
		u64 index_buffer_capacity;

		void update_texture(NotNull<ImTextureData*> tex) noexcept;
		void update_geometry(NotNull<ImDrawData*> draw_data) noexcept;

		void execute() noexcept;
	};

	struct ImGuiRendererCreateInfo {
		Renderer* renderer;
	};

	ImGuiRenderer* imgui_renderer_create(ImGuiRendererCreateInfo create_info);
	void imgui_renderer_destroy(ImGuiRenderer* imgui_renderer);
}

#endif