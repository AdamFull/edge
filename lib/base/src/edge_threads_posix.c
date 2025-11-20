#include "edge_threads.h"

#include <stdlib.h>
#include <stdio.h>

#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>

struct thread_start_info {
    edge_thrd_start_t func;
    void* arg;
};

static void* thread_start_wrapper(void* arg) {
    struct thread_start_info* info = (struct thread_start_info*)arg;
    edge_thrd_start_t func = info->func;
    void* user_arg = info->arg;
    free(info);

    int result = func(user_arg);
    return (void*)(intptr_t)result;
}

int edge_thrd_create(edge_thrd_t* thr, edge_thrd_start_t func, void* arg) {
    if (!thr || !func) {
        return edge_thrd_error;
    }

    struct thread_start_info* info = malloc(sizeof(struct thread_start_info));
    if (!info) {
        return edge_thrd_nomem;
    }

    info->func = func;
    info->arg = arg;

    pthread_t handle;
    int result = pthread_create(&handle, NULL, thread_start_wrapper, info);
    if (result != 0) {
        free(info);
        return result == ENOMEM ? edge_thrd_nomem : edge_thrd_error;
    }
    thr->handle = handle;
    return edge_thrd_success;
}

int edge_thrd_join(edge_thrd_t thr, int* res) {
    void* result_ptr;
    int join_result = pthread_join((pthread_t)thr.handle, &result_ptr);
    if (join_result != 0) {
        return edge_thrd_error;
    }

    if (res) {
        *res = (int)(intptr_t)result_ptr;
    }

    return edge_thrd_success;
}

int edge_thrd_detach(edge_thrd_t thr) {
    int result = pthread_detach((pthread_t)thr.handle);
    return result == 0 ? edge_thrd_success : edge_thrd_error;
}

edge_thrd_t edge_thrd_current(void) {
    edge_thrd_t thr;
    thr.handle = pthread_self();
    return thr;
}

unsigned int edge_thrd_current_thread_id(void) {
    edge_thrd_t thrd = edge_thrd_current();
    return (unsigned int)thrd.handle;
}

int edge_thrd_equal(edge_thrd_t lhs, edge_thrd_t rhs) {
    return pthread_equal((pthread_t)lhs.handle, (pthread_t)rhs.handle);
}

void edge_thrd_exit(int res) {
    pthread_exit((void*)(intptr_t)res);
}

void edge_thrd_yield(void) {
    sched_yield();
}

int edge_thrd_sleep(const struct timespec* duration, struct timespec* remaining) {
    if (!duration) {
        return edge_thrd_error;
    }

    struct timespec req = *duration;
    struct timespec rem;

    int result = nanosleep(&req, &rem);
    if (result != 0 && remaining) {
        *remaining = rem;
    }

    return result;
}


int edge_mtx_init(edge_mtx_t* mtx, edge_mtx_type_t type) {
    if (!mtx) {
        return edge_thrd_error;
    }

    mtx->type = type;

    pthread_mutex_t* mutex = (pthread_mutex_t*)mtx->data;
    pthread_mutexattr_t attr;

    if (pthread_mutexattr_init(&attr) != 0) {
        return edge_thrd_error;
    }

    if (type == edge_mtx_recursive) {
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    }

    int result = pthread_mutex_init(mutex, &attr);
    pthread_mutexattr_destroy(&attr);

    return result == 0 ? edge_thrd_success : edge_thrd_error;
}

void edge_mtx_destroy(edge_mtx_t* mtx) {
    if (!mtx) {
        return;
    }

    pthread_mutex_t* mutex = (pthread_mutex_t*)mtx->data;
    pthread_mutex_destroy(mutex);
}

int edge_mtx_lock(edge_mtx_t* mtx) {
    if (!mtx) {
        return edge_thrd_error;
    }

    pthread_mutex_t* mutex = (pthread_mutex_t*)mtx->data;
    int result = pthread_mutex_lock(mutex);
    return result == 0 ? edge_thrd_success : edge_thrd_error;
}

int edge_mtx_trylock(edge_mtx_t* mtx) {
    if (!mtx) {
        return edge_thrd_error;
    }

    pthread_mutex_t* mutex = (pthread_mutex_t*)mtx->data;
    int result = pthread_mutex_trylock(mutex);
    if (result == 0) {
        return edge_thrd_success;
    }
    else if (result == EBUSY) {
        return edge_thrd_busy;
    }
    return edge_thrd_error;
}

int edge_mtx_timedlock(edge_mtx_t* mtx, const struct timespec* ts) {
    if (!mtx || !ts) {
        return edge_thrd_error;
    }

    pthread_mutex_t* mutex = (pthread_mutex_t*)mtx->data;
    int result = pthread_mutex_timedlock(mutex, ts);
    if (result == 0) {
        return edge_thrd_success;
    }
    else if (result == ETIMEDOUT) {
        return edge_thrd_timedout;
    }
    return edge_thrd_error;
}

