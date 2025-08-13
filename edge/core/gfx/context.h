#pragma once

#include <memory>

#if EDGE_PLATFORM_ANDROID
#elif EDGE_PLATFORM_WINDOWS
#include <Windows.h>
#endif

namespace edge::gfx {
	enum class GraphicsDeviceType {
		eDiscrete,
		eIntegrated,
		eSoftware
	};

	struct GraphicsContextCreateInfo {
		GraphicsDeviceType physical_device_type;
#if EDGE_PLATFORM_ANDROID
		AWindowHandle* window;
#elif EDGE_PLATFORM_WINDOWS
		HWND hwnd;
		HINSTANCE hinst;
#endif
	};

	class GraphicsContextInterface {
	public:
		virtual ~GraphicsContextInterface() = default;

		virtual auto initialize() -> bool = 0;
		virtual auto shutdown() -> void = 0;


	};
}