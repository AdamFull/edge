#pragma once

#include "gfx_shader_pass.h"

namespace edge::gfx {
	class Renderer;

	class TestPass final : public IShaderPass {
	public:
		~TestPass() override = default;

		static auto create(Renderer& renderer, uint32_t read_target, Pipeline const* pipeline) -> Owned<TestPass>;

		auto execute(CommandBuffer const& cmd, float delta_time) -> void override;
	private:
		Renderer* renderer_{ nullptr };
		Pipeline const* pipeline_{ nullptr };

		uint32_t read_target_{ ~0u };
		uint32_t render_target_{ ~0u };
	};
}