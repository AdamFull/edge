#include <edge_coro.h>
#include <edge_scheduler.h>
#include <edge_allocator.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

void task_a(void* arg) {
    printf("Task A: Starting\n");
    edge_coro_yield();
    printf("Task A: Continuing\n");
    edge_coro_yield();
    printf("Task A: Done\n");
}

void task_b(void* arg) {
    printf("Task B: Starting (after A completes)\n");
    edge_coro_yield();
    printf("Task B: Done\n");
}

void task_c(void* arg) {
    printf("Task C: Starting (after both A and B complete)\n");
    printf("Task C: Done\n");
}

int main(void) {
    /* Initialize allocator */
    edge_allocator_t allocator = edge_allocator_create_default();
    edge_coro_init_thread_context(&allocator);

    edge_scheduler_t* sched = edge_scheduler_create(&allocator);

    /* Create jobs */
    edge_job_t* job_a = edge_scheduler_create_job(sched, task_a, NULL, EDGE_PRIORITY_LOW);
    edge_job_t* job_b = edge_scheduler_create_job(sched, task_b, NULL, EDGE_PRIORITY_LOW);
    edge_job_t* job_c = edge_scheduler_create_job(sched, task_c, NULL, EDGE_PRIORITY_CRITICAL);

    /* Add to scheduler (order doesn't matter with dependencies) */
    edge_scheduler_add_job(sched, job_c);
    edge_scheduler_add_job(sched, job_b);
    edge_scheduler_add_job(sched, job_a);

    /* Run scheduler */
    edge_scheduler_run(sched);

    edge_scheduler_destroy(sched);

    edge_coro_shutdown_thread_context();

    return 0;
}