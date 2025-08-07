#include "platform.h"

#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <cassert>

#include "jni_helper.h"

namespace edge::platform {
    auto AndroidPlatformWindow::construct(PlatformContextInterface* platform_context) -> std::unique_ptr<AndroidPlatformWindow> {
        auto window = std::make_unique<AndroidPlatformWindow>();
        auto& android_platform_context = static_cast<AndroidPlatformContext&>(*platform_context);
        window->android_app_ = android_platform_context.get_android_app();
        window->platform_context_ = platform_context;
        return window;
    }

    auto AndroidPlatformWindow::create(const window::Properties& props) -> bool {
        android_app_->onAppCmd = on_app_cmd;
        android_app_->userData = this;

        // TODO: Setup filters here

        input_layer_ = InputLayer::construct(android_app_, platform_context_);
        if(!input_layer_->initialize()) {
            return false;
        }

        return true;
    }

    auto AndroidPlatformWindow::destroy() -> void {
        input_layer_->shutdown();
        GameActivity_finish(android_app_->activity);
        requested_close_ = true;
    }

    auto AndroidPlatformWindow::poll_events() -> void {
        auto& dispatcher = platform_context_->get_event_dispatcher();
        auto* env = get_jni_env(android_app_);

        android_poll_source* source;
        int ident, events;

        while ((ident = ALooper_pollOnce(0, nullptr, &events, (void**)&source)) > ALOOPER_POLL_TIMEOUT) {
            if (source) {
                source->process(android_app_, source);
            }

            if (android_app_->destroyRequested != 0) {
                dispatcher.emit(events::WindowShouldCloseEvent{
                        .window_id = ~0ull
                });
            }
            requested_close_ = true;
        }

        input_layer_->update();
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
                dispatcher.emit(events::WindowFocusChangedEvent{
                        .focused = true,
                        .window_id = ~0ull
                });
                break;
            }
            case APP_CMD_LOST_FOCUS: {
                dispatcher.emit(events::WindowFocusChangedEvent{
                        .focused = false,
                        .window_id = ~0ull
                });
                break;
            }
            case APP_CMD_START: {
                gamepad_manager_->on_app_start();
                break;
            }
            case APP_CMD_STOP: {
                gamepad_manager_->on_app_stop();
                break;
            }
        }
    }
}