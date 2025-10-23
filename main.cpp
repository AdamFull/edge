#include "edge/core/platform/entry_point.h"
#include "edge/engine.h"

#include "edge/core/event_system.h"
#include "edge/core/foundation/enum_flags.h"

#include "edge/core/filesystem/filesystem.h"

#include <spdlog/spdlog.h>

auto platform_main(edge::platform::PlatformContext& platform_context) -> int {
	edge::fs::initialize_filesystem();

	edge::platform::window::Properties window_properties{
		.title = "Edge Engine - Windows Demo"
	};

	if (!platform_context.initialize({ window_properties })) {
		return -1;
	}

	// Initialize graphics interface
	edge::gfx::initialize_graphics(
		edge::gfx::ContextInfo{
			.preferred_device_type = vk::PhysicalDeviceType::eDiscreteGpu,
			.window = &platform_context.get_window()
		});

	spdlog::info("{}", platform_context.get_platform_name());

	auto& window = platform_context.get_window();
	window.show();

    spdlog::info("Window created: {}x{}", window.get_width(), window.get_height());
    spdlog::info("Window title: {}", window.get_title());

	if (!platform_context.setup_application([](std::unique_ptr<edge::IApplication>& out_app) {
		out_app = std::make_unique<edge::Engine>();
		})) {
		return -2;
	}

	platform_context.main_loop();
	platform_context.shutdown();

	edge::gfx::shutdown_graphics();
	edge::fs::shutdown_filesystem();

	return 0;
}