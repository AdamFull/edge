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
		GraphicsDeviceType physical_device_type{ GraphicsDeviceType::eDiscrete };
		platform::IPlatformWindow* window{ nullptr };

		struct RequireFeatures {
			bool mesh_shading{ false };
			bool ray_tracing{ false };
		} require_features{};
	};

	struct SemaphoreSubmitInfo {
		Shared<IGFXSemaphore> semaphore;
		uint64_t value{ 0ull };
	};

	struct SubmitQueueInfo {
		Span<SemaphoreSubmitInfo> wait_semaphores;
		Span<SemaphoreSubmitInfo> signal_semaphores;
		Span<Shared<IGFXCommandList>> command_lists;
	};

	struct PresentInfo {
		Span<SemaphoreSubmitInfo> wait_semaphores;
		Span<SemaphoreSubmitInfo> signal_semaphores;
	};

	struct PresentationEngineCreateInfo {
		QueueType queue_type{ QueueType::eDirect };

		Extent2D extent{ 1u, 1u };
		uint32_t image_count{ 1u };
		TinyImageFormat format{ TinyImageFormat_UNDEFINED };
		ColorSpace color_space{};

		bool vsync{ false };
		bool hdr{ false };
	};

	struct BufferCreateInfo {
		uint64_t block_size{ 1ull };
		uint64_t count_block{ 1ull };
		BufferType type{};
	};

	struct BufferViewCreateInfo {
		uint64_t byte_offset{ 0ull };
		uint64_t size{ 1ull };
		TinyImageFormat format{ TinyImageFormat_UNDEFINED };
	};

	struct ImageCreateInfo {
		Extent3D extent{ 1u, 1u, 1u };
		uint32_t layers{ 1u };
		uint32_t levels{ 1u };

		TinyImageFormat format{ TinyImageFormat_UNDEFINED };
		ImageFlags flags{};
	};

	struct ImageViewCreateInfo {
		uint32_t first_layer{ 0u };
		uint32_t layers{ 1u };
		uint32_t first_level{ 0u };
		uint32_t levels{ 1u };
		ImageViewType type{};
	};
}