#include <scheduler.hpp>

#include <assert.h>
#include <stdio.h>

static edge::Allocator allocator = {};

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

    edge::job_return(sum);
}

static void job_a() {
    int thread_id = edge::job_thread_id();
    printf("[Thread %d] Job A Online\n", thread_id);

    for (int i = 0; i < 100; ++i) {
        printf("[Thread %d] [Job A] Preparing request: %d%%\n", thread_id, i);

        edge::Job* subjob = edge::Job::from_lambda(&allocator,
            [&i]() -> void { job_b(i); });

        edge::Job::Promise<i32, IOError> result = {};
        subjob->set_promise(&result);
        edge::job_await(subjob);

        thread_id = edge::job_thread_id();
    }

    edge::job_switch_to_main();
    thread_id = edge::job_thread_id();
    printf("[Thread %d] [Job A] Hello from main thread.\n", thread_id);
    edge::job_switch_to_io();
    thread_id = edge::job_thread_id();
    printf("[Thread %d] [Job A] Hello from io thread.\n", thread_id);
    edge::job_switch_to_background();
    thread_id = edge::job_thread_id();
    printf("[Thread %d] [Job A] Hello from background thread.\n", thread_id);
}

int main(void) {
    allocator = edge::Allocator::create_tracking();

    edge::Scheduler* sched = edge::Scheduler::create(&allocator);
    if (!sched) {
        return -1;
    }

    edge::Job* new_job = edge::Job::from_lambda(&allocator,
        []() -> void { job_a(); });

    sched->schedule(new_job, edge::Job::Priority::High);
    sched->run();
    edge::Scheduler::destroy(&allocator, sched);

    size_t alloc_net = allocator.get_net();
    assert(alloc_net == 0 && "Memory leaks detected, some data was not freed.");

    return 0;
}
