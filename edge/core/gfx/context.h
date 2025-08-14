#pragma once

#include <memory>

namespace edge::platform {
	class PlatformWindowInterface;
}

namespace edge::gfx {
	enum class GraphicsDeviceType {
		eDiscrete,
		eIntegrated,
		eSoftware
	};

	struct GraphicsContextCreateInfo {
		GraphicsDeviceType physical_device_type;
		platform::PlatformWindowInterface* window;

		struct RequireFeatures {
			bool mesh_shading{};
			bool ray_tracing{};
		} require_features{};
	};

	class GraphicsContextInterface {
	public:
		virtual ~GraphicsContextInterface() = default;

		virtual auto create(const GraphicsContextCreateInfo& create_info) -> bool = 0;


	};
}