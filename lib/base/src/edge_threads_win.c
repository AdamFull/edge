#include "edge_threads.h"

#include <stdlib.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>

struct thread_start_info {
    edge_thrd_start_t func;
    void* arg;
};

static unsigned int __stdcall thread_start_wrapper(void* arg) {
    struct thread_start_info* info = (struct thread_start_info*)arg;
    edge_thrd_start_t func = info->func;
    void* user_arg = info->arg;
    free(info);

    int result = func(user_arg);
    return (unsigned int)result;
}

int edge_thrd_create(edge_thrd_t* thr, edge_thrd_start_t func, void* arg) {
    if (!thr || !func) {
        return edge_thrd_error;
    }

    struct thread_start_info* info = (struct thread_start_info*)malloc(sizeof(struct thread_start_info));
    if (!info) {
        return edge_thrd_nomem;
    }

    info->func = func;
    info->arg = arg;

    thr->handle = (HANDLE)_beginthreadex(NULL, 0, thread_start_wrapper, info, 0, &thr->id);
    if (!thr->handle) {
        free(info);
        return edge_thrd_error;
    }
    return edge_thrd_success;
}

int edge_thrd_join(edge_thrd_t thr, int* res) {
    if (!thr.handle) {
        return edge_thrd_error;
    }

    DWORD wait_result = WaitForSingleObject(thr.handle, INFINITE);
    if (wait_result != WAIT_OBJECT_0) {
        return edge_thrd_error;
    }

    if (res) {
        DWORD exit_code;
        if (GetExitCodeThread(thr.handle, &exit_code)) {
            *res = (int)exit_code;
        }
    }

    CloseHandle(thr.handle);
    return edge_thrd_success;
}

int edge_thrd_detach(edge_thrd_t thr) {
    if (!thr.handle) {
        return edge_thrd_error;
    }
    CloseHandle(thr.handle);
    return edge_thrd_success;
}

edge_thrd_t edge_thrd_current(void) {
    edge_thrd_t thr;
    thr.handle = GetCurrentThread();
    thr.id = GetCurrentThreadId();
    return thr;
}

unsigned int edge_thrd_current_thread_id(void) {
    edge_thrd_t thrd = edge_thrd_current();
    return thrd.id;
}

int edge_thrd_equal(edge_thrd_t lhs, edge_thrd_t rhs) {
    return lhs.id == rhs.id;
}

void edge_thrd_exit(int res) {
    _endthreadex((unsigned int)res);
}

void edge_thrd_yield(void) {
    SwitchToThread();
}

int edge_thrd_sleep(const struct timespec* duration, struct timespec* remaining) {
    if (!duration) {
        return edge_thrd_error;
    }

    DWORD milliseconds = (DWORD)(duration->tv_sec * 1000 + duration->tv_nsec / 1000000);
    Sleep(milliseconds);
    if (remaining) {
        remaining->tv_sec = 0;
        remaining->tv_nsec = 0;
    }
    return 0;
}


int edge_mtx_init(edge_mtx_t* mtx, edge_mtx_type_t type) {
    if (!mtx) {
        return edge_thrd_error;
    }

    mtx->type = type;

    if (type == edge_mtx_recursive) {
        mtx->handle = CreateMutex(NULL, FALSE, NULL);
    }
    else {
        mtx->handle = CreateMutex(NULL, FALSE, NULL);
    }

    if (!mtx->handle) {
        return edge_thrd_error;
    }
    return edge_thrd_success;
}

void edge_mtx_destroy(edge_mtx_t* mtx) {
    if (!mtx) {
        return;
    }

    if (mtx->handle) {
        CloseHandle(mtx->handle);
        mtx->handle = NULL;
    }
}

int edge_mtx_lock(edge_mtx_t* mtx) {
    if (!mtx) {
        return edge_thrd_error;
    }

    DWORD result = WaitForSingleObject(mtx->handle, INFINITE);
    return result == WAIT_OBJECT_0 ? edge_thrd_success : edge_thrd_error;
}

