#ifndef EDGE_ENGINE_H
#define EDGE_ENGINE_H

#include "runtime/platform.h"
#include "gfx_interface.h"

namespace edge {
	struct Logger;
	struct Scheduler;
	struct EventDispatcher;

	namespace gfx {
		struct Queue;
		struct Renderer;
	}

	struct EngineContext {
		const Allocator* allocator;
		Logger* logger;
		Scheduler* sched;

		EventDispatcher* event_dispatcher;

		PlatformLayout* platform_layout;
		PlatformContext* platform_context;

		gfx::Queue main_queue;
		gfx::Renderer* renderer;
	};
}

#endif //EDGE_ENGINE_H