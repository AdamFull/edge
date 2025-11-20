#ifndef EDGE_SCHEDULER_H
#define EDGE_SCHEDULER_H

#include "edge_coro.h"

#ifdef __cplusplus
extern "C" {
#endif

	typedef struct edge_allocator edge_allocator_t;
	typedef struct edge_job edge_job_t;
	typedef struct edge_sched edge_sched_t;

	typedef struct edge_sched_event edge_sched_event_t;

	typedef enum {
		EDGE_SCHED_PRIORITY_LOW = 0,
		EDGE_SCHED_PRIORITY_NORMAL = 1,
		EDGE_SCHED_PRIORITY_HIGH = 2,
		EDGE_SCHED_PRIORITY_CRITICAL = 3,
		EDGE_SCHED_PRIORITY_COUNT
	} edge_sched_priority_t;

	edge_sched_event_t* edge_sched_event_create(void);
	void edge_sched_event_destroy(edge_sched_event_t* event);
    void edge_sched_event_wait(edge_sched_event_t* event);
    void edge_sched_event_signal(edge_sched_event_t* event);
    bool edge_sched_event_signalled(edge_sched_event_t* event);

	edge_sched_t* edge_sched_create(edge_allocator_t* allocator);

	void edge_sched_destroy(edge_sched_t* sched);

	void edge_sched_schedule_job(edge_sched_t* sched, edge_coro_fn func, void* payload, edge_sched_priority_t priority);

	edge_job_t* edge_sched_current_job(void);

	int edge_sched_current_thread_id(void);

	edge_sched_t* edge_sched_current_instance(void);

	void edge_sched_run(edge_sched_t* sched);

	void edge_sched_yield(void);

	void edge_sched_await(edge_coro_fn func, void* payload, edge_sched_priority_t priority);

#ifdef __cplusplus
}
#endif

#endif /* EDGE_SCHEDULER_H */