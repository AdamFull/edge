#include "edge/core/platform/entry_point.h"

#include "edge/core/event_system.h"
#include "edge/core/foundation/enum_flags.h"

#include <spdlog/spdlog.h>

class MyApplication : public edge::ApplicationInterface {
public:
	~MyApplication() override = default;

	auto initialize() -> bool override {
		return true;
	}

	auto finish() -> void override {

	}

	auto update(float delta_time) -> void override {

	}

	auto fixed_update(float delta_time) -> void override {

	}
};

auto platform_main(edge::platform::PlatformContext& platform_context) -> int {
	edge::platform::window::Properties window_properties{
		.title = "Edge Engine - Windows Demo"
	};

	if (!platform_context.initialize({ window_properties })) {
		return -1;
	}

	spdlog::info("{}", platform_context.get_platform_name());

	auto& window = platform_context.get_window();
	window.show();

    spdlog::info("Window created: {}x{}", window.get_width(), window.get_height());
    spdlog::info("Window title: {}", window.get_title());

	if (!platform_context.setup_application([](std::unique_ptr<edge::ApplicationInterface>& out_app) {
		out_app = std::make_unique<MyApplication>();
		})) {
		return -2;
	}

	platform_context.main_loop();

	platform_context.shutdown();

	return 0;
}