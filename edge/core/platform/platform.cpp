#include "platform.h"

namespace edge::platform {
	auto PlatformContextInterface::initialize(const PlatformCreateInfo& create_info) -> bool {
		if (!window_->create(create_info.window_props)) {
			window_.reset();
			return false;
		}

		event_dispatcher_ = std::make_unique<events::Dispatcher>();

		return true;
	}

	auto PlatformContextInterface::tick(float dt) -> void {
		window_->poll_events();
		event_dispatcher_->process_events();
	}
}