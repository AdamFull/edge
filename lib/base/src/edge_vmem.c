#include "edge_platform_detect.h"

#if EDGE_HAS_WINDOWS_API
#include "edge_vmem_win.c"
#elif EDGE_PLATFORM_POSIX
#include "edge_vmem_posix.c"
#else
#error "Unsupported platform"
#endif