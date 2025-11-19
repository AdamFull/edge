#include "edge_scheduler.h"
#include "edge_fiber.h"

#include <stdatomic.h>
#include <stdio.h>
#include <assert.h>

#include <edge_allocator.h>
#include <edge_arena.h>
#include <edge_threads.h>
#include <edge_list.h>
#include <edge_queue.h>
#include <edge_vector.h>

// TODO: Add valgrind support
// TODO: Add GDB and LLDB support
// TODO: Prevent signal delivery on job threads
// TODO: Improve stack memory allocation. It may be better to preallocate several 
// stacks of different sizes and immediately place them in free lists. 
// For example, there could be small, medium, and large stacks.

struct edge_job {
    edge_fiber_context_t* context;
    edge_coro_fn func;
    void* user_data;
    edge_coro_state_t state;
    edge_job_t* caller;

    edge_sched_priority_t priority;
};

typedef struct worker_thread {
    edge_thrd_t thread;
    edge_sched_t* scheduler;
    size_t thread_id;
    atomic_bool should_exit;
} worker_thread_t;

struct edge_sched {
    edge_allocator_t* allocator;

    // For stack memory allocation
    edge_arena_t* protected_arena;
    edge_list_t* free_stacks;
    edge_mtx_t stack_mutex;

    // Global work queue
    edge_queue_t* queues[EDGE_SCHED_PRIORITY_COUNT];
    edge_mtx_t queue_mutex;
    edge_cnd_t queue_cond;

    edge_vector_t* worker_threads;

    atomic_size_t active_jobs;
    atomic_size_t queued_jobs;
    atomic_bool shutdown;

    // For statistics
    atomic_size_t jobs_completed;
    atomic_size_t jobs_failed;
};

struct edge_sched_thread_context {
    edge_sched_t* scheduler;

    edge_job_t* current_job;
    edge_job_t main_job;
    edge_fiber_context_t* main_context;

    int thread_id;
};

static _Thread_local struct edge_sched_thread_context thread_context = { 0 };

static void edge_sched_init_thread_context(worker_thread_t* worker) {
    thread_context.scheduler = worker->scheduler;
    thread_context.main_context = edge_fiber_context_create(worker->scheduler->allocator, NULL, NULL, 0);
    thread_context.main_job.context = thread_context.main_context;
    thread_context.current_job = &thread_context.main_job;
    thread_context.thread_id = worker->thread_id;

    thread_context.main_job.state = EDGE_CORO_STATE_RUNNING;
}

static void edge_sched_shutdown_thread_contex() {
    if (thread_context.main_context) {
        edge_fiber_context_destroy(thread_context.scheduler->allocator, thread_context.main_context);
    }
}

static void* edge_sched_alloc_stack_ptr(edge_sched_t* sched) {
    if (!sched) {
        return NULL;
    }

    edge_mtx_lock(&sched->stack_mutex);

    uintptr_t ptr_addr = 0;
    if (!edge_list_pop_back(sched->free_stacks, &ptr_addr)) {
        edge_mtx_unlock(&sched->stack_mutex);
        return edge_arena_alloc_ex(sched->protected_arena, EDGE_FIBER_STACK_SIZE, EDGE_FIBER_STACK_ALIGN);
    }
    edge_mtx_unlock(&sched->stack_mutex);

    return (void*)ptr_addr;
}

static void edge_sched_free_stack_ptr(edge_sched_t* sched, void* ptr) {
    if (!sched || !ptr) {
        return;
    }

    edge_mtx_lock(&sched->stack_mutex);

    uintptr_t ptr_addr = (uintptr_t)ptr;
    (void)edge_list_push_back(sched->free_stacks, &ptr_addr);

    edge_mtx_unlock(&sched->stack_mutex);
}

static void edge_job_main(void) {
    edge_job_t* job = edge_sched_current_job();

    /* Run the job function */
    if (job && job->func) {
        job->state = EDGE_CORO_STATE_RUNNING;
        job->func(job->user_data);
        job->state = EDGE_CORO_STATE_FINISHED;
    }

    /* Return to caller */
    if (job && job->caller) {
        edge_fiber_context_switch(job->context, job->caller->context);
    }

    /* Should never reach here */
    assert(0 && "Job returned without caller");
}

