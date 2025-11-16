#if defined(_WIN32) || defined(_WIN64)
#define PLATFORM_WINDOWS
#elif defined(__linux__)
#define PLATFORM_LINUX
#else
#error "Unsupported platform. Only Windows and Linux are supported."
#endif

#include "edge_threads_ext.h"
#include <string.h>

#ifdef PLATFORM_WINDOWS
#include <windows.h>
#elif defined(PLATFORM_LINUX)
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#else
#error "Unsupported platform. Only Windows and Linux are supported."
#endif

int thrd_set_affinity(thrd_t thr, int core_id) {
    if (core_id < 0) {
        return thrd_error;
    }

#ifdef PLATFORM_WINDOWS
    // On Windows, thrd_t is typically a HANDLE or can be converted to one
    HANDLE thread_handle = (HANDLE)thr._Handle;

    // Create affinity mask for the specific core
    DWORD_PTR affinity_mask = (DWORD_PTR)1 << core_id;

    if (SetThreadAffinityMask(thread_handle, affinity_mask) == 0) {
        return thrd_error;
    }

    return thrd_success;

#elif defined(PLATFORM_LINUX)
    // On Linux with glibc, thrd_t is pthread_t
    pthread_t thread = (pthread_t)thr;

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    if (pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset) != 0) {
        return thrd_error;
    }

    return thrd_success;
#endif
}

int thrd_set_name(thrd_t thr, const char* name) {
    if (name == NULL) {
        return thrd_error;
    }

#ifdef PLATFORM_WINDOWS
    // SetThreadDescription requires Windows 10 version 1607 or later
    // We need to convert char* to wchar_t*

    // Calculate required buffer size
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, name, -1, NULL, 0);
    if (size_needed <= 0) {
        return thrd_error;
    }

    // Allocate buffer for wide string
    wchar_t* wide_name = (wchar_t*)malloc(size_needed * sizeof(wchar_t));
    if (wide_name == NULL) {
        return thrd_error;
    }

    // Convert to wide string
    if (MultiByteToWideChar(CP_UTF8, 0, name, -1, wide_name, size_needed) == 0) {
        free(wide_name);
        return thrd_error;
    }

    // Set the thread description
    HANDLE thread_handle = (HANDLE)thr._Handle;
    HRESULT hr = SetThreadDescription(thread_handle, wide_name);

    free(wide_name);

    return SUCCEEDED(hr) ? thrd_success : thrd_error;

#elif defined(PLATFORM_LINUX)
    // On Linux, thread names are limited to 16 characters (including null terminator)
    pthread_t thread = (pthread_t)thr;

    // pthread_setname_np truncates to 16 chars automatically on most systems
    if (pthread_setname_np(thread, name) != 0) {
        return thrd_error;
    }

    return thrd_success;
#endif
}

int thrd_get_cpu_count(void) {
#ifdef PLATFORM_WINDOWS
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return (int)sysinfo.dwNumberOfProcessors;

#elif defined(PLATFORM_LINUX)
    long count = sysconf(_SC_NPROCESSORS_ONLN);
    if (count == -1) {
        return -1;
    }
    return (int)count;
#endif
}