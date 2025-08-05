#include "../entry_point.h"

#include <game-activity/GameActivity.cpp>
#include <game-text-input/gametextinput.cpp>
#include <cassert>

extern "C" {
#include <game-activity/native_app_glue/android_native_app_glue.c>
}

namespace edge::platform {
    auto get_jni_env(android_app* app) -> JNIEnv* {
        JNIEnv* env{ nullptr };
        jint get_env_result = app->activity->vm->GetEnv((void**)&env, JNI_VERSION_1_6);
        if (get_env_result == JNI_EDETACHED) {
            if (app->activity->vm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
                return nullptr;
            }
        }

        return env;
    }

    auto get_package_code_path(android_app* app) -> std::string {
        auto* env = get_jni_env(app);

        jclass clazz = env->GetObjectClass(app->activity->javaGameActivity);
        jmethodID methodID = env->GetMethodID(clazz, "getPackageCodePath", "()Ljava/lang/String;");
        jstring apk_path_java = (jstring)env->CallObjectMethod(app->activity->javaGameActivity, methodID);

        const char* apk_path_chars = env->GetStringUTFChars(apk_path_java, nullptr);
        return std::string(apk_path_chars ? apk_path_chars : "");
    }

    auto key_event_filter(const GameActivityKeyEvent* event) -> bool {
        if (event->source == AINPUT_SOURCE_KEYBOARD) {
            return true;
        }
        return false;
    }

	auto AndroidPlatformContext::construct(android_app* app)
		-> std::unique_ptr<AndroidPlatformContext> {
        auto ctx = std::make_unique<AndroidPlatformContext>();
        ctx->_construct(app);
        return ctx;
    }

	auto AndroidPlatformContext::initialize(const PlatformCreateInfo& create_info) -> bool {
        //app_path = get_package_code_path(app);
        //external_resources_path = app->activity->obbPath;
        //app->activity->internalDataPath;
        //temp_path = app->activity->externalDataPath;

        android_app_->onAppCmd = on_app_cmd;
        android_app_->userData = this;

        auto code = PlatformContextInterface::initialize(create_info);
        if (code != 0) {
            return code;
        }

        do {
            window_->poll_events();
        } while (!surface_ready_);

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

    auto AndroidPlatformContext::on_app_cmd(android_app* app, int32_t cmd) -> void {
        auto context = reinterpret_cast<AndroidPlatformContext*>(app->userData);
        assert(context && "Context is not valid.");
        context->_process_commands(app, cmd);
    }

    auto AndroidPlatformContext::_process_commands(android_app* app, int32_t cmd) -> void {
        switch (cmd) {
            case APP_CMD_INIT_WINDOW: {
                event_dispatcher_->emit(events::WindowSizeChangedEvent{
                        .width = ANativeWindow_getWidth(app->window),
                        .height = ANativeWindow_getHeight(app->window),
                        .window_id = ~0ull
                });
                surface_ready_ = true;
                break;
            }
            case APP_CMD_CONTENT_RECT_CHANGED: {
                // Get the new size
                auto width = app->contentRect.right - app->contentRect.left;
                auto height = app->contentRect.bottom - app->contentRect.top;
                event_dispatcher_->emit(events::WindowSizeChangedEvent{
                        .width = width,
                        .height = height,
                        .window_id = ~0ull
                });
                break;
            }
            case APP_CMD_GAINED_FOCUS: {
                event_dispatcher_->emit(events::WindowFocusChangedEvent{
                        .focused = true,
                        .window_id = ~0ull
                });
                break;
            }
            case APP_CMD_LOST_FOCUS: {
                event_dispatcher_->emit(events::WindowFocusChangedEvent{
                        .focused = false,
                        .window_id = ~0ull
                });
                break;
            }
        }
    }
}

extern "C" void android_main(android_app* state) {
	auto context = edge::platform::AndroidPlatformContext::construct(state);
	platform_main(*context);
}