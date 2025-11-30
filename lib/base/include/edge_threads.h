#ifndef EDGE_THREADS_H
#define EDGE_THREADS_H

#include <stddef.h>
#include <time.h>

#include "edge_base.h"
#include "edge_platform_detect.h"

#ifdef __cplusplus
extern "C" {
#endif

    typedef enum {
        edge_thrd_success = 0,
        edge_thrd_error = 1,
        edge_thrd_nomem = 2,
        edge_thrd_timedout = 3,
        edge_thrd_busy = 4
    } edge_thrd_result_t;

    typedef enum {
        edge_mtx_plain = 0,
        edge_mtx_recursive = 1,
        edge_mtx_timed = 2
    } edge_mtx_type_t;

    typedef struct edge_thrd edge_thrd_t;
    typedef struct edge_mtx edge_mtx_t;
    typedef struct edge_cnd edge_cnd_t;
    typedef struct edge_once edge_once_t;

    typedef i32 (*edge_thrd_start_t)(void* arg);

#if EDGE_HAS_WINDOWS_API
    struct edge_thrd {
        void* handle;
        u32 id;
    };

    struct edge_mtx {
        void* handle;
        edge_mtx_type_t type;
    };

    struct edge_cnd {
        void* handle;
    };

    struct edge_once {
        long state;
    };
#elif EDGE_PLATFORM_POSIX
    struct edge_thrd {
        unsigned long handle;
    };

    struct edge_mtx {
        unsigned char data[40];
        edge_mtx_type_t type;
    };

    struct edge_cnd {
        unsigned char data[48];
    };

    struct edge_once {
        i32 state;
    };
#else
#error "Unsupported platform"
#endif

    i32 edge_thrd_create(edge_thrd_t* thr, edge_thrd_start_t func, void* arg);
    i32 edge_thrd_join(edge_thrd_t thr, i32* res);
    i32 edge_thrd_detach(edge_thrd_t thr);
    edge_thrd_t edge_thrd_current(void);
    u32 edge_thrd_current_thread_id(void);

    i32 edge_thrd_equal(edge_thrd_t lhs, edge_thrd_t rhs);
    void edge_thrd_exit(i32 res);
    void edge_thrd_yield(void);
    i32 edge_thrd_sleep(const struct timespec* duration, struct timespec* remaining);

    i32 edge_mtx_init(edge_mtx_t* mtx, edge_mtx_type_t type);
    void edge_mtx_destroy(edge_mtx_t* mtx);
    i32 edge_mtx_lock(edge_mtx_t* mtx);
    i32 edge_mtx_trylock(edge_mtx_t* mtx);
    i32 edge_mtx_timedlock(edge_mtx_t* mtx, const struct timespec* ts);
    i32 edge_mtx_unlock(edge_mtx_t* mtx);

    i32 edge_cnd_init(edge_cnd_t* cnd);
    void edge_cnd_destroy(edge_cnd_t* cnd);
    i32 edge_cnd_signal(edge_cnd_t* cnd);
    i32 edge_cnd_broadcast(edge_cnd_t* cnd);
    i32 edge_cnd_wait(edge_cnd_t* cnd, edge_mtx_t* mtx);
    i32 edge_cnd_timedwait(edge_cnd_t* cnd, edge_mtx_t* mtx, const struct timespec* ts);

    void edge_call_once(edge_once_t* flag, void (*func)(void));

    typedef struct {
        i32 logical_id;
        i32 physical_id;
        i32 core_id;
    } edge_cpu_info_t;

    i32 edge_thrd_set_affinity_ex(edge_thrd_t thr, edge_cpu_info_t* cpu_info, i32 cpu_count, i32 core_id, bool prefer_physical);
    i32 edge_thrd_set_affinity(edge_thrd_t thr, i32 core_id, bool prefer_physical);
    i32 edge_thrd_set_name(edge_thrd_t thr, const char* name);

    i32 edge_thrd_get_physical_core_count(edge_cpu_info_t* cpu_info, i32 count);
    i32 edge_thrd_get_logical_core_count(edge_cpu_info_t* cpu_info, i32 count);
    i32 edge_thrd_get_cpu_topology(edge_cpu_info_t* cpu_info, i32 max_cpus);

#ifdef __cplusplus
}
#endif

#endif /* EDGE_THREADS_H */
