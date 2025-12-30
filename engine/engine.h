#ifndef EDGE_ENGINE_H
#define EDGE_ENGINE_H

#include "runtime/platform.h"
#include "gfx_context.h"

namespace edge {
	struct Logger;
	struct Scheduler;
	struct EventDispatcher;

	struct ImGuiLayer;

	namespace gfx {
		struct Queue;
		struct Renderer;
		struct Uploader;

		struct ImGuiRenderer;
	}

	struct EngineContext {
		const Allocator* allocator;
		Logger* logger;
		Scheduler* sched;

		EventDispatcher* event_dispatcher;

		PlatformLayout* platform_layout;
		PlatformContext* platform_context;
		Window* window;

		gfx::Queue main_queue;
		gfx::Queue copy_queue;

		gfx::Renderer* renderer;
		//gfx::Uploader* uploader;

		ImGuiLayer* imgui_layer;
		gfx::ImGuiRenderer* imgui_renderer;
	};
}

#endif //EDGE_ENGINE_H