int edge_mtx_trylock(edge_mtx_t* mtx) {
    if (!mtx) {
        return edge_thrd_error;
    }

    DWORD result = WaitForSingleObject(mtx->handle, 0);
    if (result == WAIT_OBJECT_0) {
        return edge_thrd_success;
    }
    else if (result == WAIT_TIMEOUT) {
        return edge_thrd_busy;
    }
    return edge_thrd_error;
}

int edge_mtx_timedlock(edge_mtx_t* mtx, const struct timespec* ts) {
    if (!mtx || !ts) {
        return edge_thrd_error;
    }

    /* Calculate timeout in milliseconds */
    struct timespec now;
    timespec_get(&now, TIME_UTC);

    long long diff_sec = ts->tv_sec - now.tv_sec;
    long long diff_nsec = ts->tv_nsec - now.tv_nsec;
    long long total_ms = diff_sec * 1000 + diff_nsec / 1000000;

    if (total_ms < 0) {
        total_ms = 0;
    }

    DWORD result = WaitForSingleObject(mtx->handle, (DWORD)total_ms);
    if (result == WAIT_OBJECT_0) {
        return edge_thrd_success;
    }
    else if (result == WAIT_TIMEOUT) {
        return edge_thrd_timedout;
    }
    return edge_thrd_error;
}

int edge_mtx_unlock(edge_mtx_t* mtx) {
    if (!mtx) {
        return edge_thrd_error;
    }

    return ReleaseMutex(mtx->handle) ? edge_thrd_success : edge_thrd_error;
}


int edge_cnd_init(edge_cnd_t* cnd) {
    if (!cnd) {
        return edge_thrd_error;
    }

    cnd->handle = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!cnd->handle) {
        return edge_thrd_error;
    }
    return edge_thrd_success;
}

void edge_cnd_destroy(edge_cnd_t* cnd) {
    if (!cnd) {
        return;
    }

    if (cnd->handle) {
        CloseHandle(cnd->handle);
        cnd->handle = NULL;
    }
}

int edge_cnd_signal(edge_cnd_t* cnd) {
    if (!cnd) {
        return edge_thrd_error;
    }

    return SetEvent(cnd->handle) ? edge_thrd_success : edge_thrd_error;
}

int edge_cnd_broadcast(edge_cnd_t* cnd) {
    if (!cnd) {
        return edge_thrd_error;
    }

    return SetEvent(cnd->handle) ? edge_thrd_success : edge_thrd_error;
}

int edge_cnd_wait(edge_cnd_t* cnd, edge_mtx_t* mtx) {
    if (!cnd || !mtx) {
        return edge_thrd_error;
    }

    /* Release mutex, wait for signal, reacquire mutex */
    edge_mtx_unlock(mtx);
    DWORD result = WaitForSingleObject(cnd->handle, INFINITE);
    ResetEvent(cnd->handle);
    edge_mtx_lock(mtx);
    return result == WAIT_OBJECT_0 ? edge_thrd_success : edge_thrd_error;
}

int edge_cnd_timedwait(edge_cnd_t* cnd, edge_mtx_t* mtx, const struct timespec* ts) {
    if (!cnd || !mtx || !ts) {
        return edge_thrd_error;
    }

    /* Calculate timeout in milliseconds */
    struct timespec now;
    timespec_get(&now, TIME_UTC);

    long long diff_sec = ts->tv_sec - now.tv_sec;
    long long diff_nsec = ts->tv_nsec - now.tv_nsec;
    long long total_ms = diff_sec * 1000 + diff_nsec / 1000000;

    if (total_ms < 0) {
        total_ms = 0;
    }

    edge_mtx_unlock(mtx);
    DWORD result = WaitForSingleObject(cnd->handle, (DWORD)total_ms);
    ResetEvent(cnd->handle);
    edge_mtx_lock(mtx);

    if (result == WAIT_OBJECT_0) {
        return edge_thrd_success;
    }
    else if (result == WAIT_TIMEOUT) {
        return edge_thrd_timedout;
    }
    return edge_thrd_error;
}