int edge_mtx_unlock(edge_mtx_t* mtx) {
    if (!mtx) {
        return edge_thrd_error;
    }

    pthread_mutex_t* mutex = (pthread_mutex_t*)mtx->data;
    int result = pthread_mutex_unlock(mutex);
    return result == 0 ? edge_thrd_success : edge_thrd_error;
}


int edge_cnd_init(edge_cnd_t* cnd) {
    if (!cnd) {
        return edge_thrd_error;
    }

    pthread_cond_t* cond = (pthread_cond_t*)cnd->data;
    int result = pthread_cond_init(cond, NULL);
    return result == 0 ? edge_thrd_success : edge_thrd_error;
}

void edge_cnd_destroy(edge_cnd_t* cnd) {
    if (!cnd) {
        return;
    }

    pthread_cond_t* cond = (pthread_cond_t*)cnd->data;
    pthread_cond_destroy(cond);
}

int edge_cnd_signal(edge_cnd_t* cnd) {
    if (!cnd) {
        return edge_thrd_error;
    }

    pthread_cond_t* cond = (pthread_cond_t*)cnd->data;
    int result = pthread_cond_signal(cond);
    return result == 0 ? edge_thrd_success : edge_thrd_error;
}

int edge_cnd_broadcast(edge_cnd_t* cnd) {
    if (!cnd) {
        return edge_thrd_error;
    }

    pthread_cond_t* cond = (pthread_cond_t*)cnd->data;
    int result = pthread_cond_broadcast(cond);
    return result == 0 ? edge_thrd_success : edge_thrd_error;
}

int edge_cnd_wait(edge_cnd_t* cnd, edge_mtx_t* mtx) {
    if (!cnd || !mtx) {
        return edge_thrd_error;
    }

    pthread_cond_t* cond = (pthread_cond_t*)cnd->data;
    pthread_mutex_t* mutex = (pthread_mutex_t*)mtx->data;
    int result = pthread_cond_wait(cond, mutex);
    return result == 0 ? edge_thrd_success : edge_thrd_error;
}

int edge_cnd_timedwait(edge_cnd_t* cnd, edge_mtx_t* mtx, const struct timespec* ts) {
    if (!cnd || !mtx || !ts) {
        return edge_thrd_error;
    }

    pthread_cond_t* cond = (pthread_cond_t*)cnd->data;
    pthread_mutex_t* mutex = (pthread_mutex_t*)mtx->data;
    int result = pthread_cond_timedwait(cond, mutex, ts);
    if (result == 0) {
        return edge_thrd_success;
    }
    else if (result == ETIMEDOUT) {
        return edge_thrd_timedout;
    }
    return edge_thrd_error;
}


void edge_call_once(edge_once_t* flag, void (*func)(void)) {
    if (!flag || !func) {
        return;
    }

    pthread_once_t* once = (pthread_once_t*)&flag->state;
    pthread_once(once, func);
}

int edge_thrd_set_affinity_platform(edge_thrd_t thr, int core_id) {
    if (core_id < 0) {
        return edge_thrd_error;
    }

    pthread_t thread = (pthread_t)thr.handle;

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    if (pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset) != 0) {
        return edge_thrd_error;
    }
    return edge_thrd_success;
}

int edge_thrd_set_name(edge_thrd_t thr, const char* name) {
    if (!name) {
        return edge_thrd_error;
    }

    pthread_t thread = (pthread_t)thr.handle;

    if (pthread_setname_np(thread, name) != 0) {
        return edge_thrd_error;
    }
    return edge_thrd_success;
}

int edge_thrd_get_cpu_topology(edge_cpu_info_t* cpu_info, int max_cpus) {
    if (!cpu_info || max_cpus <= 0) {
        return -1;
    }

    int cpu_count = 0;

    for (int i = 0; i < max_cpus; i++) {
        char path[256];

        /* Check if CPU exists */
        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d", i);
        FILE* test = fopen(path, "r");
        if (!test) {
            break;
        }
        fclose(test);

        cpu_info[cpu_count].logical_id = i;

        /* Get physical package ID */
        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/topology/physical_package_id", i);
        FILE* f = fopen(path, "r");
        if (f) {
            fscanf(f, "%d", &cpu_info[cpu_count].physical_id);
            fclose(f);
        }
        else {
            cpu_info[cpu_count].physical_id = 0;
        }

        /* Get core ID */
        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/topology/core_id", i);
        f = fopen(path, "r");
        if (f) {
            fscanf(f, "%d", &cpu_info[cpu_count].core_id);
            fclose(f);
        }
        else {
            cpu_info[cpu_count].core_id = i;
        }

        cpu_count++;
    }

    return cpu_count;
}