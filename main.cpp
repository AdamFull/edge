#include "edge/core/platform/entry_point.h"

#include <print>

auto platform_main(edge::platform::PlatformContext& platform_context) -> int {
	if (!platform_context.initialize()) {
		return -1;
	}

	std::println("{}", platform_context.get_platform_name());

	edge::platform::window::Properties window_properties{
		.title = "Edge Engine - Windows Demo"
	};
	if (platform_context.create_window(window_properties)) {
		auto& window = platform_context.get_window();
		window.show();

		std::println("Window created: {}x{}", window.get_width(), window.get_height());
		std::println("Window title: {}", window.get_title());

		while (!window.requested_close()) {
			window.poll_events();
		}
	}
	else {
		std::println("Failed to create window");
	}

	platform_context.shutdown();

	return 0;
}