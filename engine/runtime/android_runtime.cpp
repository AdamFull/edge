#include "runtime.h"
#include "input_system.h"

#include <logger.hpp>

#include <paddleboat/paddleboat.h>
#include <game-text-input/gametextinput.h>
#include <game-activity/native_app_glue/android_native_app_glue.h>

#include <vulkan/vulkan.h>

extern int edge_main(edge::RuntimeLayout* runtime_layout);

namespace edge {
    struct RuntimeLayout {
        struct android_app* app = nullptr;
    };

    static JNIEnv* get_jni_env(struct android_app* app) {
        JNIEnv* env = nullptr;
        jint get_env_result = app->activity->vm->GetEnv((void**)&env, JNI_VERSION_1_6);
        if (get_env_result == JNI_EDETACHED) {
            if (app->activity->vm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
                return nullptr;
            }
        }

        return env;
    }

    void on_app_cmd_cb(struct android_app* app, i32 cmd);

    Key android_keycode_to_engine_key(i32 keycode) {
        switch (keycode) {
            case AKEYCODE_SPACE: return Key::Space;
            case AKEYCODE_APOSTROPHE: return Key::Apostrophe;
            case AKEYCODE_COMMA: return Key::Comma;
            case AKEYCODE_MINUS: return Key::Minus;
            case AKEYCODE_PERIOD: return Key::Period;
            case AKEYCODE_SLASH: return Key::Slash;
            case AKEYCODE_0: return Key::_0;
            case AKEYCODE_1: return Key::_1;
            case AKEYCODE_2: return Key::_2;
            case AKEYCODE_3: return Key::_3;
            case AKEYCODE_4: return Key::_4;
            case AKEYCODE_5: return Key::_5;
            case AKEYCODE_6: return Key::_6;
            case AKEYCODE_7: return Key::_7;
            case AKEYCODE_8: return Key::_8;
            case AKEYCODE_9: return Key::_9;
            case AKEYCODE_SEMICOLON: return Key::Semicolon;
            case AKEYCODE_EQUALS: return Key::Eq;
            case AKEYCODE_A: return Key::A;
            case AKEYCODE_B: return Key::B;
            case AKEYCODE_C: return Key::C;
            case AKEYCODE_D: return Key::D;
            case AKEYCODE_E: return Key::E;
            case AKEYCODE_F: return Key::F;
            case AKEYCODE_G: return Key::G;
            case AKEYCODE_H: return Key::H;
            case AKEYCODE_I: return Key::I;
            case AKEYCODE_J: return Key::J;
            case AKEYCODE_K: return Key::K;
            case AKEYCODE_L: return Key::L;
            case AKEYCODE_M: return Key::M;
            case AKEYCODE_N: return Key::N;
            case AKEYCODE_O: return Key::O;
            case AKEYCODE_P: return Key::P;
            case AKEYCODE_Q: return Key::Q;
            case AKEYCODE_R: return Key::R;
            case AKEYCODE_S: return Key::S;
            case AKEYCODE_T: return Key::T;
            case AKEYCODE_U: return Key::U;
            case AKEYCODE_V: return Key::V;
            case AKEYCODE_W: return Key::W;
            case AKEYCODE_X: return Key::X;
            case AKEYCODE_Y: return Key::Y;
            case AKEYCODE_Z: return Key::Z;
            case AKEYCODE_LEFT_BRACKET: return Key::LeftBracket;
            case AKEYCODE_BACKSLASH: return Key::Backslash;
            case AKEYCODE_RIGHT_BRACKET: return Key::RightBracket;
            case AKEYCODE_GRAVE: return Key::GraveAccent;
            case AKEYCODE_ESCAPE: return Key::Esc;
            case AKEYCODE_ENTER: return Key::Enter;
            case AKEYCODE_TAB: return Key::Tab;
            case AKEYCODE_DEL: return Key::Backspace;
            case AKEYCODE_INSERT: return Key::Insert;
            case AKEYCODE_FORWARD_DEL: return Key::Del;
            case AKEYCODE_DPAD_RIGHT: return Key::Right;
            case AKEYCODE_DPAD_LEFT: return Key::Left;
            case AKEYCODE_DPAD_DOWN: return Key::Down;
            case AKEYCODE_DPAD_UP: return Key::Up;
            case AKEYCODE_PAGE_UP: return Key::PageUp;
            case AKEYCODE_PAGE_DOWN: return Key::PageDown;
            case AKEYCODE_MOVE_HOME: return Key::Home;
            case AKEYCODE_MOVE_END: return Key::End;
            case AKEYCODE_CAPS_LOCK: return Key::CapsLock;
            case AKEYCODE_SCROLL_LOCK: return Key::ScrollLock;
            case AKEYCODE_NUM_LOCK: return Key::NumLock;
            case AKEYCODE_SYSRQ: return Key::PrintScreen;
            case AKEYCODE_BREAK: return Key::Pause;
            case AKEYCODE_F1: return Key::F1;
            case AKEYCODE_F2: return Key::F2;
            case AKEYCODE_F3: return Key::F3;
            case AKEYCODE_F4: return Key::F4;
            case AKEYCODE_F5: return Key::F5;
            case AKEYCODE_F6: return Key::F6;
            case AKEYCODE_F7: return Key::F7;
            case AKEYCODE_F8: return Key::F8;
            case AKEYCODE_F9: return Key::F9;
            case AKEYCODE_F10: return Key::F10;
            case AKEYCODE_F11: return Key::F11;
            case AKEYCODE_F12: return Key::F12;
            case AKEYCODE_NUMPAD_0: return Key::Kp0;
            case AKEYCODE_NUMPAD_1: return Key::Kp1;
            case AKEYCODE_NUMPAD_2: return Key::Kp2;
            case AKEYCODE_NUMPAD_3: return Key::Kp3;
            case AKEYCODE_NUMPAD_4: return Key::Kp4;
            case AKEYCODE_NUMPAD_5: return Key::Kp5;
            case AKEYCODE_NUMPAD_6: return Key::Kp6;
            case AKEYCODE_NUMPAD_7: return Key::Kp7;
            case AKEYCODE_NUMPAD_8: return Key::Kp8;
            case AKEYCODE_NUMPAD_9: return Key::Kp9;
            case AKEYCODE_NUMPAD_DOT: return Key::KpDec;
            case AKEYCODE_NUMPAD_DIVIDE: return Key::KpDiv;
            case AKEYCODE_NUMPAD_MULTIPLY: return Key::KpMul;
            case AKEYCODE_NUMPAD_SUBTRACT: return Key::KpSub;
            case AKEYCODE_NUMPAD_ADD: return Key::KpAdd;
            case AKEYCODE_NUMPAD_ENTER: return Key::KpEnter;
            case AKEYCODE_NUMPAD_EQUALS: return Key::KpEq;
            case AKEYCODE_SHIFT_LEFT: return Key::LeftShift;
            case AKEYCODE_CTRL_LEFT: return Key::LeftControl;
            case AKEYCODE_ALT_LEFT: return Key::LeftAlt;
            case AKEYCODE_META_LEFT: return Key::LeftSuper;
            case AKEYCODE_SHIFT_RIGHT: return Key::RightShift;
            case AKEYCODE_CTRL_RIGHT: return Key::RightControl;
            case AKEYCODE_ALT_RIGHT: return Key::RightAlt;
            case AKEYCODE_META_RIGHT: return Key::RightSuper;
            case AKEYCODE_MENU: return Key::Menu;
            default: return Key::Unknown;
        }
    }

