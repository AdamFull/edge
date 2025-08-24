#include "../entry_point.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/android_sink.h>

#include <game-activity/native_app_glue/android_native_app_glue.h>

#include <cassert>

#include "jni_helper.h"


#include "../../gfx/vulkan/vk_context.h"

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

        auto logger = std::make_shared<spdlog::logger>("Logger", std::make_shared<spdlog::sinks::android_sink_mt>("Logger"));
#ifdef NDEBUG
        logger->set_level(spdlog::level::info);
#else
        logger->set_level(spdlog::level::trace);
#endif
        logger->set_pattern("[%Y-%m-%d %H:%M:%S] [%^%l%$] %v");
        spdlog::set_default_logger(logger);

        window_ = AndroidPlatformWindow::construct(this);
        if (!window_) {
            spdlog::error("[Android Runtime Context]: Window construction failed");
            return false;
        }

        input_ = AndroidPlatformInput::construct(this);
        if (!input_) {
            spdlog::error("[Android Runtime Context]: Input construction failed");
            return false;
        }

        graphics_ = gfx::VulkanGraphicsContext::construct();
        if (!graphics_) {
            spdlog::error("[Android Runtime Context]: Graphics construction failed");
            return false;
        }

        return true;
    }
}

extern "C" void android_main(android_app* state) {
	auto context = edge::platform::AndroidPlatformContext::construct(state);
	platform_main(*context);
}