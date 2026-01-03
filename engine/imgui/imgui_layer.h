#ifndef IMGUI_LAYER_H
#define IMGUI_LAYER_H

#include <stddef.hpp>

namespace edge {
	struct Allocator;
	struct EventDispatcher;
	struct IRuntime;
	struct InputSystem;

	struct ImGuiLayerInitInfo {
		const Allocator* alloc = nullptr;
		IRuntime* runtime = nullptr;
		InputSystem* input_system = nullptr;
	};

	struct ImGuiLayer {
		IRuntime* runtime = nullptr;
		InputSystem* input_system = nullptr;

		bool create(ImGuiLayerInitInfo init_info) noexcept;
		void destroy(NotNull<const Allocator*> alloc) noexcept;
		void update(f32 dt) noexcept;
	};

	ImGuiLayer* imgui_layer_create(ImGuiLayerInitInfo init_info);
	void imgui_layer_destroy(ImGuiLayer* layer);
	void imgui_layer_update(NotNull<ImGuiLayer*> layer, f32 delta_time);
}

#endif