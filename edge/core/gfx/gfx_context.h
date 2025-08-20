#pragma once

#include <memory>

#include "gfx_structures.h"

namespace edge::gfx {
	class GraphicsSemaphoreInterface {
	public:
		virtual ~GraphicsSemaphoreInterface() = default;
	};

	class GraphicsFenceInterface {
	public:
		virtual ~GraphicsFenceInterface() = default;

		virtual auto wait(uint64_t timeout = std::numeric_limits<uint64_t>::max()) -> void = 0;
		virtual auto reset() -> void = 0;
	};

	class GraphicsQueueInterface {
	public:
		virtual ~GraphicsQueueInterface() = default;

		//virtual auto submit(const SubmitInfo& submit_info, GraphicsFenceInterface* fence) const -> void = 0;
		//virtual auto present(const PresentInfo& present_info) const -> void = 0;
		virtual auto wait_idle() const -> void = 0;
	};

	class GraphicsContextInterface {
	public:
		virtual ~GraphicsContextInterface() = default;

		virtual auto create(const GraphicsContextCreateInfo& create_info) -> bool = 0;


	};
}