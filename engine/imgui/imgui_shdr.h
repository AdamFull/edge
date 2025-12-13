#ifndef IMGUI_SHDR_H
#define IMGUI_SHDR_H

#include "../shading/interop.h"

namespace edge::gfx::imgui {
	struct Vertex {
		float2 pos;
		float2 uv;
		uint8_t4 col;
	};

	struct PushConstant {
		TYPE_PTR(Vertex) vertices;
		float2 scale;
		float2 translate;
		uint16_t image_index;
		uint16_t sampler_index;
	};
}

#endif
