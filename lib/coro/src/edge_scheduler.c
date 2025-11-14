#include "edge_scheduler.h"

#include <edge_allocator.h>

#include <stdatomic.h>
#include <string.h>

/* Priority queue for each priority level */
typedef struct job_queue {
	edge_job_t* head;
	edge_job_t* tail;
	size_t count;
} job_queue_t;

struct edge_job {
	edge_coro_t* coro;
	edge_priority_t priority;
	struct edge_job* next;
	bool in_queue;
};

struct edge_scheduler {
	edge_allocator_t* allocator;

	job_queue_t queues[EDGE_PRIORITY_COUNT];

	atomic_size_t active_jobs;
};

static void job_queue_init(job_queue_t* queue) {
	queue->head = NULL;
	queue->tail = NULL;
	queue->count = 0;
}

static void job_queue_push(job_queue_t* queue, edge_job_t* job) {
	job->next = NULL;
	if (!queue->head) {
		queue->head = job;
		queue->tail = job;
	}
	else {
		queue->tail->next = job;
		queue->tail = job;
	}
	queue->count++;
}

static edge_job_t* job_queue_pop(job_queue_t* queue) {
	if (!queue->head) {
		return NULL;
	}

	edge_job_t* job = queue->head;
	queue->head = job->next;
	if (!queue->head) {
		queue->tail = NULL;
	}
	queue->count--;
	job->next = NULL;
	return job;
}

static edge_job_t* job_queue_remove(job_queue_t* queue, edge_job_t* job) {
	edge_job_t* prev = NULL;
	edge_job_t* current = queue->head;

	while (current) {
		if (current == job) {
			if (prev) {
				prev->next = current->next;
			}
			else {
				queue->head = current->next;
			}

			if (current == queue->tail) {
				queue->tail = prev;
			}

			queue->count--;
			current->next = NULL;
			return current;
		}

		prev = current;
		current = current->next;
	}

	return NULL;
}

static edge_job_t* scheduler_pick_job(edge_scheduler_t* sched) {
	/* Pick highest priority job */
	for (int i = EDGE_PRIORITY_COUNT - 1; i >= 0; i--) {
		edge_job_t* job = job_queue_pop(&sched->queues[i]);
		if (job) {
			job->in_queue = false;
			return job;
		}
	}

	return NULL;
}

static void scheduler_enqueue_job(edge_scheduler_t* sched, edge_job_t* job) {
	if (!job->in_queue) {
		job_queue_push(&sched->queues[job->priority], job);
		job->in_queue = true;
	}
}

edge_scheduler_t* edge_scheduler_create(edge_allocator_t* allocator) {
	if (!allocator) {
		return NULL;
	}

	edge_scheduler_t* sched = (edge_scheduler_t*)edge_allocator_malloc(allocator, sizeof(edge_scheduler_t));
	if (!sched) {
		return NULL;
	}

	sched->allocator = allocator;

	for (int i = 0; i < EDGE_PRIORITY_COUNT; i++) {
		job_queue_init(&sched->queues[i]);
	}

	atomic_init(&sched->active_jobs, 0);

	return sched;
}

void edge_scheduler_destroy(edge_scheduler_t* sched) {
	if (!sched) {
		return;
	}

	/* Destroy all remaining jobs */
	for (int i = 0; i < EDGE_PRIORITY_COUNT; i++) {
		edge_job_t* current = sched->queues[i].head;
		while (current) {
			edge_job_t* next = current->next;
			edge_scheduler_destroy_job(sched, current);
			current = next;
		}
	}

	edge_allocator_t* alloc = sched->allocator;
	edge_allocator_free(alloc, sched);
}

edge_job_t* edge_scheduler_create_job(edge_scheduler_t* sched, edge_coro_fn func, void* payload, edge_priority_t priority) {
	if (!sched || !func) {
		return NULL;
	}

	edge_job_t* job = (edge_job_t*)edge_allocator_malloc(sched->allocator, sizeof(edge_job_t));
	if (!job) {
		return NULL;
	}

	memset(job, 0, sizeof(edge_job_t));

	job->coro = edge_coro_create(func, payload);
	if (!job->coro) {
		edge_allocator_free(sched->allocator, job);
		return NULL;
	}

	job->priority = priority;
	job->next = NULL;
	job->in_queue = false;

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

	atomic_fetch_add(&sched->active_jobs, 1);
	scheduler_enqueue_job(sched, job);

	return 0;
}

int edge_scheduler_remove_job(edge_scheduler_t* sched, edge_job_t* job) {
	if (!sched || !job) {
		return -1;
	}

	edge_job_t* removed = job_queue_remove(&sched->queues[job->priority], job);
	if (removed) {
		removed->in_queue = false;
	}

	return removed ? 0 : -1;
}

int edge_scheduler_step(edge_scheduler_t* sched) {
	if (!sched) {
		return 0;
	}

	edge_job_t* job = scheduler_pick_job(sched);
	if (!job) {
		return 0;
	}

	edge_coro_t* coro = job->coro;
	if (!coro) {
		return 0;
	}

	edge_coro_state_t state = edge_coro_state(coro);
	if (state == EDGE_CORO_STATE_READY || state == EDGE_CORO_STATE_SUSPENDED) {
		int result = edge_coro_resume(job->coro);
		if (result < 0) {
			return -1;
		}

		state = edge_coro_state(job->coro);
		if (state == EDGE_CORO_STATE_FINISHED) {
			/* Clean up */
			edge_scheduler_destroy_job(sched, job);
			atomic_fetch_sub(&sched->active_jobs, 1);
		}
		else if (state == EDGE_CORO_STATE_SUSPENDED) {
			/* Re-queue for later */
			scheduler_enqueue_job(sched, job);
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

	return atomic_load(&sched->active_jobs) > 0;
}

size_t edge_scheduler_job_count(const edge_scheduler_t* sched) {
	if (!sched) {
		return 0;
	}

	size_t count = 0;
	for (int i = 0; i < EDGE_PRIORITY_COUNT; i++) {
		count += sched->queues[i].count;
	}

	return count;
}