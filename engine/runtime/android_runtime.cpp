#include "runtime.h"
#include "input.h"

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

    struct AndroidRuntime final : IRuntime {
        bool init(const RuntimeInitInfo& init_info) noexcept override {
            layout = init_info.layout;

            if (!input_system.create(init_info.alloc)) {
                EDGE_LOG_ERROR("Failed to create input system.");
                return false;
            }

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

            input_system.destroy(alloc);
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

            if (Paddleboat_isInitialized()) {
                Paddleboat_update(jni_env);
            }

            struct android_input_buffer* input_buf = android_app_swap_input_buffers(app);
            if (!input_buf) {
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

                    // TODO: Input
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
                // Process mouse buttons
                for (i32 button_idx = 0; button_idx < 8; ++button_idx) {
                    Paddleboat_Mouse_Buttons mouse_button = (enum Paddleboat_Mouse_Buttons)(1 << button_idx);
                    // TODO: Input
                }
                // TODO: Input
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
                            // TODO: Input
                        }

                        // TODO: Input
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

        bool is_focused() const noexcept override {
            return focused;
        }

        void set_title(const char* title) noexcept override {

        }

        InputSystem* get_input() noexcept override {
            return &input_system;
        }

        RuntimeLayout* layout = nullptr;
        bool should_close = false;
        bool surface_ready = false;
        bool focused = true;
        InputSystem input_system = {};
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