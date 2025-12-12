#include "platform.h"
#include "../event_dispatcher.h"

#include <cassert>

#include <allocator.hpp>
#include <logger.hpp>

#include <paddleboat/paddleboat.h>
#include <game-text-input/gametextinput.h>
#include <game-activity/native_app_glue/android_native_app_glue.h>

extern "C" int edge_main(PlatformLayout* platform_layout);

namespace edge {
    struct PlatformLayout {
        struct android_app* app;
    };

    struct Window {
        WindowMode mode;
        bool resizable;
        WindowVsyncMode vsync_mode;

        bool should_close;
        bool surface_ready;
    };

    struct platform_context {
        const Allocator* alloc;
        PlatformLayout* layout;
        EventDispatcher* event_dispatcher;

        Window* wnd;
    };

    static JNIEnv* get_jni_env(struct android_app* app) {
        JNIEnv* env = NULL;
        jint get_env_result = app->activity->vm->GetEnv((void**)&env, JNI_VERSION_1_6);
        if (get_env_result == JNI_EDETACHED) {
            if (app->activity->vm->AttachCurrentThread(&env, NULL) != JNI_OK) {
                return NULL;
            }
        }

        return env;
    }

    static void on_app_cmd_cb(struct android_app* app, i32 cmd) {
        PlatformContext* ctx = (PlatformContext*)app->userData;

        switch (cmd) {
            case APP_CMD_INIT_WINDOW: {
                // TODO: DISPATCH EVENT WINDOW SIZE CHANGED
                ctx->wnd->surface_ready = true;
                break;
            }
            case APP_CMD_CONTENT_RECT_CHANGED: {
                i32 width = app->contentRect.right - app->contentRect.left;
                i32 height = app->contentRect.bottom - app->contentRect.top;

                EDGE_LOG_DEBUG("Contect rect changed: %dx%d", width, height);

                // TODO: DISPATCH EVENT WINDOW SIZE CHANGED
                break;
            }
            case APP_CMD_GAINED_FOCUS: {
                EDGE_LOG_DEBUG("Focus gained.");
                // TODO: DISPATCH EVENT
                break;
            }
            case APP_CMD_LOST_FOCUS: {
                EDGE_LOG_DEBUG("Focus lost.");
                // TODO: DISPATCH EVENT
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

        PlatformContext* ctx = (PlatformContext*)user_data;

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

            // TODO: DISPATCH EVENT
            //dispatcher.emit(events::GamepadConnectionEvent{
            //    .gamepad_id = controller_index,
            //    .vendor_id = controller_info.vendorId,
            //    .product_id = controller_info.productId,
            //    .device_id = controller_info.deviceId,
            //    .connected = is_just_connected,
            //    .name = controller_name
            //});
        }
    }

    static void mouse_status_cb(const enum Paddleboat_MouseStatus mouse_status, void* user_data) {
        if(!user_data) {
            return;
        }

        PlatformContext* ctx = (PlatformContext*)user_data;

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

        PlatformContext* ctx = (PlatformContext*)user_data;

        EDGE_LOG_DEBUG("Physical keyboard %sconnected.", physical_keyboard_status ? "" : "dis");
    }

    PlatformContext* platform_context_create(platform_context_create_info_t* create_info) {
        if (!create_info || !create_info->alloc || !create_info->layout) {
            return NULL;
        }

        PlatformContext* ctx = allocate<PlatformContext>(create_info->alloc);
        if (!ctx) {
            return NULL;
        }

        ctx->alloc = create_info->alloc;
        ctx->layout = create_info->layout;
        ctx->event_dispatcher = create_info->event_dispatcher;
        ctx->wnd = NULL;

        Logger* logger = logger_get_global();
        LoggerOutput* debug_output = logger_create_logcat_output(create_info->alloc, EDGE_LOG_FORMAT_DEFAULT);
        logger_add_output(logger, debug_output);

        return ctx;
    }

    void platform_context_destroy(PlatformContext* ctx) {
        if (!ctx) {
            return;
        }

        if (ctx->wnd) {
            struct android_app* app = ctx->layout->app;

            if (Paddleboat_isInitialized()) {
                Paddleboat_setControllerStatusCallback(NULL, NULL);
                Paddleboat_setMouseStatusCallback(NULL, NULL);
                Paddleboat_setPhysicalKeyboardStatusCallback(NULL, NULL);
                Paddleboat_setMotionDataCallback(NULL, NULL);

                JNIEnv* jni_env = get_jni_env(app);
                Paddleboat_destroy(jni_env);
            }

            GameActivity_finish(app->activity);
            ctx->wnd->should_close = true;

            Window* wnd = ctx->wnd;
            edge_allocator_free(ctx->alloc, wnd);
            ctx->wnd = NULL;
        }

        const Allocator* alloc = ctx->alloc;
        deallocate(alloc, ctx);
    }

    bool platform_context_window_init(PlatformContext* ctx, WindowCreateInfo* create_info) {
        if (!ctx || !ctx->layout || !ctx->layout->app || !create_info) {
            return false;
        }

        Window* wnd = allocate<Window>(ctx->alloc);
        if (!wnd) {
            return false;
        }

        wnd->mode = create_info->mode;
        wnd->resizable = create_info->resizable;
        wnd->vsync_mode = create_info->vsync_mode;
        wnd->should_close = false;

        struct android_app* app = ctx->layout->app;
        app->onAppCmd = on_app_cmd_cb;
        app->userData = ctx;

        JNIEnv* jni_env = get_jni_env(app);
        if(!jni_env) {
            edge_allocator_free(ctx->alloc, wnd);
            return false;
        }

        enum Paddleboat_ErrorCode result = Paddleboat_init(jni_env, app->activity->javaGameActivity);
        if (result != PADDLEBOAT_NO_ERROR) {
            edge_allocator_free(ctx->alloc, wnd);

            EDGE_LOG_DEBUG("Failed to initialize Paddleboat: %d", (i32)result);
            return false;
        }

        if (!Paddleboat_isInitialized()) {
            edge_allocator_free(ctx->alloc, wnd);

            EDGE_LOG_DEBUG("Paddleboat initialization verification failed");
            return false;
        }

        Paddleboat_setMotionDataCallbackWithIntegratedFlags(motion_data_cb, Paddleboat_getIntegratedMotionSensorFlags(), ctx);
        Paddleboat_setControllerStatusCallback(controller_status_cb, ctx);
        Paddleboat_setMouseStatusCallback(mouse_status_cb, ctx);
        Paddleboat_setPhysicalKeyboardStatusCallback(keyboard_status_cb, ctx);

        ctx->wnd = wnd;

        return true;
    }

    bool platform_context_window_should_close(PlatformContext* ctx) {
        if (!ctx || !ctx->wnd) {
            return false;
        }

        return ctx->wnd->should_close;
    }

    void platform_context_window_process_events(PlatformContext* ctx, f32 delta_time) {
        if (!ctx || !ctx->wnd) {
            return;
        }

        struct android_app* app = ctx->layout->app;
        struct android_poll_source* source;
        i32 ident, events;

        while ((ident = ALooper_pollOnce(0, NULL, &events, (void**)&source)) > ALOOPER_POLL_TIMEOUT) {
            if (source) {
                source->process(app, source);
            }

            if (app->destroyRequested != 0) {
                EDGE_LOG_DEBUG("Requested window destroy.");
                // TODO: DISPATCH EVENT WINDOW CLOSE
                ctx->wnd->should_close = true;
            }
        }

        if (Paddleboat_isInitialized()) {
            return;
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

                // Process other events
                //auto& dispatcher = platform_context_->get_event_dispatcher();
                //dispatcher.emit(events::KeyEvent{
                //    .key_code = translate_keyboard_key_code(event->keyCode),
                //    .state = event->action == AKEY_STATE_DOWN,
                //    .window_id = ~0ull
                //});
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
                enum Paddleboat_Mouse_Buttons mouse_button = (enum Paddleboat_Mouse_Buttons)(1 << button_idx);
                // TODO: DISPATCH EVENT
                //dispatcher.emit(events::MouseKeyEvent{
                //    .key_code = translate_mouse_key_code(button_idx),
                //    .state = (mouse_data.buttonsDown & mouse_button) == mouse_button,
                //    .window_id = ~0ull
                //});
            }

            // Process mouse motion
            // TODO: DISPATCH EVENT
            //dispatcher.emit(events::MousePositionEvent{
            //    .x = mouse_data.mouseX,
            //    .y = mouse_data.mouseY,
            //    .window_id = ~0ull
            //});

            // Process mouse scroll
            // TODO: DISPATCH EVENT
            //dispatcher.emit(events::MouseScrollEvent{
            //    .offset_x = static_cast<double>(mouse_data.mouseScrollDeltaH),
            //    .offset_y = static_cast<double>(mouse_data.mouseScrollDeltaV),
            //    .window_id = ~0ull
            //});
        }

        // Update all controllers available
        for (i32 jid = 0; jid < PADDLEBOAT_MAX_CONTROLLERS; ++jid) {
            enum Paddleboat_ControllerStatus status = Paddleboat_getControllerStatus(jid);
            if (status == PADDLEBOAT_CONTROLLER_ACTIVE) {
                Paddleboat_Controller_Data controller_data;
                if (Paddleboat_getControllerData(jid, &controller_data) == PADDLEBOAT_NO_ERROR) {
                    // Update gamepad buttons
                    for (i32 button_idx = 0; button_idx < PADDLEBOAT_BUTTON_COUNT; ++button_idx) {
                        enum Paddleboat_Buttons controller_button = (enum Paddleboat_Buttons)(1 << button_idx);
                        //dispatcher.emit(events::GamepadButtonEvent{
                        //    .gamepad_id = jid,
                        //    .key_code = translate_gamepad_key_code(button_idx),
                        //    .state = (controller_data.buttonsDown & controller_button) == controller_button
                        //});
                    }

                    // Update gamepad axis
                    //dispatcher.emit(events::GamepadAxisEvent{
                    //    .gamepad_id = jid,
                    //    .values = { controller_data.leftStick.stickX, controller_data.leftStick.stickY },
                    //    .axis_code = GamepadAxisCode::eLeftStick
                    //});

                    //dispatcher.emit(events::GamepadAxisEvent{
                    //    .gamepad_id = jid,
                    //    .values = { controller_data.rightStick.stickX, controller_data.rightStick.stickY },
                    //    .axis_code = GamepadAxisCode::eRightStick
                    //});

                    //dispatcher.emit(events::GamepadAxisEvent{
                    //    .gamepad_id = jid,
                    //    .values = { controller_data.triggerL2 },
                    //    .axis_code = GamepadAxisCode::eLeftTrigger
                    //});

                    //dispatcher.emit(events::GamepadAxisEvent{
                    //    .gamepad_id = jid,
                    //    .values = { controller_data.triggerR2 },
                    //    .axis_code = GamepadAxisCode::eRightTrigger
                    //});
                }
            }
        }
    }

    void platform_context_window_show(PlatformContext* ctx) {
        if (!ctx || !ctx->wnd) {
            return;
        }
    }

    void platform_context_window_hide(PlatformContext* ctx) {
        if (!ctx || !ctx->wnd) {
            return;
        }
    }

    void platform_context_window_set_title(PlatformContext* ctx, const char* title) {
        if (!ctx || !ctx->wnd || !title) {
            return;
        }
    }

    f32 platform_context_window_dpi_scale_factor(PlatformContext* ctx) {
        if (!ctx || !ctx->wnd) {
            return 1.0f;
        }

        struct android_app* app = ctx->layout->app;
        return (f32)AConfiguration_getDensity(app->config) / (f32)ACONFIGURATION_DENSITY_MEDIUM;
    }

    f32 platform_context_window_content_scale_factor(PlatformContext* ctx) {
        return 1.0f;
    }
}

extern "C" void android_main(struct android_app* state) {
    PlatformLayout platform_layout = { 0 };
    edge_main(&platform_layout);
}