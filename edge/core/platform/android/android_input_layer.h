#pragma once

#include <memory>

struct _JNIEnv;
typedef _JNIEnv JNIEnv;

struct android_app;
struct GameActivityMotionEvent;
struct GameActivityKeyEvent;

namespace edge::platform {
    class PlatformContextInterface;

    class InputLayer {
    public:
        static auto construct(android_app* app, PlatformContextInterface* platform_context) -> std::unique_ptr<InputLayer>;

        auto initialize() -> bool;
        auto shutdown() -> void;

        auto update() -> void;

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
        PlatformContextInterface* platform_context_{ nullptr };
    };
}