    PadBtn paddleboat_button_to_engine_btn(Paddleboat_Buttons button) {
        switch (button) {
            case PADDLEBOAT_BUTTON_A: return PadBtn::A;
            case PADDLEBOAT_BUTTON_B: return PadBtn::B;
            case PADDLEBOAT_BUTTON_X: return PadBtn::X;
            case PADDLEBOAT_BUTTON_Y: return PadBtn::Y;
            case PADDLEBOAT_BUTTON_L1: return PadBtn::BumperLeft;
            case PADDLEBOAT_BUTTON_L2: return PadBtn::TriggerLeft;
            case PADDLEBOAT_BUTTON_R1: return PadBtn::BumperRight;
            case PADDLEBOAT_BUTTON_R2: return PadBtn::TriggerRight;
            case PADDLEBOAT_BUTTON_SELECT: return PadBtn::Back;
            case PADDLEBOAT_BUTTON_START: return PadBtn::Start;
            case PADDLEBOAT_BUTTON_SYSTEM: return PadBtn::Guide;
            case PADDLEBOAT_BUTTON_L3: return PadBtn::ThumbLeft;
            case PADDLEBOAT_BUTTON_R3: return PadBtn::ThumbRight;
            case PADDLEBOAT_BUTTON_DPAD_UP: return PadBtn::DpadUp;
            case PADDLEBOAT_BUTTON_DPAD_RIGHT: return PadBtn::DpadRight;
            case PADDLEBOAT_BUTTON_DPAD_DOWN: return PadBtn::DpadDown;
            case PADDLEBOAT_BUTTON_DPAD_LEFT: return PadBtn::DpadLeft;
            default: return PadBtn::Count;
        }
    }

