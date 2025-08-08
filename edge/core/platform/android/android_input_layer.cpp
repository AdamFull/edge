#include "platform.h"

#include <cassert>
#include <unordered_map>

#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <paddleboat/paddleboat.h>

#include "jni_helper.h"

namespace edge::platform {
    static std::array<Paddleboat_Controller_Data, PADDLEBOAT_MAX_CONTROLLERS> gamepad_last_state_{};

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

    inline auto translate_gamepad_key_code(int32_t key) -> GamepadKeyCode {
        static std::array<GamepadKeyCode, PADDLEBOAT_BUTTON_COUNT> lut {
                GamepadKeyCode::eButtonDPadUp,
                GamepadKeyCode::eButtonDPadLeft,
                GamepadKeyCode::eButtonDPadDown,
                GamepadKeyCode::eButtonDPadRight,
                GamepadKeyCode::eButtonA,
                GamepadKeyCode::eButtonB,
                GamepadKeyCode::eButtonX,
                GamepadKeyCode::eButtonY,
                GamepadKeyCode::eButtonLeftBumper,
                GamepadKeyCode::eButtonLeftTrigger,
                GamepadKeyCode::eButtonLeftThumb,
                GamepadKeyCode::eButtonRightBumper,
                GamepadKeyCode::eButtonRightTrigger,
                GamepadKeyCode::eButtonRightThumb,
                GamepadKeyCode::eButtonBack,
                GamepadKeyCode::eButtonStart,
                GamepadKeyCode::eButtonGuide,
                GamepadKeyCode::eUnknown, // Touchpad
                GamepadKeyCode::eUnknown, // Aux1
                GamepadKeyCode::eUnknown, // Aux2
                GamepadKeyCode::eUnknown, // Aux3
                GamepadKeyCode::eUnknown, // Aux4
        };

        return lut[key];
    }

    auto InputLayer::construct(android_app* app, PlatformContextInterface* platform_context) -> std::unique_ptr<InputLayer> {
        auto self = std::make_unique<InputLayer>();
        self->android_app_ = app;
        self->platform_context_ = platform_context;
        return self;
    }

    auto InputLayer::initialize() -> bool {
        jni_env_ = get_jni_env(android_app_);

        Paddleboat_ErrorCode result = Paddleboat_init(jni_env_, android_app_->activity->javaGameActivity);
        if (result != PADDLEBOAT_NO_ERROR) {
            // TODO: LOG ERROR
            return false;
        }

        Paddleboat_setMotionDataCallbackWithIntegratedFlags([](const int32_t controller_index, const Paddleboat_Motion_Data *motion_data, void *user_data) -> void {
            auto* input_layer = static_cast<InputLayer*>(user_data);
            input_layer->process_controller_motion_data(controller_index, motion_data);
        }, Paddleboat_getIntegratedMotionSensorFlags(), this);

        Paddleboat_setControllerStatusCallback([](const int32_t controller_index, const Paddleboat_ControllerStatus controller_status, void *user_data) -> void {
            auto* input_layer = static_cast<InputLayer*>(user_data);
            input_layer->process_controller_state_changes(controller_index, controller_status);
            }, this);

        return true;
    }

    auto InputLayer::shutdown() -> void {
        Paddleboat_setMotionDataCallback(nullptr, nullptr);
        Paddleboat_setControllerStatusCallback(nullptr, nullptr);
        Paddleboat_destroy(jni_env_);
    }

