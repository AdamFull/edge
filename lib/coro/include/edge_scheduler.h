#ifndef EDGE_SCHEDULER_H
#define EDGE_SCHEDULER_H

#include "edge_coro.h"

#ifdef __cplusplus
extern "C" {
#endif

	typedef struct edge_job edge_job_t;
	typedef struct edge_scheduler edge_scheduler_t;

	/**
	 * @brief Create a new scheduler
	 * @param allocator Memory allocator
	 * @return Scheduler handle
	 */
	edge_scheduler_t* edge_scheduler_create(edge_allocator_t* allocator);

	/**
	 * @brief Destroy a scheduler
	 * @param sched Scheduler handle
	 */
	void edge_scheduler_destroy(edge_scheduler_t* sched);

	/**
	 * @brief Create a new job handle
	 * @param sched Scheduler handle
	 * @param func Workload function pointer
	 * @param payload Payload pointer
	 * @return Job handle
	 */
	edge_job_t* edge_scheduler_create_job(edge_scheduler_t* sched, edge_coro_fn func, void* payload);

	/**
	 * @brief Destroy a job handle
	 * @param sched Scheduler handle
	 * @param job Job handle
	 */
	void edge_scheduler_destroy_job(edge_scheduler_t* sched, edge_job_t* job);

	/**
	 * @brief Add job to scheduler
	 * @param sched Scheduler handle
	 * @param job Job handle
	 * @return Adding status (<0 is bad)
	 */
	int edge_scheduler_add_job(edge_scheduler_t* sched, edge_job_t* job);

	/**
	 * @brief Remove job from scheduler
	 * @param sched Scheduler handle
	 * @param job Job handle
	 * @return Removing status (<0 is bad)
	 */
	int edge_scheduler_remove_job(edge_scheduler_t* sched, edge_job_t* job);

	/**
	 * @brief Do scheduler step
	 * @param sched Scheduler handle
	 * @return Scheduler step result (<0 is bad)
	 */
	int edge_scheduler_step(edge_scheduler_t* sched);

	/**
	 * @brief Process all jobs added to scheduler
	 * @param sched Scheduler handle
	 * @return Scheduler run result (<0 is bad)
	 */
	int edge_scheduler_run(edge_scheduler_t* sched);

	/**
	 * @brief Check for active jobs in flight
	 * @param sched Scheduler handle
	 * @return Has active or not
	 */
	bool edge_scheduler_has_active(const edge_scheduler_t* sched);

	/**
	 * @brief Ask for job count in scheduler
	 * @param sched Scheduler handle
	 * @return Number of jobs attached to scheduler
	 */
	size_t edge_scheduler_job_count(const edge_scheduler_t* sched);

#ifdef __cplusplus
}
#endif

#endif /* EDGE_SCHEDULER_H */