    MouseBtn paddleboat_mouse_button_to_engine_btn(Paddleboat_Mouse_Buttons button) {
        switch (button) {
            case PADDLEBOAT_MOUSE_BUTTON_LEFT: return MouseBtn::Left;
            case PADDLEBOAT_MOUSE_BUTTON_RIGHT: return MouseBtn::Right;
            case PADDLEBOAT_MOUSE_BUTTON_MIDDLE: return MouseBtn::Middle;
            case PADDLEBOAT_MOUSE_BUTTON_BACK: return MouseBtn::_4;
            case PADDLEBOAT_MOUSE_BUTTON_FORWARD: return MouseBtn::_5;
            default: return MouseBtn::Count;
        }
    }

    f32 apply_deadzone(f32 value, f32 deadzone) {
        if (abs(value) < deadzone) {
            return 0.0f;
        }

        f32 sign = value < 0.0f ? -1.0f : 1.0f;
        f32 abs_val = abs(value);
        return sign * ((abs_val - deadzone) / (1.0f - deadzone));
    }

    struct AndroidRuntime final : IRuntime {
        bool init(const RuntimeInitInfo& init_info) noexcept override {
            layout = init_info.layout;
            input_system = init_info.input_system;

            layout->app->onAppCmd = on_app_cmd_cb;
            layout->app->userData = this;

#if 0
            Paddleboat_setMotionDataCallbackWithIntegratedFlags(motion_data_cb, Paddleboat_getIntegratedMotionSensorFlags(), this);
            Paddleboat_setControllerStatusCallback(controller_status_cb, this);
            Paddleboat_setMouseStatusCallback(mouse_status_cb, this);
            Paddleboat_setPhysicalKeyboardStatusCallback(keyboard_status_cb, this);
#endif

            JNIEnv* jni_env = get_jni_env(layout->app);
            if(!jni_env) {
                return false;
            }

            Paddleboat_ErrorCode result = Paddleboat_init(jni_env, layout->app->activity->javaGameActivity);
            if (result != PADDLEBOAT_NO_ERROR) {
                EDGE_LOG_ERROR("Failed to initialize Paddleboat: %d", (i32)result);
                return false;
            }

            if (!Paddleboat_isInitialized()) {
                EDGE_LOG_ERROR("Paddleboat initialization verification failed");
                return false;
            }

            while(!surface_ready) {
                process_events();
            }

            return true;
        }

        void deinit(NotNull<const Allocator*> alloc) noexcept override {
            if (Paddleboat_isInitialized()) {
                Paddleboat_setControllerStatusCallback(nullptr, nullptr);
                Paddleboat_setMouseStatusCallback(nullptr, nullptr);
                Paddleboat_setPhysicalKeyboardStatusCallback(nullptr, nullptr);
                Paddleboat_setMotionDataCallback(nullptr, nullptr);

                JNIEnv* jni_env = get_jni_env(layout->app);
                Paddleboat_destroy(jni_env);
            }

            GameActivity_finish(layout->app->activity);
            should_close = true;
        }

        bool requested_close() const noexcept override {
            return should_close;
        }

