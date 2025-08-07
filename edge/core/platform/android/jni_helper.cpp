#include "jni_helper.h"

#include <game-activity/native_app_glue/android_native_app_glue.h>

namespace edge::platform {
    auto get_jni_env(android_app* app) -> JNIEnv* {
        JNIEnv* env{ nullptr };
        jint get_env_result = app->activity->vm->GetEnv((void**)&env, JNI_VERSION_1_6);
        if (get_env_result == JNI_EDETACHED) {
            if (app->activity->vm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
                return nullptr;
            }
        }

        return env;
    }
}