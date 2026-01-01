#include "platform.h"
#include "input_events.h"
#include "window_events.h"

#include <cassert>

#include <allocator.hpp>
#include <logger.hpp>

#include <paddleboat/paddleboat.h>
#include <game-text-input/gametextinput.h>
#include <game-activity/native_app_glue/android_native_app_glue.h>

#include <vulkan/vulkan.h>

extern int edge_main(edge::PlatformLayout* platform_layout);

namespace edge {
    struct PlatformLayout {
        struct android_app* app = nullptr;
    };

    struct Window {
        PlatformContext* ctx = nullptr;

        WindowMode mode = WindowMode::Default;
        bool resizable = false;
        WindowVsyncMode vsync_mode = WindowVsyncMode::Default;

        bool should_close = false;
        bool surface_ready = false;

        InputState input_state = {};
    };

    struct PlatformContext {
        const Allocator* alloc = nullptr;
        PlatformLayout* layout = nullptr;
        EventDispatcher* event_dispatcher = nullptr;
        Window* wnd = nullptr;
    };

    static InputKeyboardKey pb_key_code_to_edge[] = {
            [AKEYCODE_SPACE] = InputKeyboardKey::Space,
            [AKEYCODE_APOSTROPHE] = InputKeyboardKey::Apostrophe,
            [AKEYCODE_COMMA] = InputKeyboardKey::Comma,
            [AKEYCODE_MINUS] = InputKeyboardKey::Minus,
            [AKEYCODE_PERIOD] = InputKeyboardKey::Period,
            [AKEYCODE_SLASH] = InputKeyboardKey::Slash,
            [AKEYCODE_0] = InputKeyboardKey::_0,
            [AKEYCODE_1] = InputKeyboardKey::_1,
            [AKEYCODE_2] = InputKeyboardKey::_2,
            [AKEYCODE_3] = InputKeyboardKey::_3,
            [AKEYCODE_4] = InputKeyboardKey::_4,
            [AKEYCODE_5] = InputKeyboardKey::_5,
            [AKEYCODE_6] = InputKeyboardKey::_6,
            [AKEYCODE_7] = InputKeyboardKey::_7,
            [AKEYCODE_8] = InputKeyboardKey::_8,
            [AKEYCODE_9] = InputKeyboardKey::_9,
            [AKEYCODE_SEMICOLON] = InputKeyboardKey::Semicolon,
            [AKEYCODE_EQUALS] = InputKeyboardKey::Eq,
            [AKEYCODE_A] = InputKeyboardKey::A,
            [AKEYCODE_B] = InputKeyboardKey::B,
            [AKEYCODE_C] = InputKeyboardKey::C,
            [AKEYCODE_D] = InputKeyboardKey::D,
            [AKEYCODE_E] = InputKeyboardKey::E,
            [AKEYCODE_F] = InputKeyboardKey::F,
            [AKEYCODE_G] = InputKeyboardKey::G,
            [AKEYCODE_H] = InputKeyboardKey::H,
            [AKEYCODE_I] = InputKeyboardKey::I,
            [AKEYCODE_J] = InputKeyboardKey::J,
            [AKEYCODE_K] = InputKeyboardKey::K,
            [AKEYCODE_L] = InputKeyboardKey::L,
            [AKEYCODE_M] = InputKeyboardKey::M,
            [AKEYCODE_N] = InputKeyboardKey::N,
            [AKEYCODE_O] = InputKeyboardKey::O,
            [AKEYCODE_P] = InputKeyboardKey::P,
            [AKEYCODE_Q] = InputKeyboardKey::Q,
            [AKEYCODE_R] = InputKeyboardKey::R,
            [AKEYCODE_S] = InputKeyboardKey::S,
            [AKEYCODE_T] = InputKeyboardKey::T,
            [AKEYCODE_U] = InputKeyboardKey::U,
            [AKEYCODE_V] = InputKeyboardKey::V,
            [AKEYCODE_W] = InputKeyboardKey::W,
            [AKEYCODE_X] = InputKeyboardKey::X,
            [AKEYCODE_Y] = InputKeyboardKey::Y,
            [AKEYCODE_Z] = InputKeyboardKey::Z,
            [AKEYCODE_LEFT_BRACKET] = InputKeyboardKey::LeftBracket,
            [AKEYCODE_BACKSLASH] = InputKeyboardKey::Backslash,
            [AKEYCODE_RIGHT_BRACKET] = InputKeyboardKey::RightBracket,
            //[AKEYCODE_GRAVE_ACCENT] = InputKeyboardKey::GraveAccent,

            [AKEYCODE_ESCAPE] = InputKeyboardKey::Esc,
            [AKEYCODE_ENTER] = InputKeyboardKey::Enter,
            [AKEYCODE_TAB] = InputKeyboardKey::Tab,
            [AKEYCODE_FORWARD_DEL] = InputKeyboardKey::Backspace,
            [AKEYCODE_INSERT] = InputKeyboardKey::Insert,
            [AKEYCODE_DEL] = InputKeyboardKey::Del,
            [AKEYCODE_SYSTEM_NAVIGATION_RIGHT] = InputKeyboardKey::Right,
            [AKEYCODE_SYSTEM_NAVIGATION_LEFT] = InputKeyboardKey::Left,
            [AKEYCODE_SYSTEM_NAVIGATION_DOWN] = InputKeyboardKey::Down,
            [AKEYCODE_SYSTEM_NAVIGATION_UP] = InputKeyboardKey::Up,
            [AKEYCODE_PAGE_UP] = InputKeyboardKey::PageUp,
            [AKEYCODE_PAGE_DOWN] = InputKeyboardKey::PageDown,
            [AKEYCODE_HOME] = InputKeyboardKey::Home,
            [AKEYCODE_MOVE_END] = InputKeyboardKey::End,
            [AKEYCODE_CAPS_LOCK] = InputKeyboardKey::CapsLock,
            [AKEYCODE_SCROLL_LOCK] = InputKeyboardKey::ScrollLock,
            [AKEYCODE_NUM_LOCK] = InputKeyboardKey::NumLock,
            [AKEYCODE_SYSRQ] = InputKeyboardKey::PrintScreen,
            [AKEYCODE_MEDIA_PAUSE] = InputKeyboardKey::Pause,
            [AKEYCODE_F1] = InputKeyboardKey::F1,
            [AKEYCODE_F2] = InputKeyboardKey::F2,
            [AKEYCODE_F3] = InputKeyboardKey::F3,
            [AKEYCODE_F4] = InputKeyboardKey::F4,
            [AKEYCODE_F5] = InputKeyboardKey::F5,
            [AKEYCODE_F6] = InputKeyboardKey::F6,
            [AKEYCODE_F7] = InputKeyboardKey::F7,
            [AKEYCODE_F8] = InputKeyboardKey::F8,
            [AKEYCODE_F9] = InputKeyboardKey::F9,
            [AKEYCODE_F10] = InputKeyboardKey::F10,
            [AKEYCODE_F11] = InputKeyboardKey::F11,
            [AKEYCODE_F12] = InputKeyboardKey::F12,
            //[AKEYCODE_F13] = InputKeyboardKey::F13,
            //[AKEYCODE_F14] = InputKeyboardKey::F14,
            //[AKEYCODE_F15] = InputKeyboardKey::F15,
            //[AKEYCODE_F16] = InputKeyboardKey::F16,
            //[AKEYCODE_F17] = InputKeyboardKey::F17,
            //[AKEYCODE_F18] = InputKeyboardKey::F18,
            //[AKEYCODE_F19] = InputKeyboardKey::F19,
            //[AKEYCODE_F20] = InputKeyboardKey::F20,
            //[AKEYCODE_F21] = InputKeyboardKey::F21,
            //[AKEYCODE_F22] = InputKeyboardKey::F22,
            //[AKEYCODE_F23] = InputKeyboardKey::F23,
            //[AKEYCODE_F24] = InputKeyboardKey::F24,
            //[AKEYCODE_F25] = InputKeyboardKey::F25,
            [AKEYCODE_NUMPAD_0] = InputKeyboardKey::Kp0,
            [AKEYCODE_NUMPAD_1] = InputKeyboardKey::Kp1,
            [AKEYCODE_NUMPAD_2] = InputKeyboardKey::Kp2,
            [AKEYCODE_NUMPAD_3] = InputKeyboardKey::Kp3,
            [AKEYCODE_NUMPAD_4] = InputKeyboardKey::Kp4,
            [AKEYCODE_NUMPAD_5] = InputKeyboardKey::Kp5,
            [AKEYCODE_NUMPAD_6] = InputKeyboardKey::Kp6,
            [AKEYCODE_NUMPAD_7] = InputKeyboardKey::Kp7,
            [AKEYCODE_NUMPAD_8] = InputKeyboardKey::Kp8,
            [AKEYCODE_NUMPAD_9] = InputKeyboardKey::Kp9,
            [AKEYCODE_NUMPAD_DOT] = InputKeyboardKey::KpDec,
            [AKEYCODE_NUMPAD_DIVIDE] = InputKeyboardKey::KpDiv,
            [AKEYCODE_NUMPAD_MULTIPLY] = InputKeyboardKey::KpMul,
            [AKEYCODE_NUMPAD_SUBTRACT] = InputKeyboardKey::KpSub,
            [AKEYCODE_NUMPAD_ADD] = InputKeyboardKey::KpAdd,
            [AKEYCODE_NUMPAD_ENTER] = InputKeyboardKey::KpEnter,
            [AKEYCODE_NUMPAD_EQUALS] = InputKeyboardKey::KpEq,
            [AKEYCODE_SHIFT_LEFT] = InputKeyboardKey::LeftShift,
            [AKEYCODE_CTRL_LEFT] = InputKeyboardKey::LeftControl,
            [AKEYCODE_ALT_LEFT] = InputKeyboardKey::LeftAlt,
            //[AKEYCODE_SUPER_LEFT] = InputKeyboardKey::LeftSuper,
            [AKEYCODE_SHIFT_RIGHT] = InputKeyboardKey::RightShift,
            [AKEYCODE_CTRL_RIGHT] = InputKeyboardKey::RightControl,
            [AKEYCODE_ALT_RIGHT] = InputKeyboardKey::RightAlt,
            //[AKEYCODE_SUPER_RIGHT] = InputKeyboardKey::RightSuper,
            [AKEYCODE_MENU] = InputKeyboardKey::Menu
    };

