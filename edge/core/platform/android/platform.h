#pragma once

#include "../platform.h"

struct _JNIEnv;
typedef _JNIEnv JNIEnv;

struct android_app;
struct GameActivityMotionEvent;
struct GameActivityKeyEvent;
struct GameTextInputState;

namespace edge::platform {
    class AndroidPlatformInput;
    class AndroidPlatformWindow;
    class AndroidPlatformContext;

    class AndroidPlatformInput final : public PlatformInputInterface {
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
        std::string input_string_{};
    };

    class AndroidPlatformWindow final : public PlatformWindowInterface {
    public:
        ~AndroidPlatformWindow() override;
        static auto construct(AndroidPlatformContext* platform_context) -> std::unique_ptr<AndroidPlatformWindow>;

        auto create(const window::Properties& props) -> bool override;
        [[maybe_unused]] auto show() -> void override {}
        [[maybe_unused]] auto hide() -> void override {}
        [[nodiscard]] auto is_visible() const -> bool override { return surface_ready_; }
        auto poll_events() -> void override;

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

    using Window = AndroidPlatformWindow;

	class AndroidPlatformContext final : public PlatformContextInterface {
	public:
		static auto construct(android_app* app) -> std::unique_ptr<AndroidPlatformContext>;

        auto shutdown() -> void override;
        [[nodiscard]] auto get_platform_name() const->std::string_view override;

		auto get_android_app() -> android_app*;
		[[nodiscard]] auto get_android_app() const -> const android_app*;
    private:
		auto _construct(android_app* app) -> bool;

		android_app* android_app_{ nullptr };
	};

	using PlatformContext = AndroidPlatformContext;
}