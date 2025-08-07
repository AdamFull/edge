#pragma once

struct android_app;

struct _JNIEnv;
typedef _JNIEnv JNIEnv;

namespace edge::platform {
    auto get_jni_env(android_app* app) -> JNIEnv*;
}