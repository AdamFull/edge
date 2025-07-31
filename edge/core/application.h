#pragma once

namespace edge {
	class ApplicationInterface {
	public:
		virtual ~ApplicationInterface() = default;

		virtual auto initialize() -> bool = 0;
		virtual auto finish() -> void = 0;

		virtual auto update(float delta_time) -> void = 0;
		virtual auto fixed_update(float delta_time) -> void = 0;
	};
}