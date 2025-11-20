#ifndef EDGE_THREADS_H
#define EDGE_THREADS_H

#include <stddef.h>
#include <stdbool.h>
#include <time.h>

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

    typedef int (*edge_thrd_start_t)(void* arg);

#if EDGE_HAS_WINDOWS_API
    struct edge_thrd {
        void* handle;
        unsigned int id;
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
        int state;
    };
#else
#error "Unsupported platform"
#endif

    int edge_thrd_create(edge_thrd_t* thr, edge_thrd_start_t func, void* arg);
    int edge_thrd_join(edge_thrd_t thr, int* res);
    int edge_thrd_detach(edge_thrd_t thr);
    edge_thrd_t edge_thrd_current(void);
    unsigned int edge_thrd_current_thread_id(void);

    int edge_thrd_equal(edge_thrd_t lhs, edge_thrd_t rhs);
    void edge_thrd_exit(int res);
    void edge_thrd_yield(void);
    int edge_thrd_sleep(const struct timespec* duration, struct timespec* remaining);

    int edge_mtx_init(edge_mtx_t* mtx, edge_mtx_type_t type);
    void edge_mtx_destroy(edge_mtx_t* mtx);
    int edge_mtx_lock(edge_mtx_t* mtx);
    int edge_mtx_trylock(edge_mtx_t* mtx);
    int edge_mtx_timedlock(edge_mtx_t* mtx, const struct timespec* ts);
    int edge_mtx_unlock(edge_mtx_t* mtx);

    int edge_cnd_init(edge_cnd_t* cnd);
    void edge_cnd_destroy(edge_cnd_t* cnd);
    int edge_cnd_signal(edge_cnd_t* cnd);
    int edge_cnd_broadcast(edge_cnd_t* cnd);
    int edge_cnd_wait(edge_cnd_t* cnd, edge_mtx_t* mtx);
    int edge_cnd_timedwait(edge_cnd_t* cnd, edge_mtx_t* mtx, const struct timespec* ts);

    void edge_call_once(edge_once_t* flag, void (*func)(void));

    typedef struct {
        int logical_id;
        int physical_id;
        int core_id;
    } edge_cpu_info_t;

    int edge_thrd_set_affinity_ex(edge_thrd_t thr, edge_cpu_info_t* cpu_info, int cpu_count, int core_id, bool prefer_physical);
    int edge_thrd_set_affinity(edge_thrd_t thr, int core_id, bool prefer_physical);
    int edge_thrd_set_name(edge_thrd_t thr, const char* name);

    int edge_thrd_get_physical_core_count(edge_cpu_info_t* cpu_info, int count);
    int edge_thrd_get_logical_core_count(edge_cpu_info_t* cpu_info, int count);
    int edge_thrd_get_cpu_topology(edge_cpu_info_t* cpu_info, int max_cpus);

#ifdef __cplusplus
}
#endif

#endif /* EDGE_THREADS_H */
