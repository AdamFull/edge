#pragma once

#include "gfx_enum.h"

namespace edge::platform {
	class PlatformWindowInterface;
}

namespace edge::gfx {
	class GraphicsSemaphoreInterface;
	class GraphicsFenceInterface;
	class GraphicsContextInterface;

	struct GraphicsContextCreateInfo {
		GraphicsDeviceType physical_device_type;
		platform::PlatformWindowInterface* window;

		struct RequireFeatures {
			bool mesh_shading{};
			bool ray_tracing{};
		} require_features{};
	};
}