static edge_job_t* edge_job_create(edge_sched_t* sched, edge_coro_fn func, void* payload) {
    if (!sched || !func) {
        return NULL;
    }

    edge_job_t* job = (edge_job_t*)edge_allocator_calloc(sched->allocator, 1, sizeof(edge_job_t));
    if (!job) {
        return NULL;
    }

    void* stack_ptr = edge_sched_alloc_stack_ptr(sched);
    if (!stack_ptr) {
        goto failed;
    }

    job->context = edge_fiber_context_create(sched->allocator, edge_job_main, stack_ptr, EDGE_FIBER_STACK_SIZE);
    if (!job->context) {
        goto failed;
    }

    job->state = EDGE_CORO_STATE_SUSPENDED;
    job->caller = NULL;
    job->func = func;
    job->user_data = payload;

    job->priority = EDGE_SCHED_PRIORITY_LOW;

    return job;

failed:
    if (job) {
        if (job->context) {
            void* stack_ptr = edge_fiber_get_stack_ptr(job->context);
            if (stack_ptr) {
                edge_sched_free_stack_ptr(sched, stack_ptr);
            }

            edge_fiber_context_destroy(sched->allocator, job->context);
        }

        edge_allocator_free(sched->allocator, job);
    }

    return NULL;
}

static void edge_job_destroy(edge_sched_t* sched, edge_job_t* job) {
    if (!sched || !job) {
        return;
    }

    if (job->context) {
        void* stack_ptr = edge_fiber_get_stack_ptr(job->context);
        if (stack_ptr) {
            edge_sched_free_stack_ptr(sched, stack_ptr);
        }

        edge_fiber_context_destroy(sched->allocator, job->context);
    }

    edge_allocator_free(sched->allocator, job);
}

static bool edge_job_state_update(edge_job_t* job, edge_coro_state_t expected_state, edge_coro_state_t new_state) {
    if (job->state == expected_state) {
        job->state = new_state;
        return true;
    }
    return false;
}

static edge_job_t* edge_sched_pick_job(edge_sched_t* sched) {
    if (!sched) {
        return NULL;
    }

    edge_mtx_lock(&sched->queue_mutex);

    edge_job_t* job = NULL;

    /* Pick highest priority job */
    for (int i = EDGE_SCHED_PRIORITY_COUNT - 1; i >= 0; i--) {
        uintptr_t job_addr;
        if (edge_queue_dequeue(sched->queues[i], &job_addr)) {
            job = (edge_job_t*)job_addr;
            atomic_fetch_sub_explicit(&sched->queued_jobs, 1, memory_order_release);
            break;
        }
    }

    edge_mtx_unlock(&sched->queue_mutex);

    return job;
}

static void edge_sched_enqueue_job(edge_sched_t* sched, edge_job_t* job) {
    if (!sched || !job) {
        return;
    }

    edge_mtx_lock(&sched->queue_mutex);

    uintptr_t job_addr = (uintptr_t)job;
    bool enqueued = edge_queue_enqueue(sched->queues[job->priority], &job_addr);

    if (enqueued) {
        atomic_fetch_add_explicit(&sched->queued_jobs, 1, memory_order_release);
        edge_cnd_broadcast(&sched->queue_cond);
    }

    edge_mtx_unlock(&sched->queue_mutex);
}

static int sched_worker_thread(void* payload) {
    worker_thread_t* worker = (worker_thread_t*)payload;
    edge_sched_t* sched = worker->scheduler;

    edge_sched_init_thread_context(worker);

    while (!atomic_load_explicit(&worker->should_exit, memory_order_acquire)) {
        edge_job_t* job = edge_sched_pick_job(sched);
        if (!job) {
            if (atomic_load_explicit(&sched->shutdown, memory_order_acquire)) {
                break;
            }

            /* Wait for work */
            edge_mtx_lock(&sched->queue_mutex);

            if (atomic_load_explicit(&sched->queued_jobs, memory_order_acquire) == 0) {
                struct timespec ts;
                timespec_get(&ts, TIME_UTC);
                ts.tv_nsec += 10000000; /* 10ms */
                if (ts.tv_nsec >= 1000000000) {
                    ts.tv_sec++;
                    ts.tv_nsec -= 1000000000;
                }
                edge_cnd_timedwait(&sched->queue_cond, &sched->queue_mutex, &ts);
            }

            edge_mtx_unlock(&sched->queue_mutex);
            continue;
        }

        bool claimed = edge_job_state_update(job, EDGE_CORO_STATE_SUSPENDED, EDGE_CORO_STATE_RUNNING);
        if (!claimed) {
            atomic_fetch_add_explicit(&sched->jobs_failed, 1, memory_order_relaxed);
            continue;
        }

        edge_job_t* caller = edge_sched_current_job();
        job->caller = caller;

        caller->state = EDGE_CORO_STATE_SUSPENDED;

        edge_job_t* volatile target_job = job;
        thread_context.current_job = target_job;

        edge_fiber_context_switch(caller->context, job->context);

        /* When we return here, job has yielded or finished */
        thread_context.current_job = caller;
        caller->state = EDGE_CORO_STATE_RUNNING;

        edge_coro_state_t job_state = job->state;
        if (job_state == EDGE_CORO_STATE_FINISHED) {
            edge_job_destroy(sched, job);

            size_t prev_active = atomic_fetch_sub_explicit(&sched->active_jobs, 1, memory_order_acq_rel);
            atomic_fetch_add_explicit(&sched->jobs_completed, 1, memory_order_relaxed);

            if (prev_active == 1) {
                edge_mtx_lock(&sched->queue_mutex);
                edge_cnd_broadcast(&sched->queue_cond);
                edge_mtx_unlock(&sched->queue_mutex);
            }
        }
        else if (job_state == EDGE_CORO_STATE_SUSPENDED) {
            /* Re-queue for later */
            edge_sched_enqueue_job(sched, job);
        }
    }

    edge_sched_shutdown_thread_contex();

    return 0;
}

