#pragma once

#include <expected>

#include <tiny_imageformat/tinyimageformat.h>

#include "gfx_enum.h"
#include "../foundation/foundation.h"

namespace edge::platform {
	class IPlatformWindow;
}

namespace edge::gfx {
	class IGFXContext;

	class IGFXSemaphore;
	class IGFXQueue;
	class IGFXCommandAllocator;
	class IGFXCommandList;

	class IGFXImage;

	template<typename T>
	using GFXResult = std::expected<T, Result>;

	struct Extent2D {
		uint32_t width, height;
	};

	struct Extent3D {
		uint32_t width, height, depth;
	};

	struct GraphicsContextCreateInfo {
		GraphicsDeviceType physical_device_type;
		platform::IPlatformWindow* window;

		struct RequireFeatures {
			bool mesh_shading{};
			bool ray_tracing{};
		} require_features{};
	};

	struct SemaphoreSubmitInfo {
		Shared<IGFXSemaphore> semaphore;
		uint64_t value;
	};

	struct SubmitQueueInfo {
		std::vector<SemaphoreSubmitInfo> wait_semaphores;
		std::vector<SemaphoreSubmitInfo> signal_semaphores;
		std::vector<Shared<IGFXCommandList>> command_lists;
	};

	struct PresentInfo {
		std::vector<SemaphoreSubmitInfo> wait_semaphores;
		std::vector<SemaphoreSubmitInfo> signal_semaphores;
	};

	struct PresentationEngineCreateInfo {
		QueueType queue_type;

		uint32_t width, height;
		uint32_t image_count;
		TinyImageFormat format;
		ColorSpace color_space;

		bool vsync;
		bool hdr;
	};

	struct BufferCreateInfo {
		uint64_t block_size;
		uint64_t count_block;
		BufferType type;
	};

	struct ImageCreateInfo {
		Extent3D extent;
		uint32_t layers;
		uint32_t levels;

		TinyImageFormat format;
		ImageFlags flags;
	};
}