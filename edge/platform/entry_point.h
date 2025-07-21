#pragma once

#include "platform_definitioin.h"

#if EDGE_PLATFORM_WINDOWS
#include "windows_context.h"
#endif

auto platform_main(edge::platform::PlatformContext& platform_context) -> int;