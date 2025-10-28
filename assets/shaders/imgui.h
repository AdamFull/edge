#pragma once

#include "interop.h"

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