        void process_events() noexcept override {
            struct android_app* app = layout->app;
            struct android_poll_source* source;
            i32 ident, events;

            while ((ident = ALooper_pollOnce(0, nullptr, &events, (void**)&source)) > ALOOPER_POLL_TIMEOUT) {
                if (source) {
                    source->process(app, source);
                }

                if (app->destroyRequested != 0) {
                    EDGE_LOG_DEBUG("Requested window destroy.");
                    should_close = true;
                }
            }

            JNIEnv* jni_env = get_jni_env(layout->app);

            struct android_input_buffer* input_buf = android_app_swap_input_buffers(app);
            if (!input_buf) {
                return;
            }

            KeyboardDevice* keyboard = input_system->get_keyboard();
            MouseDevice* mouse = input_system->get_mouse();
            TouchDevice* touch = input_system->get_touch();

            if (input_buf->motionEventsCount) {
                for (i32 idx = 0; idx < input_buf->motionEventsCount; idx++) {
                    GameActivityMotionEvent* event = &input_buf->motionEvents[idx];
                    assert((event->source == AINPUT_SOURCE_MOUSE || event->source == AINPUT_SOURCE_TOUCHSCREEN) &&
                        "Invalid motion event source");

                    if (Paddleboat_processGameActivityMotionInputEvent(event, sizeof(GameActivityMotionEvent)) != 0) {
                        continue;
                    }

                    // Process other events
                    if (event->source == AINPUT_SOURCE_TOUCHSCREEN) {
                        if (event->pointerCount > 0) {
                            // TODO: Update touches. Mouse emulation is temporary

                            GameActivityPointerAxes& pointer = event->pointers[0];
                            f32 x = GameActivityPointerAxes_getX(&pointer);
                            f32 y = GameActivityPointerAxes_getY(&pointer);

                            mouse->set_position(x, y);

                            // Map touch press/release to left mouse button
                            if (event->action == AMOTION_EVENT_ACTION_DOWN) {
                                mouse->state.cur.set(static_cast<usize>(MouseBtn::Left));
                            } else if (event->action == AMOTION_EVENT_ACTION_UP) {
                                mouse->state.cur.clear(static_cast<usize>(MouseBtn::Left));
                            }
                        }
                    }
                }
                android_app_clear_motion_events(input_buf);
            }

            if (input_buf->keyEventsCount) {
                for (i32 idx = 0; idx < input_buf->keyEventsCount; idx++) {
                    GameActivityKeyEvent* event = &input_buf->keyEvents[idx];
                    assert((event->source == AINPUT_SOURCE_KEYBOARD) && "Invalid key event source");

                    if (Paddleboat_processGameActivityKeyInputEvent(event, sizeof(GameActivityKeyEvent)) != 0) {
                        return;
                    }

                    if (event->action == AKEY_STATE_VIRTUAL) {
                        return;
                    }

                    Key engine_key = android_keycode_to_engine_key(event->keyCode);
                    if (engine_key != Key::Unknown) {
                        bool pressed = (event->action == AKEY_EVENT_ACTION_DOWN);
                        keyboard->set_key(engine_key, pressed);
                    }


                }
                android_app_clear_key_events(input_buf);
            }

            if (!jni_env) {
                return;
            }

            Paddleboat_update(jni_env);

            // Process mouse
            Paddleboat_Mouse_Data mouse_data;
            if (Paddleboat_getMouseData(&mouse_data) == PADDLEBOAT_NO_ERROR) {
                mouse->set_position(mouse_data.mouseX, mouse_data.mouseY);
                mouse->set_scroll((f32)mouse_data.mouseScrollDeltaH, (f32)mouse_data.mouseScrollDeltaV);

                // Process mouse buttons
                for (i32 button_idx = 0; button_idx < 8; ++button_idx) {
                    auto paddle_button = (Paddleboat_Mouse_Buttons)(1 << button_idx);
                    MouseBtn engine_btn = paddleboat_mouse_button_to_engine_btn(paddle_button);

                    if (engine_btn != MouseBtn::Count) {
                        bool pressed = (mouse_data.buttonsDown & paddle_button) != 0;
                        mouse->set_button(engine_btn, pressed);
                    }
                }
            }

            // Update all controllers available
            for (i32 controller_idx = 0; controller_idx < PADDLEBOAT_MAX_CONTROLLERS; ++controller_idx) {
                if (controller_idx >= MAX_GAMEPADS) {
                    break;
                }

                Paddleboat_ControllerStatus status = Paddleboat_getControllerStatus(controller_idx);
                PadDevice* pad = input_system->get_gamepad(controller_idx);

                if (status == PADDLEBOAT_CONTROLLER_ACTIVE) {
                    if (!pad->state.connected) {
                        // Newly connected
                        pad->state.connected = true;

                        Paddleboat_Controller_Info info;
                        if (Paddleboat_getControllerInfo(controller_idx, &info) == PADDLEBOAT_NO_ERROR) {
                            pad->state.vendor_id = info.vendorId;
                            pad->state.product_id = info.productId;
                        }
                    }

                    Paddleboat_Controller_Data controller_data;
                    if (Paddleboat_getControllerData(controller_idx, &controller_data) == PADDLEBOAT_NO_ERROR) {
                        // Update buttons
                        for (i32 button_idx = 0; button_idx < PADDLEBOAT_BUTTON_COUNT; ++button_idx) {
                            auto paddle_button = (Paddleboat_Buttons)(1 << button_idx);
                            PadBtn engine_btn = paddleboat_button_to_engine_btn(paddle_button);

                            if (engine_btn != PadBtn::Count) {
                                bool pressed = (controller_data.buttonsDown & paddle_button) != 0;
                                pad->set_button(engine_btn, pressed);
                            }
                        }

                        // Update axes with deadzone
                        f32 left_x = apply_deadzone(controller_data.leftStick.stickX, pad->stick_deadzone);
                        f32 left_y = apply_deadzone(controller_data.leftStick.stickY, pad->stick_deadzone);
                        f32 right_x = apply_deadzone(controller_data.rightStick.stickX, pad->stick_deadzone);
                        f32 right_y = apply_deadzone(controller_data.rightStick.stickY, pad->stick_deadzone);

                        // Paddleboat already normalizes triggers to 0-1
                        f32 trigger_left = apply_deadzone(controller_data.triggerL2, pad->trigger_deadzone);
                        f32 trigger_right = apply_deadzone(controller_data.triggerR2, pad->trigger_deadzone);

                        pad->set_axis(PadAxis::LeftX, left_x);
                        pad->set_axis(PadAxis::LeftY, left_y);
                        pad->set_axis(PadAxis::RightX, right_x);
                        pad->set_axis(PadAxis::RightY, right_y);
                        pad->set_axis(PadAxis::TriggerLeft, trigger_left);
                        pad->set_axis(PadAxis::TriggerRight, trigger_right);
                    }
                } else {
                    // Disconnected
                    if (pad->state.connected) {
                        pad->clear();
                    }
                }
            }
        }

