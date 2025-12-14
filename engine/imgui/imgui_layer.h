#ifndef IMGUI_LAYER_H
#define IMGUI_LAYER_H

#include <stddef.hpp>

namespace edge {
	struct Allocator;
	struct EventDispatcher;
	struct PlatformContext;

	struct ImGuiLayer {
		const Allocator* alocator;
		EventDispatcher* event_dispatcher;

		u64 listener_id;
	};

	struct ImGuiLayerInitInfo {
		const Allocator* alocator;
		EventDispatcher* event_dispatcher;

		PlatformContext* platform_context;
	};

	ImGuiLayer* imgui_layer_create(const ImGuiLayerInitInfo* init_info);
	void imgui_layer_destroy(ImGuiLayer* layer);
	void imgui_layer_update(ImGuiLayer* layer, f32 delta_time);
}

#endif