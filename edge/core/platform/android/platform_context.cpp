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
        ctx->window_ = AndroidPlatformWindow::construct(ctx.get());
        ctx->input_ = AndroidPlatformInput::construct(ctx.get());
        return ctx;
    }

	auto AndroidPlatformContext::shutdown() -> void {
        input_->destroy();
        window_->destroy();
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