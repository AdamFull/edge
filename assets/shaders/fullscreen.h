#pragma once

namespace edge::gfx::fullscreen {
	struct PushConstant {
		uint32_t width;
		uint32_t height;
		uint32_t image_id;
	};
}