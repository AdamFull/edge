#include "../entry_point.h"

#include <game-activity/GameActivity.cpp>
#include <game-text-input/gametextinput.cpp>

extern "C" {
#include <game-activity/native_app_glue/android_native_app_glue.c>
}

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

	auto AndroidPlatformContext::initialize() -> bool {
		return true;
	}

	auto AndroidPlatformContext::shutdown() -> void {

	}

    auto AndroidPlatformContext::get_platform_name() const -> std::string_view {
        return "Android";
    }

    auto AndroidPlatformContext::get_window() -> PlatformWindowInterface& {
        return *window_;
    }

    auto AndroidPlatformContext::get_window() const->PlatformWindowInterface const& {
        return *window_;
    }

    auto AndroidPlatformContext::create_window(const window::Properties& props) -> bool {
        window_ = AndroidPlatformWindow::construct(props);
        if (!window_) {
            return false;
        }

        if (!window_->create()) {
            window_.reset();
            return false;
        }

        return true;
    }

	auto AndroidPlatformContext::get_android_app() -> android_app* {
		return android_app_;
	}

	auto AndroidPlatformContext::get_android_app() const -> const android_app* {
		return android_app_;
	}
}

extern "C" void android_main(android_app* state) {
	auto context = edge::platform::AndroidPlatformContext::construct(state);
	platform_main(*context);
}