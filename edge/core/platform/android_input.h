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

    class AndroidPlatformInput final : public IPlatformInput {
    public:
        ~AndroidPlatformInput() override;
        static auto construct(AndroidPlatformContext* platform_context) -> std::unique_ptr<AndroidPlatformInput>;

        auto create() -> bool override;

        auto update(float delta_time) -> void override;

        auto begin_text_input_capture(std::string_view initial_text = {}) -> bool override;
        auto end_text_input_capture() -> void override;

        auto set_gamepad_color(int32_t gamepad_id, uint32_t color) -> bool override;

        auto process_motion_event(GameActivityMotionEvent* event) -> void;
        auto process_key_event(GameActivityKeyEvent* event) -> void;
        auto on_app_start() -> void;
        auto on_app_stop() -> void;
    private:
        auto process_controller_motion_data(const int32_t controller_index, const void* motion_data) -> void;
        auto process_controller_status_change(const int32_t controller_index, const uint32_t controller_status) -> void;
        auto process_mouse_status_change(const uint32_t mouse_status) -> void;
        auto process_keyboard_status_change(bool mouse_status) -> void;

        android_app* android_app_{ nullptr };
        JNIEnv* jni_env_{ nullptr };
        AndroidPlatformContext* platform_context_{ nullptr };

        GameTextInputState* input_state_{ nullptr };
        mi::String input_string_{};
    };
}