void edge_call_once(edge_once_t* flag, void (*func)(void)) {
    if (!flag || !func) {
        return;
    }

    if (InterlockedCompareExchange(&flag->state, 1, 0) == 0) {
        func();
        InterlockedExchange(&flag->state, 2);
    }
    else {
        while (flag->state != 2) {
            SwitchToThread();
        }
    }
}

int edge_thrd_set_affinity_platform(edge_thrd_t thr, int core_id) {
    if (core_id < 0) {
        return edge_thrd_error;
    }

    HANDLE thread_handle = thr.handle;
    DWORD_PTR affinity_mask = (DWORD_PTR)1 << core_id;

    if (SetThreadAffinityMask(thread_handle, affinity_mask) == 0) {
        return edge_thrd_error;
    }
    return edge_thrd_success;
}

int edge_thrd_set_name(edge_thrd_t thr, const char* name) {
    if (!name) {
        return edge_thrd_error;
    }

    /* SetThreadDescription requires Windows 10 version 1607 or later */
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, name, -1, NULL, 0);
    if (size_needed <= 0) {
        return edge_thrd_error;
    }

    wchar_t* wide_name = (wchar_t*)malloc(size_needed * sizeof(wchar_t));
    if (!wide_name) {
        return edge_thrd_nomem;
    }

    if (MultiByteToWideChar(CP_UTF8, 0, name, -1, wide_name, size_needed) == 0) {
        free(wide_name);
        return edge_thrd_error;
    }

    HRESULT hr = SetThreadDescription(thr.handle, wide_name);
    free(wide_name);

    return SUCCEEDED(hr) ? edge_thrd_success : edge_thrd_error;
}

typedef BOOL(WINAPI* GetLogicalProcessorInformationFunc)(PSYSTEM_LOGICAL_PROCESSOR_INFORMATION, PDWORD);

int edge_thrd_get_cpu_topology(edge_cpu_info_t* cpu_info, int max_cpus) {
    if (!cpu_info || max_cpus <= 0) {
        return -1;
    }

    HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
    if (!kernel32) {
        return -1;
    }

    GetLogicalProcessorInformationFunc GetLogicalProcessorInfo = (GetLogicalProcessorInformationFunc)GetProcAddress(kernel32, "GetLogicalProcessorInformation");

    if (!GetLogicalProcessorInfo) {
        return -1;
    }

    DWORD buffer_size = 0;
    GetLogicalProcessorInfo(NULL, &buffer_size);

    if (buffer_size == 0) {
        return -1;
    }

    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION buffer =
        (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)malloc(buffer_size);

    if (!buffer) {
        return -1;
    }

    if (!GetLogicalProcessorInfo(buffer, &buffer_size)) {
        free(buffer);
        return -1;
    }

    DWORD count = buffer_size / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
    int cpu_count = 0;
    int physical_core = 0;

    for (DWORD i = 0; i < count && cpu_count < max_cpus; i++) {
        if (buffer[i].Relationship == RelationProcessorCore) {
            ULONG_PTR mask = buffer[i].ProcessorMask;

            for (int bit = 0; bit < 64 && cpu_count < max_cpus; bit++) {
                if (mask & ((ULONG_PTR)1 << bit)) {
                    cpu_info[cpu_count].logical_id = cpu_count;
                    cpu_info[cpu_count].physical_id = 0; /* Windows doesn't expose socket info easily */
                    cpu_info[cpu_count].core_id = physical_core;
                    cpu_count++;
                }
            }
            physical_core++;
        }
    }

    free(buffer);
    return cpu_count;
}