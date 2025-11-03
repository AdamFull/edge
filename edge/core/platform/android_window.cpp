#include "android_window.h"
#include "android_input.h"
#include "android_context.h"

#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <cassert>

#include "android_jni_helper.h"

#define EDGE_LOGGER_SCOPE "platform::AndroidPlatformWindow"

namespace edge::platform {
    AndroidPlatformWindow::~AndroidPlatformWindow() {
        GameActivity_finish(android_app_->activity);
        requested_close_ = true;
    }

    auto AndroidPlatformWindow::construct(AndroidPlatformContext* platform_context) -> std::unique_ptr<AndroidPlatformWindow> {
        auto window = std::make_unique<AndroidPlatformWindow>();
        window->android_app_ = platform_context->get_android_app();
        window->platform_context_ = platform_context;
        return window;
    }

    auto AndroidPlatformWindow::create(const window::Properties& props) -> bool {
        android_app_->onAppCmd = on_app_cmd;
        android_app_->userData = this;

        // TODO: Setup filters here

        return true;
    }

    auto AndroidPlatformWindow::poll_events(float delta_time) -> void {
        auto& dispatcher = platform_context_->get_event_dispatcher();

        android_poll_source* source;
        int ident, events;

        while ((ident = ALooper_pollOnce(0, nullptr, &events, (void**)&source)) > ALOOPER_POLL_TIMEOUT) {
            if (source) {
                source->process(android_app_, source);
            }

            if (android_app_->destroyRequested != 0) {
                EDGE_SLOGD("Requested window destroy.");
                dispatcher.emit(events::WindowShouldCloseEvent{
                        .window_id = ~0ull
                    });
                requested_close_ = true;
            }
        }

        auto& input = platform_context_->get_input();
        input.update(delta_time);
    }

    auto AndroidPlatformWindow::get_dpi_factor() const noexcept -> float {
        return static_cast<float>(AConfiguration_getDensity(android_app_->config)) / static_cast<float>(ACONFIGURATION_DENSITY_MEDIUM);
    }

    auto AndroidPlatformWindow::get_content_scale_factor() const noexcept -> float {
        return 1.0f;
    }

    auto AndroidPlatformWindow::set_title(std::string_view title) -> void {
        properties_.title = title;
    }

    auto AndroidPlatformWindow::get_native_handle() -> void* {
        return android_app_->window;
    }

    auto AndroidPlatformWindow::on_app_cmd(android_app* app, int32_t cmd) -> void {
        auto context = reinterpret_cast<AndroidPlatformWindow*>(app->userData);
        assert(context && "Context is not valid.");
        context->_process_commands(app, cmd);
    }

    auto AndroidPlatformWindow::_process_commands(android_app* app, int32_t cmd) -> void {
        auto& dispatcher = platform_context_->get_event_dispatcher();
        auto* env = get_jni_env(android_app_);

        switch (cmd) {
        case APP_CMD_INIT_WINDOW: {
            dispatcher.emit(events::WindowSizeChangedEvent{
                    .width = ANativeWindow_getWidth(app->window),
                    .height = ANativeWindow_getHeight(app->window),
                    .window_id = ~0ull
                });
            surface_ready_ = true;
            break;
        }
        case APP_CMD_CONTENT_RECT_CHANGED: {
            EDGE_SLOGD("Window rect changed [{}, {}, {}, {}].",
                app->contentRect.left, app->contentRect.right,
                app->contentRect.top, app->contentRect.bottom);

            // Get the new size
            auto width = app->contentRect.right - app->contentRect.left;
            auto height = app->contentRect.bottom - app->contentRect.top;
            dispatcher.emit(events::WindowSizeChangedEvent{
                    .width = width,
                    .height = height,
                    .window_id = ~0ull
                });

            break;
        }
        case APP_CMD_GAINED_FOCUS: {
            EDGE_SLOGD("Window focus gained.");

            dispatcher.emit(events::WindowFocusChangedEvent{
                    .focused = true,
                    .window_id = ~0ull
                });
            break;
        }
        case APP_CMD_LOST_FOCUS: {
            EDGE_SLOGD("Window focus lost.");

            dispatcher.emit(events::WindowFocusChangedEvent{
                    .focused = false,
                    .window_id = ~0ull
                });
            break;
        }
        case APP_CMD_START: {
            EDGE_SLOGD("Application started.");

            auto& input = static_cast<AndroidPlatformInput&>(platform_context_->get_input());
            input.on_app_start();
            break;
        }
        case APP_CMD_STOP: {
            EDGE_SLOGD("Application stopped.");

            auto& input = static_cast<AndroidPlatformInput&>(platform_context_->get_input());
            input.on_app_stop();
            break;
        }
        default: {
            static std::array<std::string_view, 21> lut{
                "UNUSED_APP_CMD_INPUT_CHANGED",
                "APP_CMD_INIT_WINDOW",
                "APP_CMD_TERM_WINDOW",
                "APP_CMD_WINDOW_RESIZED",
                "APP_CMD_WINDOW_REDRAW_NEEDED",
                "APP_CMD_CONTENT_RECT_CHANGED",
                "APP_CMD_SOFTWARE_KB_VIS_CHANGED",
                "APP_CMD_GAINED_FOCUS",
                "APP_CMD_LOST_FOCUS",
                "APP_CMD_CONFIG_CHANGED",
                "APP_CMD_LOW_MEMORY",
                "APP_CMD_START",
                "APP_CMD_RESUME",
                "APP_CMD_SAVE_STATE",
                "APP_CMD_PAUSE",
                "APP_CMD_STOP",
                "APP_CMD_DESTROY",
                "APP_CMD_WINDOW_INSETS_CHANGED",
                "APP_CMD_EDITOR_ACTION",
                "APP_CMD_KEY_EVENT",
                "APP_CMD_TOUCH_EVENT"
            };

            EDGE_SLOGE("Unhandled window command: {}", lut[cmd]);

            break;
        }
        }
    }
}

#undef EDGE_LOGGER_SCOPE