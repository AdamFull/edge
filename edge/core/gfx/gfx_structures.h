#pragma once

#include "gfx_enum.h"

#include <memory>

namespace edge::platform {
	class IPlatformWindow;
}

namespace edge::gfx {
	class IGFXContext;

	class IGFXSemaphore;
	class IGFXFence;
	class IGFXQueue;

	class IGFXCommandList;
	class IGFXCommandAllocator;

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
		uint64_t value{ 0ull };
		StageFlags stage;
	};

	struct SubmitInfo {
		std::span<SemaphoreSubmitInfo> wait_semaphore_infos;
		std::span<std::shared_ptr<IGFXCommandList>> command_lists;
		std::span<SemaphoreSubmitInfo> signal_semaphore_infos;
	};

	struct PresentInfo {
		std::span<std::shared_ptr<IGFXSemaphore>> wait_semaphores;
		//std::span<std::shared_ptr<IGFXSwapchain>> swapchains;
		const uint32_t* image_indices{ nullptr };
	};
}