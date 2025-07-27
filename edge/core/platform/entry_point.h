#pragma once

#include "platform_definitioin.h"

#if EDGE_PLATFORM_WINDOWS
#include "windows/platform.h"
#elif EDGE_PLATFORM_ANDROID
#include "android/platform.h"
#endif

auto platform_main(edge::platform::PlatformContext& platform_context) -> int;