edge_sched_t* edge_sched_create(edge_allocator_t* allocator) {
    if (!allocator) {
        return NULL;
    }

    edge_sched_t* sched = (edge_sched_t*)edge_allocator_calloc(allocator, 1, sizeof(edge_sched_t));
    if (!sched) {
        return NULL;
    }

    sched->allocator = allocator;

    sched->protected_arena = edge_arena_create(allocator, 0);
    if (!sched->protected_arena) {
        goto failed;
    }

    sched->free_stacks = edge_list_create(allocator, sizeof(uintptr_t));
    if (!sched->free_stacks) {
        goto failed;
    }

    if (edge_mtx_init(&sched->stack_mutex, edge_mtx_plain) != edge_thrd_success) {
        goto failed;
    }

    for (int i = 0; i < EDGE_SCHED_PRIORITY_COUNT; ++i) {
        edge_queue_t* queue = edge_queue_create(allocator, sizeof(uintptr_t), 16);
        if (!queue) {
            goto failed;
        }
        sched->queues[i] = queue;
    }

    if (edge_mtx_init(&sched->queue_mutex, edge_mtx_plain) != edge_thrd_success) {
        goto failed;
    }

    if (edge_cnd_init(&sched->queue_cond) != edge_thrd_success) {
        goto failed;
    }

    edge_cpu_info_t cpu_info[256];
    int cpu_count = edge_thrd_get_cpu_topology(cpu_info, 256);

    int num_cores = edge_thrd_get_logical_core_count(cpu_info, cpu_count);
    if (num_cores <= 0) {
        num_cores = 4;
    }

    sched->worker_threads = edge_vector_create(allocator, sizeof(worker_thread_t), num_cores);
    if (!sched->worker_threads) {
        goto failed;
    }

    if (!edge_vector_resize(sched->worker_threads, num_cores)) {
        goto failed;
    }

    atomic_init(&sched->shutdown, false);
    atomic_init(&sched->active_jobs, 0);
    atomic_init(&sched->queued_jobs, 0);
    atomic_init(&sched->jobs_completed, 0);
    atomic_init(&sched->jobs_failed, 0);

    for (int i = 0; i < num_cores; ++i) {
        worker_thread_t* worker = (worker_thread_t*)edge_vector_at(sched->worker_threads, i);
        worker->scheduler = sched;
        worker->thread_id = i;
        atomic_init(&worker->should_exit, false);

        if (edge_thrd_create(&worker->thread, sched_worker_thread, worker) != edge_thrd_success) {
            edge_vector_resize(sched->worker_threads, i);
            goto failed;
        }

        edge_thrd_set_affinity_ex(worker->thread, cpu_info, cpu_count, i, false);

        char buffer[32] = { 0 };
        snprintf(buffer, sizeof(buffer), "worker-%d", i);
        edge_thrd_set_name(worker->thread, buffer);
    }

    return sched;

failed:
    if (sched) {
        if (sched->worker_threads) {
            for (int i = 0; i < edge_vector_size(sched->worker_threads); ++i) {
                worker_thread_t* worker = (worker_thread_t*)edge_vector_at(sched->worker_threads, i);
                atomic_store_explicit(&worker->should_exit, true, memory_order_release);
            }

            atomic_store_explicit(&sched->shutdown, true, memory_order_release);
            edge_cnd_broadcast(&sched->queue_cond);

            for (size_t i = 0; i < edge_vector_size(sched->worker_threads); ++i) {
                worker_thread_t* worker = (worker_thread_t*)edge_vector_at(sched->worker_threads, i);
                edge_thrd_join(worker->thread, NULL);
            }

            edge_vector_destroy(sched->worker_threads);
        }

        edge_cnd_destroy(&sched->queue_cond);
        edge_mtx_destroy(&sched->queue_mutex);

        for (int i = 0; i < EDGE_SCHED_PRIORITY_COUNT; ++i) {
            edge_queue_t* queue = sched->queues[i];
            if (queue) {
                edge_queue_destroy(queue);
            }
        }

        edge_mtx_destroy(&sched->stack_mutex);

        if (sched->free_stacks) {
            edge_list_destroy(sched->free_stacks);
        }

        if (sched->protected_arena) {
            edge_arena_destroy(sched->protected_arena);
        }

        edge_allocator_free(allocator, sched);
    }

    return NULL;
}

