#pragma once 

namespace edge {
	class ILayer {
	public:
		virtual ~ILayer() = default;

		virtual auto attach() -> void = 0;
		virtual auto detach() -> void = 0;

		virtual auto update(float delta_time) -> void = 0;
		virtual auto fixed_update(float delta_time) -> void = 0;
	};
}