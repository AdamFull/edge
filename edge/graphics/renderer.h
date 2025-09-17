#pragma once

#include "../core/gfx/gfx_context.h"

namespace edge::graphics {
	class Frame {
	public:
	private:
		gfx::Image backbuffer_;
		gfx::ImageView backbuffer_image_view_;

		//gfx::CommandList command_list_;

		gfx::Semaphore image_available_;
		gfx::Semaphore rendering_finished_;
		gfx::Fence fence_;

	};

	class Renderer {
	public:
	private:
	};
}