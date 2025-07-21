#pragma once

#if defined(_WIN32) || defined(_WIN64)
#define EDGE_PLATFORM_WINDOWS 1
#if defined(_WIN64)
#define EDGE_PLATFORM_WIN64 1
#else
#define EDGE_PLATFORM_WIN32 1
#endif
#elif defined(__ANDROID__)
#define EDGE_PLATFORM_ANDROID 1
#elif defined(__EMSCRIPTEN__)
#define EDGE_PLATFORM_EMSCRIPTEN 1
#elif defined(__linux__)
#define EDGE_PLATFORM_LINUX 1
#elif defined(__APPLE__)
#define EDGE_PLATFORM_APPLE 1
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE
#define EDGE_PLATFORM_IOS 1
#elif TARGET_OS_MAC
#define EDGE_PLATFORM_OSX 1
#endif
#elif defined(__FreeBSD__)
#define EDGE_PLATFORM_FREEBSD 1
#elif defined(__ORBIS__)
#define EDGE_PLATFORM_PS4 1
#elif defined(__PROSPERO__)
#define EDGE_PLATFORM_PS5 1
#elif defined(__NX__)
#define EDGE_PLATFORM_NX 1
#endif