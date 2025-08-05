#include "platform.h"

#include <game-activity/native_app_glue/android_native_app_glue.h>

#include <unordered_map>
#include <cassert>

namespace edge::platform {
    inline constexpr auto trnaslate_key_action(int action) -> KeyAction {
        if (action == AKEY_STATE_DOWN)
            return KeyAction::ePress;
        else if (action == AKEY_STATE_UP)
            return KeyAction::eRelease;

        return KeyAction::eUnknown;
    }

	inline auto translate_keyboard_key_code(int key) -> KeyboardKeyCode {
		static auto lut = std::unordered_map<int32_t, KeyboardKeyCode>({
			{ AKEYCODE_SPACE, KeyboardKeyCode::eSpace },
			{ AKEYCODE_APOSTROPHE, KeyboardKeyCode::eApostrophe },
			{ AKEYCODE_COMMA, KeyboardKeyCode::eComma },
			{ AKEYCODE_MINUS, KeyboardKeyCode::eMinus },
			{ AKEYCODE_PERIOD, KeyboardKeyCode::ePeriod },
			{ AKEYCODE_SLASH, KeyboardKeyCode::eSlash },
			{ AKEYCODE_0, KeyboardKeyCode::e0 },
			{ AKEYCODE_1, KeyboardKeyCode::e1 },
			{ AKEYCODE_2, KeyboardKeyCode::e2 },
			{ AKEYCODE_3, KeyboardKeyCode::e3 },
			{ AKEYCODE_4, KeyboardKeyCode::e4 },
			{ AKEYCODE_5, KeyboardKeyCode::e5 },
			{ AKEYCODE_6, KeyboardKeyCode::e6 },
			{ AKEYCODE_7, KeyboardKeyCode::e7 },
			{ AKEYCODE_8, KeyboardKeyCode::e8 },
			{ AKEYCODE_9, KeyboardKeyCode::e9 },
			{ AKEYCODE_SEMICOLON, KeyboardKeyCode::eSemicolon },
			{ AKEYCODE_EQUALS, KeyboardKeyCode::eEq },
			{ AKEYCODE_A, KeyboardKeyCode::eA },
			{ AKEYCODE_B, KeyboardKeyCode::eB },
			{ AKEYCODE_C, KeyboardKeyCode::eC },
			{ AKEYCODE_D, KeyboardKeyCode::eD },
			{ AKEYCODE_E, KeyboardKeyCode::eE },
			{ AKEYCODE_F, KeyboardKeyCode::eF },
			{ AKEYCODE_G, KeyboardKeyCode::eG },
			{ AKEYCODE_H, KeyboardKeyCode::eH },
			{ AKEYCODE_I, KeyboardKeyCode::eI },
			{ AKEYCODE_J, KeyboardKeyCode::eJ },
			{ AKEYCODE_K, KeyboardKeyCode::eK },
			{ AKEYCODE_L, KeyboardKeyCode::eL },
			{ AKEYCODE_M, KeyboardKeyCode::eM },
			{ AKEYCODE_N, KeyboardKeyCode::eN },
			{ AKEYCODE_O, KeyboardKeyCode::eO },
			{ AKEYCODE_P, KeyboardKeyCode::eP },
			{ AKEYCODE_Q, KeyboardKeyCode::eQ },
			{ AKEYCODE_R, KeyboardKeyCode::eR },
			{ AKEYCODE_S, KeyboardKeyCode::eS },
			{ AKEYCODE_T, KeyboardKeyCode::eT },
			{ AKEYCODE_U, KeyboardKeyCode::eU },
			{ AKEYCODE_V, KeyboardKeyCode::eV },
			{ AKEYCODE_W, KeyboardKeyCode::eW },
			{ AKEYCODE_X, KeyboardKeyCode::eX },
			{ AKEYCODE_Y, KeyboardKeyCode::eY },
			{ AKEYCODE_Z, KeyboardKeyCode::eZ },
			{ AKEYCODE_LEFT_BRACKET, KeyboardKeyCode::eLeftBracket },
			{ AKEYCODE_BACKSLASH, KeyboardKeyCode::eBackslash },
			{ AKEYCODE_RIGHT_BRACKET, KeyboardKeyCode::eRightBracket },
			{ AKEYCODE_ESCAPE, KeyboardKeyCode::eEsc },
			{ AKEYCODE_ENTER, KeyboardKeyCode::eEnter },
			{ AKEYCODE_TAB, KeyboardKeyCode::eTab },
			{ AKEYCODE_DEL, KeyboardKeyCode::eBackspace },
			{ AKEYCODE_INSERT, KeyboardKeyCode::eInsert },
			{ AKEYCODE_DEL, KeyboardKeyCode::eDel },
			{ AKEYCODE_SYSTEM_NAVIGATION_RIGHT, KeyboardKeyCode::eRight },
			{ AKEYCODE_SYSTEM_NAVIGATION_LEFT, KeyboardKeyCode::eLeft },
			{ AKEYCODE_SYSTEM_NAVIGATION_DOWN, KeyboardKeyCode::eDown },
			{ AKEYCODE_SYSTEM_NAVIGATION_UP, KeyboardKeyCode::eUp },
			{ AKEYCODE_PAGE_UP, KeyboardKeyCode::ePageUp },
			{ AKEYCODE_PAGE_DOWN, KeyboardKeyCode::ePageDown },
			{ AKEYCODE_MOVE_HOME, KeyboardKeyCode::eHome },
			{ AKEYCODE_MOVE_END, KeyboardKeyCode::eEnd },
			{ AKEYCODE_CAPS_LOCK, KeyboardKeyCode::eCapsLock },
			{ AKEYCODE_SCROLL_LOCK, KeyboardKeyCode::eScrollLock },
			{ AKEYCODE_NUM_LOCK, KeyboardKeyCode::eNumLock },
			{ AKEYCODE_SYSRQ, KeyboardKeyCode::ePrintScreen },
			{ AKEYCODE_MEDIA_PAUSE, KeyboardKeyCode::ePause },
			{ AKEYCODE_F1, KeyboardKeyCode::eF1 },
			{ AKEYCODE_F2, KeyboardKeyCode::eF2 },
			{ AKEYCODE_F3, KeyboardKeyCode::eF3 },
			{ AKEYCODE_F4, KeyboardKeyCode::eF4 },
			{ AKEYCODE_F5, KeyboardKeyCode::eF5 },
			{ AKEYCODE_F6, KeyboardKeyCode::eF6 },
			{ AKEYCODE_F7, KeyboardKeyCode::eF7 },
			{ AKEYCODE_F8, KeyboardKeyCode::eF8 },
			{ AKEYCODE_F9, KeyboardKeyCode::eF9 },
			{ AKEYCODE_F10, KeyboardKeyCode::eF10 },
			{ AKEYCODE_F11, KeyboardKeyCode::eF11 },
			{ AKEYCODE_F12, KeyboardKeyCode::eF12 },
			{ AKEYCODE_NUMPAD_0, KeyboardKeyCode::eKp0 },
			{ AKEYCODE_NUMPAD_1, KeyboardKeyCode::eKp1 },
			{ AKEYCODE_NUMPAD_2, KeyboardKeyCode::eKp2 },
			{ AKEYCODE_NUMPAD_3, KeyboardKeyCode::eKp3 },
			{ AKEYCODE_NUMPAD_4, KeyboardKeyCode::eKp4 },
			{ AKEYCODE_NUMPAD_5, KeyboardKeyCode::eKp5 },
			{ AKEYCODE_NUMPAD_6, KeyboardKeyCode::eKp6 },
			{ AKEYCODE_NUMPAD_7, KeyboardKeyCode::eKp7 },
			{ AKEYCODE_NUMPAD_8, KeyboardKeyCode::eKp8 },
			{ AKEYCODE_NUMPAD_9, KeyboardKeyCode::eKp9 },
			{ AKEYCODE_NUMPAD_DOT, KeyboardKeyCode::eKpDec },
			{ AKEYCODE_NUMPAD_DIVIDE, KeyboardKeyCode::eKpDiv },
			{ AKEYCODE_NUMPAD_MULTIPLY, KeyboardKeyCode::eKpMul },
			{ AKEYCODE_NUMPAD_SUBTRACT, KeyboardKeyCode::eKpSub },
			{ AKEYCODE_NUMPAD_ADD, KeyboardKeyCode::eKpAdd },
			{ AKEYCODE_NUMPAD_ENTER, KeyboardKeyCode::eKpEnter },
			{ AKEYCODE_NUMPAD_EQUALS, KeyboardKeyCode::eKpEq },
			{ AKEYCODE_SHIFT_LEFT, KeyboardKeyCode::eLeftShift },
			{ AKEYCODE_CTRL_LEFT, KeyboardKeyCode::eLeftControl },
			{ AKEYCODE_ALT_LEFT, KeyboardKeyCode::eLeftAlt },
			{ AKEYCODE_SHIFT_RIGHT, KeyboardKeyCode::eRightShift },
			{ AKEYCODE_CTRL_RIGHT, KeyboardKeyCode::eRightControl },
			{ AKEYCODE_ALT_RIGHT, KeyboardKeyCode::eRightAlt },
			{ AKEYCODE_MENU, KeyboardKeyCode::eMenu }
			});

		if (auto found = lut.find(key); found != lut.end()) {
			return found->second;
		}

		return KeyboardKeyCode::eUnknown;
	}

