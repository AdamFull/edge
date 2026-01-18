#include <scheduler.hpp>

#include <assert.h>
#include <stdio.h>

static edge::Allocator allocator = {};

#if 0
static void job_b(void* payload) {
    int thread_id = edge::sched_current_thread_id();
    printf("[Thread %d] Job B Online\n", thread_id);

    for (int i = 0; i < 100; ++i) {
        printf("[Thread %d] [Job B] Preparing request: %d%%\n", thread_id, i);
        edge::sched_yield();
        thread_id = edge::sched_current_thread_id();
    }

    thread_id = edge::sched_current_thread_id();

    edge::SchedulerEvent* completion_event = edge::sched_event_create();

    edge::Scheduler* sched = edge::sched_current_instance();
    edge::Job* new_job = edge::sched_create_job(sched, edge::callable_create_from_lambda(
        edge::sched_get_allocator(sched), [completion_event]() {
            int thread_id = edge::sched_current_thread_id();
            printf("[Thread %d] [Job A] Downloading began.\n", thread_id);

            for (int i = 0; i < 100; ++i) {
                printf("[Thread %d] [Job A] Progress: %d%%\n", thread_id, i);
                edge::sched_yield();
            }

            edge::sched_event_signal(completion_event);
        }), edge::SchedulerPriority::Critical);
    edge::sched_schedule_job(sched, new_job);

    printf("[Thread %d] [Job B] waiting subtask completion\n", thread_id);
    edge::sched_event_wait(completion_event);
    printf("[Thread %d] [Job B] subtask completed\n", thread_id);

    edge::sched_event_destroy(completion_event);

    for (int i = 0; i < 100; i += 2) {
        printf("[Thread %d] [Job B] Processing downloaded data: %d%%\n", thread_id, i);
        edge::sched_yield();
        thread_id = edge::sched_current_thread_id();
    }

    thread_id = edge::sched_current_thread_id();
    printf("[Thread %d] [Job B] Shutdown\n", thread_id);
}
#endif

enum class IOError {
    UnknownError,
    FileNotFound,
    Timeout
};

static void job_b(i32 mult) {
    int thread_id = edge::job_thread_id();
    printf("[Thread %d] Job B Online\n", thread_id);

    i32 sum = 0;

    for (int i = 0; i < 100; ++i) {
        sum += i + (i * mult);
        edge::job_yield();
        thread_id = edge::job_thread_id();
    }

    edge::job_return<i32, IOError>(sum);
}

static void job_a() {
    int thread_id = edge::job_thread_id();
    printf("[Thread %d] Job A Online\n", thread_id);

    for (int i = 0; i < 100; ++i) {
        printf("[Thread %d] [Job A] Preparing request: %d%%\n", thread_id, i);

        edge::Job* subjob = edge::Job::from_lambda(&allocator,
            [&i]() -> void { job_b(i); }, edge::SchedulerPriority::Critical);

        edge::JobPromise<i32, IOError> result = {};
        edge::job_await(subjob, &result);

        thread_id = edge::job_thread_id();
    }
}

int main(void) {
    allocator = edge::Allocator::create_tracking();

    edge::Scheduler* sched = edge::Scheduler::create(&allocator);
    if (!sched) {
        return -1;
    }

#if 0
    edge::Job* new_job = edge::sched_create_job(sched,
        edge::callable_create_from_lambda(
            &allocator, []() -> void {
                job_b(nullptr);
            }), edge::SchedulerPriority::Critical);

#endif

    edge::Job* new_job = edge::Job::from_lambda(&allocator,
        []() -> void { job_a(); }, edge::SchedulerPriority::Critical);

    sched->schedule(new_job);
    sched->run();
    edge::Scheduler::destroy(&allocator, sched);

    size_t alloc_net = allocator.get_net();
    assert(alloc_net == 0 && "Memory leaks detected, some data was not freed.");

    return 0;
}
