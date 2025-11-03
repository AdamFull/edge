#pragma once

#include "platform.h"

struct _JNIEnv;
typedef _JNIEnv JNIEnv;

struct android_app;
struct GameActivityMotionEvent;
struct GameActivityKeyEvent;
struct GameTextInputState;

namespace edge::platform {
    class AndroidPlatformContext;

    class AndroidPlatformWindow final : public IPlatformWindow {
    public:
        ~AndroidPlatformWindow() override;
        static auto construct(AndroidPlatformContext* platform_context) -> std::unique_ptr<AndroidPlatformWindow>;

        auto create(const window::Properties& props) -> bool override;
        [[maybe_unused]] auto show() -> void override {}
        [[maybe_unused]] auto hide() -> void override {}
        [[nodiscard]] auto is_visible() const -> bool override { return surface_ready_; }
        auto poll_events(float delta_time) -> void override;

        auto get_dpi_factor() const noexcept -> float override;
        auto get_content_scale_factor() const noexcept -> float override;

        auto set_title(std::string_view title) -> void override;

        [[nodiscard]] auto get_native_handle() -> void* override;
    private:
        static auto on_app_cmd(android_app* app, int32_t cmd) -> void;
        auto _process_commands(android_app* app, int32_t cmd) -> void;

        android_app* android_app_{ nullptr };
        AndroidPlatformContext* platform_context_{ nullptr };
        bool surface_ready_{ false };
    };
}