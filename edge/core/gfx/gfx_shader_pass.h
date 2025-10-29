#pragma once

#include "gfx_context.h"

namespace edge::gfx {
	class IShaderPass {
	public:
		virtual ~IShaderPass() = default;

		virtual auto execute(CommandBuffer const& cmd, float delta_time) -> void = 0;
	};
}