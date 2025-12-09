#ifndef EDGE_SCHEDULER_HPP
#define EDGE_SCHEDULER_HPP

#include "coro.hpp"

namespace edge {
	struct Job;
	struct Scheduler;
	struct SchedulerEvent;

	enum class SchedulerPriority {
		Low = 0,
		Normal = 1,
		High = 2,
		Critical = 3,
		Count
	};

	SchedulerEvent* sched_event_create(void);
	void sched_event_destroy(SchedulerEvent* event);
	void sched_event_wait(SchedulerEvent* event);
	void sched_event_signal(SchedulerEvent* event);
	bool sched_event_signalled(SchedulerEvent* event);

	Scheduler* sched_create(const Allocator* allocator);
	void sched_destroy(Scheduler* sched);

	void sched_schedule_job(Scheduler* sched, CoroFn func, void* payload, SchedulerPriority priority);

	Job* sched_current_job(void);
	i32 sched_current_thread_id(void);
	Scheduler* sched_current_instance(void);

	void sched_run(Scheduler* sched);
	void sched_yield(void);

	void sched_await(CoroFn func, void* payload, SchedulerPriority priority);
}

#endif