        void get_surface(void* surface_info) const noexcept override {
            *(VkAndroidSurfaceCreateInfoKHR*)surface_info = {
                .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
                .window = layout->app->window
            };
        }

        void get_surface_extent(i32& width, i32& height) const noexcept override {
            struct android_app* app = layout->app;
            width = app->contentRect.right - app->contentRect.left;
            height = app->contentRect.bottom - app->contentRect.top;
        }

        [[nodiscard]] f32 get_surface_scale_factor() const noexcept override {
            return (f32)AConfiguration_getDensity(layout->app->config) / (f32)ACONFIGURATION_DENSITY_MEDIUM;
        }

        [[nodiscard]] bool is_focused() const noexcept override {
            return focused;
        }

        void set_title(const char* title) noexcept override {

        }

        RuntimeLayout* layout = nullptr;
        bool should_close = false;
        bool surface_ready = false;
        bool focused = true;
        InputSystem* input_system = nullptr;
    };

    void on_app_cmd_cb(struct android_app* app, i32 cmd) {
        auto* rt = (AndroidRuntime*)app->userData;

        switch (cmd) {
        case APP_CMD_INIT_WINDOW: {
            rt->surface_ready = true;
            break;
        }
        case APP_CMD_GAINED_FOCUS: {
            EDGE_LOG_DEBUG("Focus gained.");
            rt->focused = true;
            break;
        }
        case APP_CMD_LOST_FOCUS: {
            EDGE_LOG_DEBUG("Focus lost.");
            rt->focused = false;
            break;
        }
        case APP_CMD_START: {
            if (!Paddleboat_isInitialized()) {
                break;
            }

            JNIEnv* jni_env = get_jni_env(app);
            Paddleboat_onStart(jni_env);
            break;
        }
        case APP_CMD_STOP: {
            if (!Paddleboat_isInitialized()) {
                break;
            }

            JNIEnv* jni_env = get_jni_env(app);
            Paddleboat_onStop(jni_env);
            break;
        }
        default: {
            const char* cmd_lut[21] = {
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

            EDGE_LOG_DEBUG("Unhandled command: %s", cmd_lut[cmd]);
            break;
        }
        }
    }

    IRuntime* create_runtime(NotNull<const Allocator*> alloc) noexcept {
        return alloc->allocate<AndroidRuntime>();
    }
}

extern "C" void android_main(struct android_app* state) {
    edge::RuntimeLayout runtime_layout = {
            .app = state
    };
    edge_main(&runtime_layout);
}