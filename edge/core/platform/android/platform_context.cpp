#include "../entry_point.h"

#include <game-activity/native_app_glue/android_native_app_glue.h>

#include <cassert>

#include "jni_helper.h"

namespace edge::platform {
    auto get_package_code_path(android_app* app) -> std::string {
        auto* env = get_jni_env(app);

        jclass clazz = env->GetObjectClass(app->activity->javaGameActivity);
        jmethodID methodID = env->GetMethodID(clazz, "getPackageCodePath", "()Ljava/lang/String;");
        jstring apk_path_java = (jstring)env->CallObjectMethod(app->activity->javaGameActivity, methodID);

        const char* apk_path_chars = env->GetStringUTFChars(apk_path_java, nullptr);
        return std::string(apk_path_chars ? apk_path_chars : "");
    }

	auto AndroidPlatformContext::construct(android_app* app)
		-> std::unique_ptr<AndroidPlatformContext> {
        auto ctx = std::make_unique<AndroidPlatformContext>();
        ctx->_construct(app);
        return ctx;
    }

	auto AndroidPlatformContext::initialize(const PlatformCreateInfo& create_info) -> bool {
        window_ = AndroidPlatformWindow::construct(this);
        if(!window_->create(create_info.window_props)) {
            // TODO: LOG ERROR
            return false;
        }

        auto code = PlatformContextInterface::initialize(create_info);
        if (code != 0) {
            return code;
        }

        do {
            window_->poll_events();
        } while (!window_->is_visible());

		return true;
	}

	auto AndroidPlatformContext::shutdown() -> void {
	}

    auto AndroidPlatformContext::get_platform_name() const -> std::string_view {
        return "Android";
    }

	auto AndroidPlatformContext::get_android_app() -> android_app* {
		return android_app_;
	}

	auto AndroidPlatformContext::get_android_app() const -> const android_app* {
		return android_app_;
	}

    auto AndroidPlatformContext::_construct(android_app* app) -> bool {
        android_app_ = app;
        return true;
    }
}

extern "C" void android_main(android_app* state) {
	auto context = edge::platform::AndroidPlatformContext::construct(state);
	platform_main(*context);
}