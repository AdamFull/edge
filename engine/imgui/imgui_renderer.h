#ifndef IMGUI_RENDERER_H
#define IMGUI_RENDERER_H

#include "../gfx_interface.h"
#include <handle_pool.hpp>

namespace edge {
	struct Allocator;
}

namespace edge::gfx {
	struct Renderer;

	struct ImGuiRenderer {
		const Allocator* allocator;
		Renderer* renderer;

		ShaderModule vertex_shader;
		ShaderModule fragment_shader;
		Pipeline pipeline;

		Handle vertex_buffer;
		u64 vertex_buffer_capacity;

		Handle index_buffer;
		u64 index_buffer_capacity;
	};

	struct ImGuiRendererCreateInfo {
		const Allocator* allocator;
		Renderer* renderer;
	};

	ImGuiRenderer* imgui_renderer_create(ImGuiRendererCreateInfo create_info);
	void imgui_renderer_destroy(ImGuiRenderer* imgui_renderer);
	void imgui_renderer_execute(NotNull<ImGuiRenderer*> imgui_renderer);
}

#endif