#include <scheduler.hpp>

#include <assert.h>
#include <stdio.h>

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
    edge::sched_schedule_job(sched, edge::callable_create_from_lambda(
        edge::sched_get_allocator(sched), [completion_event]() {
            int thread_id = edge::sched_current_thread_id();
            printf("[Thread %d] [Job A] Downloading began.\n", thread_id);

            for (int i = 0; i < 100; ++i) {
                printf("[Thread %d] [Job A] Progress: %d%%\n", thread_id, i);
                edge::sched_yield();
            }

            edge::sched_event_signal(completion_event);
        }), edge::SchedulerPriority::Critical);

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

int main(void) {
    edge::Allocator allocator = edge::Allocator::create_tracking();

    edge::Scheduler* sched = edge::sched_create(&allocator);

    edge::sched_schedule_job(sched, 
        edge::callable_create_from_lambda(
            &allocator, []() -> void {
                job_b(nullptr);
        }), edge::SchedulerPriority::Critical);

    edge::sched_run(sched);

    edge::sched_destroy(sched);

    size_t alloc_net = allocator.get_net();
    assert(alloc_net == 0 && "Memory leaks detected, some data was not freed.");

    return 0;
}
