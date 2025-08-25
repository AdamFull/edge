#pragma once

#include "gfx_enum.h"

#include <memory>

namespace edge::platform {
	class IPlatformWindow;
}

namespace edge::gfx {
	class IGFXContext;

	class IGFXSemaphore;
	class IGFXCommandList;
	class IGFXQueue;

	struct GraphicsContextCreateInfo {
		GraphicsDeviceType physical_device_type;
		platform::IPlatformWindow* window;

		struct RequireFeatures {
			bool mesh_shading{};
			bool ray_tracing{};
		} require_features{};
	};

	struct SemaphoreSubmitInfo {
		std::shared_ptr<IGFXSemaphore> semaphore;
		uint64_t value;
	};

	struct SignalQueueInfo {
		std::vector<SemaphoreSubmitInfo> wait_semaphores;
		std::vector<SemaphoreSubmitInfo> signal_semaphores;
		std::vector<std::shared_ptr<IGFXCommandList>> command_lists;
	};
}