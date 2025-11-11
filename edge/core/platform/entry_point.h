#pragma once

#if EDGE_PLATFORM_WINDOWS
#include "windows_context.h"
#elif EDGE_PLATFORM_ANDROID
#include "android_context.h"
#elif EDGE_PLATFORM_LINUX
#include "linux_context.h"
#endif

auto platform_main(edge::platform::PlatformContext& platform_context) -> int;