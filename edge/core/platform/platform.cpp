#include "platform.h"

#include "platform_definitioin.h"

#if EDGE_PLATFORM_WINDOWS
#include "windows/platform_context.cpp"
#include "windows/platform_window.cpp"
#elif EDGE_PLATFORM_ANDROID
#include "android/platform_context.cpp"
#include "android/platform_window.cpp"
#endif