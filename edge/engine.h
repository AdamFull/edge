#pragma once

#include "core/application.h"

namespace edge {
	class Engine final : public IApplication {
	public:
		auto initialize(platform::IPlatformContext const& context) -> bool override;
		auto finish() -> void override;

		auto update(float delta_time) -> void override;
		auto fixed_update(float delta_time) -> void override;
	private:

	};
}