void edge_sched_destroy(edge_sched_t* sched) {
    if (!sched) {
        return;
    }

    atomic_store_explicit(&sched->shutdown, true, memory_order_release);
    edge_cnd_broadcast(&sched->queue_cond);

    if (sched->worker_threads) {
        for (int i = 0; i < edge_vector_size(sched->worker_threads); ++i) {
            worker_thread_t* worker = (worker_thread_t*)edge_vector_at(sched->worker_threads, i);
            atomic_store_explicit(&worker->should_exit, true, memory_order_release);
            edge_thrd_join(worker->thread, NULL);
        }

        edge_vector_destroy(sched->worker_threads);
    }

    edge_cnd_destroy(&sched->queue_cond);
    edge_mtx_destroy(&sched->queue_mutex);

    for (int i = 0; i < EDGE_SCHED_PRIORITY_COUNT; ++i) {
        edge_queue_t* queue = sched->queues[i];
        if (queue) {
            while (true) {
                uintptr_t job_addr;
                if (!edge_queue_dequeue(queue, &job_addr)) {
                    break;
                }

                edge_job_t* job = (edge_job_t*)job_addr;
                edge_job_destroy(sched, job);
            }

            edge_queue_destroy(queue);
        }
    }

    edge_mtx_destroy(&sched->stack_mutex);

    if (sched->free_stacks) {
        edge_list_destroy(sched->free_stacks);
    }

    if (sched->protected_arena) {
        edge_arena_destroy(sched->protected_arena);
    }

    edge_allocator_t* alloc = sched->allocator;
    edge_allocator_free(alloc, sched);
}

void edge_sched_schedule_job(edge_sched_t* sched, edge_coro_fn func, void* payload, edge_sched_priority_t priority) {
    if (!sched || !func) {
        return;
    }

    edge_job_t* job = edge_job_create(sched, func, payload);
    if (!job) {
        return;
    }

    job->priority = priority;

    atomic_fetch_add_explicit(&sched->active_jobs, 1, memory_order_relaxed);
    edge_sched_enqueue_job(sched, job);
}

edge_job_t* edge_sched_current_job(void) {
    return thread_context.current_job;
}

int edge_sched_current_thread_id(void) {
    return thread_context.thread_id;
}

void edge_sched_run(edge_sched_t* sched) {
    if (!sched) {
        return;
    }

    edge_mtx_lock(&sched->queue_mutex);

    while (
        atomic_load_explicit(&sched->active_jobs, memory_order_acquire) > 0 &&
        !atomic_load_explicit(&sched->shutdown, memory_order_acquire)) {
        edge_cnd_wait(&sched->queue_cond, &sched->queue_mutex);
    }

    edge_mtx_unlock(&sched->queue_mutex);
}

void edge_sched_yield(void) {
    edge_job_t* job = thread_context.current_job;

    edge_coro_state_t job_state = job->state;
    if (!job || job == &thread_context.main_job || job_state == EDGE_CORO_STATE_FINISHED) {
        return;
    }

    edge_job_t* caller = job->caller;
    if (!caller) {
        return;
    }

    // TODO: Add helper function bool edge_job_state_exchange(job, expected_state, new_state)
    job->state = EDGE_CORO_STATE_SUSPENDED;
    caller->state = EDGE_CORO_STATE_RUNNING;

    edge_fiber_context_switch(job->context, caller->context);

    /* When resumed, we continue here */
    job->state = EDGE_CORO_STATE_RUNNING;
}

void edge_sched_await(edge_coro_fn func, void* payload, edge_sched_priority_t priority) {
    if (!func) {
        return;
    }

    edge_job_t* parent = edge_sched_current_job();
    // TODO: Not implemented for now
}