#ifndef EDGE_SCHEDULER_HPP
#define EDGE_SCHEDULER_HPP

#include <callable.hpp>

namespace edge {
	struct Job;
	struct Scheduler;
	struct SchedulerEvent;

	enum class JobState {
		Running = 1,
		Suspended = 2,
		Finished = 3
	};

	enum class SchedulerPriority {
		Low = 0,
		Normal = 1,
		High = 2,
		Critical = 3,
		Count
	};

	using JobFn = Callable<void()>;

	SchedulerEvent* sched_event_create(void);
	void sched_event_destroy(SchedulerEvent* event);
	void sched_event_wait(SchedulerEvent* event);
	void sched_event_signal(SchedulerEvent* event);
	bool sched_event_signalled(SchedulerEvent* event);

	Scheduler* sched_create(const Allocator* allocator);
	void sched_destroy(Scheduler* sched);

	void sched_schedule_job(Scheduler* sched, JobFn func, SchedulerPriority priority);

	Job* sched_current_job(void);
	i32 sched_current_thread_id(void);
	Scheduler* sched_current_instance(void);
	const Allocator* sched_get_allocator(Scheduler* sched);

	void sched_run(Scheduler* sched);
	void sched_yield(void);
}

#endif