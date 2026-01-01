#ifndef IMGUI_LAYER_H
#define IMGUI_LAYER_H

#include <stddef.hpp>

namespace edge {
	struct Allocator;
	struct EventDispatcher;
	struct PlatformContext;
	struct Window;

	struct ImGuiLayerInitInfo {
		const Allocator* alloc = nullptr;
		EventDispatcher* event_dispatcher = nullptr;

		PlatformContext* platform_context = nullptr;
		Window* window = nullptr;
	};

	struct ImGuiLayer {
		EventDispatcher* event_dispatcher = nullptr;
		u64 listener_id = 0;

		bool create(ImGuiLayerInitInfo init_info) noexcept;
		void destroy(NotNull<const Allocator*> alloc) noexcept;
		void update(f32 dt) noexcept;
	};

	ImGuiLayer* imgui_layer_create(ImGuiLayerInitInfo init_info);
	void imgui_layer_destroy(ImGuiLayer* layer);
	void imgui_layer_update(NotNull<ImGuiLayer*> layer, f32 delta_time);
}

#endif