    static InputMouseBtn pb_mouse_btn_code_to_edge[] = {
            InputMouseBtn::Left,
            InputMouseBtn::Right,
            InputMouseBtn::Middle,
            InputMouseBtn::_4,
            InputMouseBtn::_5,
            InputMouseBtn::_6,
            InputMouseBtn::_7,
            InputMouseBtn::_8
    };

    static InputPadBtn pb_pad_btn_code_to_edge[PADDLEBOAT_BUTTON_COUNT] = {
            InputPadBtn::DpadUp,
            InputPadBtn::DpadLeft,
            InputPadBtn::DpadDown,
            InputPadBtn::DpadRight,
            InputPadBtn::A,
            InputPadBtn::B,
            InputPadBtn::X,
            InputPadBtn::Y,
            InputPadBtn::BumperLeft,
            InputPadBtn::TriggerLeft,
            InputPadBtn::ThumbLeft,
            InputPadBtn::BumperRight,
            InputPadBtn::TriggerRight,
            InputPadBtn::ThumbRight,
            InputPadBtn::Back,
            InputPadBtn::Start,
            InputPadBtn::Guide,
            InputPadBtn::Unknown,
            InputPadBtn::Unknown,
            InputPadBtn::Unknown,
            InputPadBtn::Unknown,
            InputPadBtn::Unknown
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

    static void on_app_cmd_cb(struct android_app* app, i32 cmd) {
        auto* ctx = (PlatformContext*)app->userData;

        switch (cmd) {
            case APP_CMD_INIT_WINDOW: {
                ctx->wnd->surface_ready = true;
                break;
            }
            case APP_CMD_CONTENT_RECT_CHANGED: {
                i32 width = app->contentRect.right - app->contentRect.left;
                i32 height = app->contentRect.bottom - app->contentRect.top;

                EDGE_LOG_DEBUG("Contect rect changed: %dx%d", width, height);

                WindowResizeEvent evt = {};
                window_resize_event_init(&evt, width, height);
                event_dispatcher_dispatch(ctx->event_dispatcher, (EventHeader*)&evt);
                break;
            }
            case APP_CMD_GAINED_FOCUS: {
                EDGE_LOG_DEBUG("Focus gained.");

                WindowFocusEvent evt = {};
                window_focus_event_init(&evt, true);
                event_dispatcher_dispatch(ctx->event_dispatcher, (EventHeader*)&evt);
                break;
            }
            case APP_CMD_LOST_FOCUS: {
                EDGE_LOG_DEBUG("Focus lost.");
                WindowFocusEvent evt = {};
                window_focus_event_init(&evt, false);
                event_dispatcher_dispatch(ctx->event_dispatcher, (EventHeader*)&evt);
                break;
            }
            case APP_CMD_START: {
                if(!Paddleboat_isInitialized()) {
                    break;
                }

                JNIEnv* jni_env = get_jni_env(app);
                Paddleboat_onStart(jni_env);
                break;
            }
            case APP_CMD_STOP: {
                if(!Paddleboat_isInitialized()) {
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

    static void motion_data_cb(const i32 controller_index, const Paddleboat_Motion_Data* motion_data, void* user_data) {
        if(!user_data || !motion_data) {
            return;
        }

        PlatformContext* ctx = (PlatformContext*)user_data;

        switch (motion_data->motionType) {
            case PADDLEBOAT_MOTION_ACCELEROMETER: {
                // TODO: DISPATCH EVENT
                break;
            }
            case PADDLEBOAT_MOTION_GYROSCOPE: {
                // TODO: DISPATCH EVENT
                break;
            }
        }
    }

    static void controller_status_cb(const i32 controller_index, const enum Paddleboat_ControllerStatus controller_status, void* user_data) {
        if(!user_data) {
            return;
        }

        auto* ctx = (PlatformContext*)user_data;

        bool is_just_connected = controller_status == PADDLEBOAT_CONTROLLER_JUST_CONNECTED;
        bool is_just_disconnected = controller_status == PADDLEBOAT_CONTROLLER_JUST_DISCONNECTED;

        if (is_just_connected || is_just_disconnected) {
            char controller_name[256];
            if (Paddleboat_getControllerName(controller_index, sizeof(controller_name), controller_name) != PADDLEBOAT_NO_ERROR) {
                return;
            }

            Paddleboat_Controller_Info controller_info;
            if (Paddleboat_getControllerInfo(controller_index, &controller_info) != PADDLEBOAT_NO_ERROR) {
                return;
            }

            EDGE_LOG_DEBUG("Connected gamepad, name: \"%s\", id: %d, vendor: %d, product: %d, device: %d.",
                       controller_name, controller_index, controller_info.vendorId,
                       controller_info.productId, controller_info.deviceId);

            EDGE_LOG_DEBUG("Feature support:\naccel: %d; gyro: %d; player light: %d; rgb light: %d; battery info: %d; vibration: %d; dual motor vibration: %d; touchpad: %d; virtual mouse: %d;",
                       (controller_info.controllerFlags & PADDLEBOAT_CONTROLLER_FLAG_ACCELEROMETER) == PADDLEBOAT_CONTROLLER_FLAG_ACCELEROMETER,
                       (controller_info.controllerFlags & PADDLEBOAT_CONTROLLER_FLAG_GYROSCOPE) == PADDLEBOAT_CONTROLLER_FLAG_GYROSCOPE,
                       (controller_info.controllerFlags & PADDLEBOAT_CONTROLLER_FLAG_LIGHT_PLAYER) == PADDLEBOAT_CONTROLLER_FLAG_LIGHT_PLAYER,
                       (controller_info.controllerFlags & PADDLEBOAT_CONTROLLER_FLAG_LIGHT_RGB) == PADDLEBOAT_CONTROLLER_FLAG_LIGHT_RGB,
                       (controller_info.controllerFlags & PADDLEBOAT_CONTROLLER_FLAG_BATTERY) == PADDLEBOAT_CONTROLLER_FLAG_BATTERY,
                       (controller_info.controllerFlags & PADDLEBOAT_CONTROLLER_FLAG_VIBRATION) == PADDLEBOAT_CONTROLLER_FLAG_VIBRATION,
                       (controller_info.controllerFlags & PADDLEBOAT_CONTROLLER_FLAG_VIBRATION_DUAL_MOTOR) == PADDLEBOAT_CONTROLLER_FLAG_VIBRATION_DUAL_MOTOR,
                       (controller_info.controllerFlags & PADDLEBOAT_CONTROLLER_FLAG_TOUCHPAD) == PADDLEBOAT_CONTROLLER_FLAG_TOUCHPAD,
                       (controller_info.controllerFlags & PADDLEBOAT_CONTROLLER_FLAG_VIRTUAL_MOUSE) == PADDLEBOAT_CONTROLLER_FLAG_VIRTUAL_MOUSE);

            InputPadConnectionEvent evt = {};
            input_pad_connection_event_init(&evt,
                                            controller_index,
                                            controller_info.vendorId,
                                            controller_info.productId,
                                            controller_info.deviceId,
                                            is_just_connected,
                                            controller_name
            );

            event_dispatcher_dispatch(ctx->event_dispatcher, (EventHeader*)&evt);
        }
    }

    static void mouse_status_cb(const enum Paddleboat_MouseStatus mouse_status, void* user_data) {
        if(!user_data) {
            return;
        }

        auto* ctx = (PlatformContext*)user_data;
        if (mouse_status == PADDLEBOAT_MOUSE_NONE) {
            EDGE_LOG_DEBUG("Mouse disconnected.");
        }
        else {
            EDGE_LOG_DEBUG("%s mouse connected.", mouse_status == PADDLEBOAT_MOUSE_CONTROLLER_EMULATED ? "Virtual" : "Physical");
        }
    }

    static void keyboard_status_cb(const bool physical_keyboard_status, void* user_data) {
        if(!user_data) {
            return;
        }

        auto* ctx = (PlatformContext*)user_data;
        EDGE_LOG_DEBUG("Physical keyboard %sconnected.", physical_keyboard_status ? "" : "dis");
    }

    PlatformContext* platform_context_create(PlatformContextCreateInfo create_info) {
        if (!create_info.alloc || !create_info.layout) {
            return nullptr;
        }

        auto* ctx = create_info.alloc->allocate<PlatformContext>();
        if (!ctx) {
            return nullptr;
        }

        ctx->alloc = create_info.alloc;
        ctx->layout = create_info.layout;
        ctx->event_dispatcher = create_info.event_dispatcher;

        Logger* logger = logger_get_global();
        LoggerOutput* debug_output = logger_create_logcat_output(create_info.alloc, LogFormat_Default);
        logger_add_output(logger, debug_output);

        struct android_app* app = ctx->layout->app;
        app->onAppCmd = on_app_cmd_cb;
        app->userData = ctx;

        Paddleboat_setMotionDataCallbackWithIntegratedFlags(motion_data_cb, Paddleboat_getIntegratedMotionSensorFlags(), ctx);
        Paddleboat_setControllerStatusCallback(controller_status_cb, ctx);
        Paddleboat_setMouseStatusCallback(mouse_status_cb, ctx);
        Paddleboat_setPhysicalKeyboardStatusCallback(keyboard_status_cb, ctx);

        return ctx;
    }

    void platform_context_destroy(PlatformContext* ctx) {
        if (!ctx) {
            return;
        }

        if (Paddleboat_isInitialized()) {
            Paddleboat_setControllerStatusCallback(nullptr, nullptr);
            Paddleboat_setMouseStatusCallback(nullptr, nullptr);
            Paddleboat_setPhysicalKeyboardStatusCallback(nullptr, nullptr);
            Paddleboat_setMotionDataCallback(nullptr, nullptr);

            JNIEnv* jni_env = get_jni_env(ctx->layout->app);
            Paddleboat_destroy(jni_env);
        }

        const Allocator* alloc = ctx->alloc;
        alloc->deallocate(ctx);
    }

    Window* window_create(WindowCreateInfo create_info) {
        auto* wnd = create_info.alloc->allocate<Window>();
        if (!wnd) {
            return nullptr;
        }

        wnd->mode = create_info.mode;
        wnd->resizable = create_info.resizable;
        wnd->vsync_mode = create_info.vsync_mode;
        wnd->should_close = false;

        struct android_app* app = create_info.platform_context->layout->app;

        JNIEnv* jni_env = get_jni_env(app);
        if(!jni_env) {
            create_info.alloc->deallocate(wnd);
            return nullptr;
        }

        Paddleboat_ErrorCode result = Paddleboat_init(jni_env, app->activity->javaGameActivity);
        if (result != PADDLEBOAT_NO_ERROR) {
            create_info.alloc->deallocate(wnd);

            EDGE_LOG_DEBUG("Failed to initialize Paddleboat: %d", (i32)result);
            return nullptr;
        }

        if (!Paddleboat_isInitialized()) {
            create_info.alloc->deallocate(wnd);

            EDGE_LOG_DEBUG("Paddleboat initialization verification failed");
            return nullptr;
        }

        create_info.platform_context->wnd = wnd;
        wnd->ctx = create_info.platform_context;

        while(!wnd->surface_ready) {
            window_process_events(wnd, 0.33f);
        }

        return wnd;
    }

    void window_destroy(NotNull<const Allocator*> alloc, Window* wnd) {
        GameActivity_finish(wnd->ctx->layout->app->activity);
        wnd->should_close = true;

        alloc->deallocate(wnd);
    }

    bool window_should_close(NotNull<Window*> wnd) {
        return wnd->should_close;
    }

    void window_process_events(NotNull<Window*> wnd, f32 delta_time) {
        struct android_app* app = wnd->ctx->layout->app;
        struct android_poll_source* source;
        i32 ident, events;

        while ((ident = ALooper_pollOnce(0, nullptr, &events, (void**)&source)) > ALOOPER_POLL_TIMEOUT) {
            if (source) {
                source->process(app, source);
            }

            if (app->destroyRequested != 0) {
                EDGE_LOG_DEBUG("Requested window destroy.");

                WindowCloseEvent evt{};
                window_close_event_init(&evt);
                event_dispatcher_dispatch(wnd->ctx->event_dispatcher, (EventHeader*)&evt);

                wnd->ctx->wnd->should_close = true;
            }
        }

        if (Paddleboat_isInitialized()) {
            return;
            Paddleboat_update()
        }

        struct android_input_buffer* input_buf = android_app_swap_input_buffers(app);
        if(!input_buf) {
            return;
        }

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
                    // TODO: Not Implemented
                    for (i32 i = 0; i < event->pointerCount; ++i) {

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

                input_update_keyboard_state(&wnd->input_state,
                                            wnd->ctx->event_dispatcher,
                                            pb_key_code_to_edge[event->keyCode],
                                            event->action == AKEY_STATE_DOWN ? InputKeyAction::Down : InputKeyAction::Up);
            }
            android_app_clear_key_events(input_buf);
        }

        JNIEnv* jni_env = get_jni_env(app);
        if(!jni_env) {
            return;
        }

        Paddleboat_update(jni_env);

        // Process mouse
        Paddleboat_Mouse_Data mouse_data;
        if (Paddleboat_getMouseData(&mouse_data) == PADDLEBOAT_NO_ERROR) {
            // Process mouse buttons
            for (i32 button_idx = 0; button_idx < 8; ++button_idx) {
                Paddleboat_Mouse_Buttons mouse_button = (enum Paddleboat_Mouse_Buttons)(1 << button_idx);
                input_update_mouse_btn_state(&wnd->input_state,
                                             wnd->ctx->event_dispatcher,
                                             pb_mouse_btn_code_to_edge[button_idx],
                                             (mouse_data.buttonsDown & mouse_button) == mouse_button ? InputKeyAction::Down : InputKeyAction::Up);
            }

            // Process mouse motion
            input_update_mouse_move_state(&wnd->input_state,
                                          wnd->ctx->event_dispatcher,
                                          (f32)mouse_data.mouseX,
                                          (f32)mouse_data.mouseY);

            // Process mouse scroll
            InputMouseScrollEvent evt = {};
            input_mouse_scroll_event_init(&evt,
                                          (f32)mouse_data.mouseScrollDeltaH,
                                          (f32)mouse_data.mouseScrollDeltaV);

            event_dispatcher_dispatch(wnd->ctx->event_dispatcher, (EventHeader*)&evt);
        }

        // Update all controllers available
        for (i32 jid = 0; jid < PADDLEBOAT_MAX_CONTROLLERS; ++jid) {
            enum Paddleboat_ControllerStatus status = Paddleboat_getControllerStatus(jid);
            if (status == PADDLEBOAT_CONTROLLER_ACTIVE) {
                Paddleboat_Controller_Data controller_data;
                if (Paddleboat_getControllerData(jid, &controller_data) == PADDLEBOAT_NO_ERROR) {
                    // Update gamepad buttons
                    for (i32 button_idx = 0; button_idx < PADDLEBOAT_BUTTON_COUNT; ++button_idx) {
                        Paddleboat_Buttons controller_button = (enum Paddleboat_Buttons)(1 << button_idx);
                        input_update_pad_btn_state(&wnd->input_state,
                                                   wnd->ctx->event_dispatcher,
                                                   jid, pb_pad_btn_code_to_edge[button_idx],
                                                   (controller_data.buttonsDown & controller_button) == controller_button ? InputKeyAction::Down : InputKeyAction::Up);
                    }

                    input_update_pad_axis_state(&wnd->input_state,
                                                wnd->ctx->event_dispatcher,
                                                jid, InputPadAxis::StickLeft,
                                                controller_data.leftStick.stickX,
                                                controller_data.leftStick.stickY,
                                                0.0f);

                    input_update_pad_axis_state(&wnd->input_state,
                                                wnd->ctx->event_dispatcher,
                                                jid,
                                                InputPadAxis::StickRight,
                                                controller_data.rightStick.stickX,
                                                controller_data.rightStick.stickY,
                                                0.0f);
                    input_update_pad_axis_state(&wnd->input_state,
                                                wnd->ctx->event_dispatcher, jid,
                                                InputPadAxis::TriggerLeft,
                                                controller_data.triggerL2,
                                                0.0f,
                                                0.0f);
                    input_update_pad_axis_state(&wnd->input_state,
                                                wnd->ctx->event_dispatcher,
                                                jid,
                                                InputPadAxis::TriggerRight,
                                                controller_data.triggerR2,
                                                0.0f,
                                                0.0f);
                }
            }
        }
    }

    void window_show(NotNull<Window*> wnd) {

    }

    void window_hide(NotNull<Window*> wnd) {

    }

    void window_get_surface(NotNull<Window*> wnd, void* surface_info) {
        *(VkAndroidSurfaceCreateInfoKHR*)surface_info = {
                .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
                .window = wnd->ctx->layout->app->window
        };
    }

    void window_set_title(NotNull<Window*> wnd, const char* title) {

    }

    void window_get_size(NotNull<Window*> wnd, i32* width, i32* height) {
        struct android_app* app = wnd->ctx->layout->app;
        *width = app->contentRect.right - app->contentRect.left;
        *height = app->contentRect.bottom - app->contentRect.top;
    }

    f32 window_dpi_scale_factor(NotNull<Window*> wnd) {
        struct android_app* app = wnd->ctx->layout->app;
        return (f32)AConfiguration_getDensity(app->config) / (f32)ACONFIGURATION_DENSITY_MEDIUM;
    }

    f32 window_content_scale_factor(NotNull<Window*> wnd) {
        return 1.0f;
    }
}

extern "C" void android_main(struct android_app* state) {
    edge::PlatformLayout platform_layout = {
            .app = state
    };
    edge_main(&platform_layout);
}