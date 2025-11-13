#include "edge_scheduler.h"

#include <edge_allocator.h>

struct edge_job {
    edge_coro_t* coro;
	struct edge_job* next;
};

struct edge_scheduler {
	edge_allocator_t* allocator;

	edge_job_t* head;
	edge_job_t* tail;
	int count;
};

edge_scheduler_t* edge_scheduler_create(edge_allocator_t* allocator) {
	if (!allocator) {
		return NULL;
	}

	edge_scheduler_t* sched = (edge_scheduler_t*)edge_allocator_malloc(allocator, sizeof(edge_scheduler_t));
	if (!sched) {
		return NULL;
	}

	sched->allocator = allocator;
	sched->head = NULL;
	sched->tail = NULL;
	sched->count = 0;
}

void edge_scheduler_destroy(edge_scheduler_t* sched) {
	if (!sched) {
		return;
	}

	/* Destroy all jobs */
	edge_job_t* current = sched->head;
	while (current) {
		edge_job_t* next = current->next;
		edge_scheduler_destroy_job(sched, current);
		current = next;
	}

	edge_allocator_t* alloc = sched->allocator;
	edge_allocator_free(alloc, sched);
}

edge_job_t* edge_scheduler_create_job(edge_scheduler_t* sched, edge_coro_fn func, void* payload) {
	if (!sched || !func) {
		return NULL;
	}

	edge_job_t* job = (edge_job_t*)edge_allocator_malloc(sched->allocator, sizeof(edge_job_t));
	if (!job) {
		return NULL;
	}

	job->coro = edge_coro_create(func, payload);
	job->next = NULL;

	return job;
}

void edge_scheduler_destroy_job(edge_scheduler_t* sched, edge_job_t* job) {
	if (!sched || !job) {
		return;
	}

	if (job->coro) {
		edge_coro_destroy(job->coro);
	}

	edge_allocator_free(sched->allocator, job);
}

int edge_scheduler_add_job(edge_scheduler_t* sched, edge_job_t* job) {
	if (!sched || !job) {
		return -1;
	}

	if (!sched->head) {
		sched->head = job;
		sched->tail = job;
	}
	else {
		sched->tail->next = job;
		sched->tail = job;
	}

	sched->count++;
	return 0;
}

int edge_scheduler_remove_job(edge_scheduler_t* sched, edge_job_t* job) {
	if (!sched || !job) {
		return -1;
	}

	edge_job_t* prev = NULL;
	edge_job_t* current = sched->head;

	while (current) {
		if (current == job) {
			if (prev) {
				prev->next = current->next;
			}
			else {
				sched->head = current->next;
			}

			if (current == sched->tail) {
				sched->tail = prev;
			}

			sched->count--;
			return 0;
		}

		prev = current;
		current = current->next;
	}

	return -1;
}

int edge_scheduler_step(edge_scheduler_t* sched) {
	if (!sched || !sched->head) {
		return 0;
	}

	edge_job_t* job = sched->head;

	edge_coro_state_t state = edge_coro_state(job->coro);
	if (state == EDGE_CORO_STATE_READY || state == EDGE_CORO_STATE_SUSPENDED) {
		int result = edge_coro_resume(job->coro);
		if (result < 0) {
			return -1;
		}

		state = edge_coro_state(job->coro);
		if (state == EDGE_CORO_STATE_FINISHED) {
			/* Clean up */
			edge_scheduler_remove_job(sched, job);
			edge_scheduler_destroy_job(sched, job);
		}
		else if (state == EDGE_CORO_STATE_SUSPENDED) {
			/* Move to tail */
			if (sched->head != sched->tail) {
				sched->head = job->next;
				job->next = NULL;
				sched->tail->next = job;
				sched->tail = job;
			}
		}
		return 1;
	}

	return 0;
}

int edge_scheduler_run(edge_scheduler_t* sched) {
	if (!sched) {
		return -1;
	}

	while (edge_scheduler_has_active(sched)) {
		int result = edge_scheduler_step(sched);
		if (result < 0) {
			return -1;
		}
	}

	return 0;
}

bool edge_scheduler_has_active(const edge_scheduler_t* sched) {
	if (!sched) {
		return false;
	}

	edge_job_t* job = sched->head;
	while (job) {
		edge_coro_state_t state = edge_coro_state(job->coro);
		if (state != EDGE_CORO_STATE_FINISHED) {
			return true;
		}
		job = job->next;
	}

	return false;
}

size_t edge_scheduler_job_count(const edge_scheduler_t* sched) {
	return sched ? sched->count : 0;
}