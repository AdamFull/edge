#include <edge_scheduler.h>
#include <edge_testing.h>

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

static void job_a(void* payload) {
    edge_sched_event_t* completion_event = (edge_sched_event_t*)payload;
    if (!completion_event) {
        return;
    }

    int thread_id = edge_sched_current_thread_id();
    printf("[Thread %d] [Job A] Downloading began.\n", thread_id);

    for (int i = 0; i < 100; ++i) {
        printf("[Thread %d] [Job A] Progress: %d%%\n", thread_id, i);
        edge_sched_yield();
    }

    edge_sched_event_signal(completion_event);
}

static void job_b(void* payload) {
    int thread_id = edge_sched_current_thread_id();
    printf("[Thread %d] Job B Online\n", thread_id);

    for (int i = 0; i < 100; ++i) {
        printf("[Thread %d] [Job B] Preparing request: %d%%\n", thread_id, i);
        edge_sched_yield();
        thread_id = edge_sched_current_thread_id();
    }

    thread_id = edge_sched_current_thread_id();

    edge_sched_event_t* completion_event = edge_sched_event_create();

    edge_sched_t* sched = edge_sched_current_instance();
    edge_sched_schedule_job(sched, job_a, completion_event, EDGE_SCHED_PRIORITY_CRITICAL);

    printf("[Thread %d] [Job B] waiting subtask completion\n", thread_id);
    edge_sched_event_wait(completion_event);
    printf("[Thread %d] [Job B] subtask completed\n", thread_id);

    edge_sched_event_destroy(completion_event);

    for (int i = 0; i < 100; i += 2) {
        printf("[Thread %d] [Job B] Processing downloaded data: %d%%\n", thread_id, i);
        edge_sched_yield();
        thread_id = edge_sched_current_thread_id();
    }

    thread_id = edge_sched_current_thread_id();
    printf("[Thread %d] [Job B] Shutdown\n", thread_id);
}

int main(void) {
    edge_allocator_t allocator = edge_testing_allocator_create();

    edge_sched_t* sched = edge_sched_create(&allocator);

    edge_sched_schedule_job(sched, job_b, NULL, EDGE_SCHED_PRIORITY_CRITICAL);

    edge_sched_run(sched);

    edge_sched_destroy(sched);

    size_t alloc_net = edge_testing_net_allocated();
    assert(alloc_net == 0 && "Memory leaks detected, some data was not freed.");

    return 0;
}
