#include "edge/platform/entry_point.h"

#include <print>

auto platform_main(edge::platform::PlatformContext& platform_context) -> int {
	if (!platform_context.initialize()) {
		return -1;
	}

	std::println("{}", platform_context.get_platform_name());

	platform_context.shutdown();

	return 0;
}