#if defined(_WIN32) || defined(_WIN64)
#include "edge_threads_win.c"
#elif defined(__linux__) || defined(__unix__) || defined(__APPLE__)
#include "edge_threads_posix.c"
#else
#error "Unsupported platform"
#endif