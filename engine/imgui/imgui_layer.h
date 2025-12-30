#ifndef IMGUI_LAYER_H
#define IMGUI_LAYER_H

#include <stddef.hpp>

namespace edge {
	struct Allocator;
	struct EventDispatcher;
	struct PlatformContext;
	struct Window;

	struct ImGuiLayer {
		const Allocator* alocator;
		EventDispatcher* event_dispatcher;

		u64 listener_id;
	};

	struct ImGuiLayerInitInfo {
		const Allocator* alocator;
		EventDispatcher* event_dispatcher;

		PlatformContext* platform_context;
		Window* window;
	};

	ImGuiLayer* imgui_layer_create(ImGuiLayerInitInfo init_info);
	void imgui_layer_destroy(ImGuiLayer* layer);
	void imgui_layer_update(NotNull<ImGuiLayer*> layer, f32 delta_time);
}

#endif