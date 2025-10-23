#pragma once

#include "core/application.h"
#include "core/gfx/gfx_renderer.h"

namespace edge {
	namespace platform {
		class IPlatformWindow;
	}

	class Engine final : public IApplication {
	public:
		auto initialize(platform::IPlatformContext& context) -> bool override;
		auto finish() -> void override;

		auto update(float delta_time) -> void override;
		auto fixed_update(float delta_time) -> void override;
	private:
		platform::IPlatformWindow* window_{ nullptr };
		std::unique_ptr<gfx::Renderer> renderer_;
	};
}