    auto AndroidPlatformWindow::construct(PlatformContextInterface* platform_context) -> std::unique_ptr<AndroidPlatformWindow> {
        auto window = std::make_unique<AndroidPlatformWindow>();
        auto& android_platform_context = static_cast<AndroidPlatformContext&>(*platform_context);
        window->android_app_ = android_platform_context.get_android_app();
        window->event_dispatcher_ = &platform_context->get_event_dispatcher();
        return window;
    }

    auto AndroidPlatformWindow::create(const window::Properties& props) -> bool {
        return true;
    }

    auto AndroidPlatformWindow::destroy() -> void {
        GameActivity_finish(android_app_->activity);
        requested_close_ = true;
    }

    auto AndroidPlatformWindow::poll_events() -> void {
        android_poll_source* source;

        int ident;
        int events;

        while ((ident = ALooper_pollOnce(0, nullptr, &events, (void**)&source)) > ALOOPER_POLL_TIMEOUT) {
            if (source) {
                source->process(android_app_, source);
            }

            if (android_app_->destroyRequested != 0) {
                event_dispatcher_->emit(events::WindowShouldCloseEvent{
                        .window_id = ~0ull
                });
            }
            requested_close_ = true;
        }

        // Process input
        auto input_buf = android_app_swap_input_buffers(android_app_);
        if(!input_buf)
            return;

        if (input_buf->motionEventsCount) {
            for (int idx = 0; idx < input_buf->motionEventsCount; idx++) {
                auto event = &input_buf->motionEvents[idx];
                assert((event->source == AINPUT_SOURCE_MOUSE || event->source == AINPUT_SOURCE_TOUCHSCREEN) &&
                       "Invalid motion event source");

                if (event->source == AINPUT_SOURCE_MOUSE) {
                    // TODO: Not Implemented
                    //float x = GameActivityPointerAxes_getX(&event->pointers[0]);
                    //float y = GameActivityPointerAxes_getY(&event->pointers[0]);
                }
                else if (event->source == AINPUT_SOURCE_TOUCHSCREEN) {
                    // TODO: Not Implemented
                    for(int32_t i = 0; i < event->pointerCount; ++i) {

                    }
                }
            }
            android_app_clear_motion_events(input_buf);
        }

        if (input_buf->keyEventsCount) {
            for (int idx = 0; idx < input_buf->keyEventsCount; idx++) {
                auto event = &input_buf->keyEvents[idx];
                assert((event->source == AINPUT_SOURCE_KEYBOARD) && "Invalid key event source");
                event_dispatcher_->emit(events::KeyEvent{
                        .key_code = translate_keyboard_key_code(event->keyCode),
                        .key_action = trnaslate_key_action(event->action),
                        .window_id = ~0ull
                });
            }
            android_app_clear_key_events(input_buf);
        }
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
}