#ifndef IMGUI_RENDERER_H
#define IMGUI_RENDERER_H

#include "../gfx_context.h"
#include <handle_pool.hpp>

struct ImDrawData;
struct ImTextureData;

namespace edge {
	struct Allocator;
}

namespace edge::gfx {
	struct Renderer;

	struct ImGuiRendererCreateInfo {
		const Allocator* alloc = nullptr;
		Renderer* renderer = nullptr;
	};

	struct ImGuiRenderer {
		Renderer* renderer = nullptr;

		ShaderModule vertex_shader = {};
		ShaderModule fragment_shader = {};
		Pipeline pipeline = {};

		Handle vertex_buffer = HANDLE_INVALID;
		u64 vertex_buffer_capacity = 0;
		bool vertex_need_to_grow = true;

		Handle index_buffer = HANDLE_INVALID;
		u64 index_buffer_capacity = 0;
		bool index_need_to_grow = true;

		bool create(ImGuiRendererCreateInfo create_info) noexcept;
		void destroy(NotNull<const Allocator*> alloc) noexcept;

		void execute(NotNull<const Allocator*> alloc) noexcept;
	private:
		void update_buffers(NotNull<const Allocator*> alloc) noexcept;
		void update_texture(NotNull<const Allocator*> alloc, NotNull<ImTextureData*> tex) noexcept;
		void update_geometry(NotNull<const Allocator*> alloc, NotNull<ImDrawData*> draw_data) noexcept;
	};
}

#endif