    auto InputLayer::update() -> void {
        auto& dispatcher = platform_context_->get_event_dispatcher();

        jni_env_ = get_jni_env(android_app_);
        Paddleboat_update(jni_env_);

        // Process input
        auto input_buf = android_app_swap_input_buffers(android_app_);
        if(!input_buf)
            return;

        if (input_buf->motionEventsCount) {
            for (int idx = 0; idx < input_buf->motionEventsCount; idx++) {
                auto event = &input_buf->motionEvents[idx];
                assert((event->source == AINPUT_SOURCE_MOUSE || event->source == AINPUT_SOURCE_TOUCHSCREEN) &&
                       "Invalid motion event source");

                process_motion_event(event);
            }
            android_app_clear_motion_events(input_buf);
        }

        if (input_buf->keyEventsCount) {
            for (int idx = 0; idx < input_buf->keyEventsCount; idx++) {
                auto event = &input_buf->keyEvents[idx];
                assert((event->source == AINPUT_SOURCE_KEYBOARD) && "Invalid key event source");
                process_key_event(event);
            }
            android_app_clear_key_events(input_buf);
        }

        // Update all controllers available
        for (int32_t jid = 0; jid < PADDLEBOAT_MAX_CONTROLLERS; ++jid) {
            Paddleboat_ControllerStatus status = Paddleboat_getControllerStatus(jid);
            if (status == PADDLEBOAT_CONTROLLER_ACTIVE) {
                Paddleboat_Controller_Data controller_data;
                if (Paddleboat_getControllerData(jid, &controller_data) == PADDLEBOAT_NO_ERROR) {
                    auto& last_state = gamepad_last_state_[jid];

                    // Update gamepad buttons
                    for(int32_t button_idx = 0; button_idx < PADDLEBOAT_BUTTON_COUNT; ++button_idx) {
                        auto controller_button = static_cast<Paddleboat_Buttons>(1 << button_idx);

                        bool curr = (controller_data.buttonsDown & controller_button) == controller_button;
                        bool prev = (last_state.buttonsDown & controller_button) == controller_button;

                        KeyAction key_action = KeyAction::eUnknown;
                        if (curr && !prev) {
                            key_action = KeyAction::ePress;
                        }
                        else if (curr && prev) {
                            key_action = KeyAction::eHold;
                        }
                        else if (!curr && prev) {
                            key_action = KeyAction::eRelease;
                        }

                        if (key_action != KeyAction::eUnknown) {
                            dispatcher.emit(events::GamepadButtonEvent{
                                    .gamepad_id = jid,
                                    .key_code = translate_gamepad_key_code(button_idx),
                                    .key_action = key_action
                            });
                        }
                    }

                    // Update gamepad axis
                    if(controller_data.leftStick.stickX != last_state.leftStick.stickX ||
                            controller_data.leftStick.stickY != last_state.leftStick.stickY) {
                        dispatcher.emit(events::GamepadAxisEvent{
                                .gamepad_id = jid,
                                .values = { controller_data.leftStick.stickX, controller_data.leftStick.stickY },
                                .axis_code = GamepadAxisCode::eLeftStick
                        });
                    }

                    if(controller_data.rightStick.stickX != last_state.rightStick.stickX ||
                            controller_data.rightStick.stickY != last_state.rightStick.stickY) {
                        dispatcher.emit(events::GamepadAxisEvent{
                                .gamepad_id = jid,
                                .values = { controller_data.rightStick.stickX, controller_data.rightStick.stickY },
                                .axis_code = GamepadAxisCode::eRightStick
                        });
                    }

                    if(controller_data.triggerL2 > last_state.triggerL2) {
                        dispatcher.emit(events::GamepadAxisEvent{
                                .gamepad_id = jid,
                                .values = { controller_data.triggerL2 },
                                .axis_code = GamepadAxisCode::eLeftTrigger
                        });
                    }

                    if(controller_data.triggerR2 > last_state.triggerR2) {
                        dispatcher.emit(events::GamepadAxisEvent{
                                .gamepad_id = jid,
                                .values = { controller_data.triggerR2 },
                                .axis_code = GamepadAxisCode::eRightTrigger
                        });
                    }

                    gamepad_last_state_[jid] = controller_data;
                }
            }
        }
    }

    auto InputLayer::process_motion_event(GameActivityMotionEvent* event) -> void {
        if(Paddleboat_processGameActivityMotionInputEvent(event, sizeof(GameActivityMotionEvent)) != 0) {
            return;
        }

        // Process other events
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

    auto InputLayer::process_key_event(GameActivityKeyEvent* event) -> void {
        if(Paddleboat_processGameActivityKeyInputEvent(event, sizeof(GameActivityKeyEvent)) != 0) {
            return;
        }

        auto& dispatcher = platform_context_->get_event_dispatcher();

        // Process other events
        dispatcher.emit(events::KeyEvent{
                .key_code = translate_keyboard_key_code(event->keyCode),
                .key_action = trnaslate_key_action(event->action),
                .window_id = ~0ull
        });
    }

    auto InputLayer::on_app_start() -> void {
        Paddleboat_onStart(jni_env_);
    }

    auto InputLayer::on_app_stop() -> void {
        Paddleboat_onStop(jni_env_);
    }

    auto InputLayer::process_controller_motion_data(const int32_t controller_index, const void* motion_data) -> void {
        if(!motion_data) {
            return;
        }

        auto& dispatcher = platform_context_->get_event_dispatcher();
        auto* data = static_cast<const Paddleboat_Motion_Data*>(motion_data);
        switch (data->motionType) {
            case PADDLEBOAT_MOTION_ACCELEROMETER: {
                dispatcher.emit(events::GamepadAxisEvent{
                        .gamepad_id = controller_index,
                        .values = { data->motionX, data->motionY, data->motionZ },
                        .axis_code = GamepadAxisCode::eAccel
                });
                break;
            }
            case PADDLEBOAT_MOTION_GYROSCOPE: {
                dispatcher.emit(events::GamepadAxisEvent{
                        .gamepad_id = controller_index,
                        .values = { data->motionX, data->motionY, data->motionZ },
                        .axis_code = GamepadAxisCode::eGyro
                });
                break;
            }
        }
    }

    auto InputLayer::process_controller_state_changes(const int32_t controller_index, const uint32_t controller_status) -> void {
        auto& dispatcher = platform_context_->get_event_dispatcher();
        bool is_just_connected = controller_status == PADDLEBOAT_CONTROLLER_JUST_CONNECTED;
        bool is_just_disconnected = controller_status == PADDLEBOAT_CONTROLLER_JUST_DISCONNECTED;
        if(is_just_connected || is_just_disconnected) {
            char controller_name[256];
            Paddleboat_ErrorCode result = Paddleboat_getControllerName(controller_index, sizeof(controller_name), controller_name);
            if (result == PADDLEBOAT_NO_ERROR) {
                Paddleboat_Controller_Info controller_info;
                if (Paddleboat_getControllerInfo(controller_index, &controller_info) == PADDLEBOAT_NO_ERROR) {
                    // TODO: Can be used for provide additional info
                }

                dispatcher.emit(events::GamepadConnectionEvent{
                        .gamepad_id = controller_index,
                        .connected = is_just_connected,
                        .name = controller_name
                });
            }
        }
    }
}