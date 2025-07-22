#include "entry_point.h"

#include <game-activity/native_app_glue/android_native_app_glue.h>

namespace edge::platform {
	auto AndroidPlatformContext::construct(android_app* app)
		-> std::unique_ptr<AndroidPlatformContext> {
		auto ctx = std::make_unique<AndroidPlatformContext>();
		ctx->_construct(app);
		return ctx;
	}

	auto AndroidPlatformContext::_construct(android_app* app) -> bool {
		android_app_ = app;
		return true;
	}

	auto AndroidPlatformContext::_initialize() -> bool {
		return true;
	}

	auto AndroidPlatformContext::_get_platform_name() const -> std::string_view {
		return "Android";
	}

	auto AndroidPlatformContext::_shutdown() -> void {

	}

	auto AndroidPlatformContext::get_android_app() -> android_app* {
		return android_app_;
	}

	auto AndroidPlatformContext::get_android_app() const -> const android_app* {
		return android_app_;
	}
}

extern "C" auto android_main(android_app* state) -> void {
	auto context = edge::platform::AndroidPlatformContext::construct(state);
	platform_main(*context);
}