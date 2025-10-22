#pragma once

namespace edge {
	namespace platform {
		class IPlatformContext;
	}

	class IApplication {
	public:
		virtual ~IApplication() = default;

		virtual auto initialize(platform::IPlatformContext& context) -> bool = 0;
		virtual auto finish() -> void = 0;

		virtual auto update(float delta_time) -> void = 0;
		virtual auto fixed_update(float delta_time) -> void = 0;
	};
}