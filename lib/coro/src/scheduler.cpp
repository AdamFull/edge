#include "scheduler.hpp"
#include "fiber.hpp"
#include "arena.hpp"
#include "array.hpp"
#include "mpmc_queue.hpp"
#include "threads.hpp"

#include <atomic>
#include <cstdio>
#include <cassert>
#include <ctime>

namespace edge {
	struct Job {
		FiberContext* context;
		CoroFn func;
		void* user_data;
		CoroState state;
		Job* caller;

		SchedulerPriority priority;
	};

	struct WorkerThread {
		Thread thread;
		Scheduler* scheduler;
		usize thread_id;
		std::atomic<bool> should_exit;
	};

	struct Scheduler {
		const Allocator* allocator;

		Arena* protected_arena;
		Array<uintptr_t>* free_stacks;
		Mutex stack_mutex;

		MPMCQueue<uintptr_t>* queues[static_cast<usize>(SchedulerPriority::Count)];

		Array<WorkerThread*>* worker_threads;

		std::atomic<usize> active_jobs;
		std::atomic<usize> queued_jobs;
		std::atomic<bool> shutdown;

		std::atomic<usize> jobs_completed;
		std::atomic<usize> jobs_failed;
	};

	struct SchedulerThreadContext {
		Scheduler* scheduler;

		Job* current_job;
		Job main_job;
		FiberContext* main_context;

		i32 thread_id;
	};

	struct SchedulerEvent {
		std::atomic<bool> done;
	};

	static thread_local SchedulerThreadContext thread_context = { };

	static void sched_init_thread_context(WorkerThread* worker) {
		thread_context.scheduler = worker->scheduler;
		thread_context.main_context = fiber_context_create(worker->scheduler->allocator, nullptr, nullptr, 0);
		thread_context.main_job.context = thread_context.main_context;
		thread_context.current_job = &thread_context.main_job;
		thread_context.thread_id = static_cast<i32>(worker->thread_id);

		thread_context.main_job.state = CoroState::Running;
	}

	static void sched_shutdown_thread_context() {
		if (thread_context.main_context) {
			fiber_context_destroy(thread_context.scheduler->allocator, thread_context.main_context);
		}
	}

	SchedulerEvent* sched_event_create(void) {
		Scheduler* sched = thread_context.scheduler;
		if (!sched || !sched->allocator) {
			return nullptr;
		}

		SchedulerEvent* event = allocate_zeroed<SchedulerEvent>(sched->allocator, 1);
		if (!event) {
			return nullptr;
		}

		event->done.store(false, std::memory_order_relaxed);

		return event;
	}

	void sched_event_destroy(SchedulerEvent* event) {
		Scheduler* sched = thread_context.scheduler;
		if (!sched || !sched->allocator) {
			return;
		}

		if (!event) {
			return;
		}

		deallocate(thread_context.scheduler->allocator, event);
	}

	void sched_event_wait(SchedulerEvent* event) {
		if (!event) {
			return;
		}

		Job* current_job = thread_context.current_job;
		if (!current_job) {
			return;
		}

		SchedulerPriority job_original_prio = current_job->priority;
		current_job->priority = SchedulerPriority::Low;

		while (!event->done.load(std::memory_order_acquire)) {
			sched_yield();
		}

		current_job->priority = job_original_prio;
	}

	void sched_event_signal(SchedulerEvent* event) {
		if (!event) {
			return;
		}

		event->done.store(true, std::memory_order_release);
	}

	bool sched_event_signalled(SchedulerEvent* event) {
		if (!event) {
			return false;
		}

		return event->done.load(std::memory_order_acquire);
	}

	static void* sched_alloc_stack_ptr(Scheduler* sched) {
		if (!sched) {
			return nullptr;
		}

		mutex_lock(&sched->stack_mutex);

		uintptr_t ptr_addr = 0;
		if (!array_pop_back(sched->free_stacks, &ptr_addr)) {
			mutex_unlock(&sched->stack_mutex);
			return arena_alloc_ex(sched->protected_arena, EDGE_FIBER_STACK_SIZE, EDGE_FIBER_STACK_ALIGN);
		}
		mutex_unlock(&sched->stack_mutex);

		return reinterpret_cast<void*>(ptr_addr);
	}

	static void sched_free_stack_ptr(Scheduler* sched, void* ptr) {
		if (!sched || !ptr) {
			return;
		}

		mutex_lock(&sched->stack_mutex);

		uintptr_t ptr_addr = reinterpret_cast<uintptr_t>(ptr);
		array_push_back(sched->free_stacks, ptr_addr);

		mutex_unlock(&sched->stack_mutex);
	}

	extern "C" void job_main(void) {
		Job* job = sched_current_job();

		if (job && job->func) {
			job->state = CoroState::Running;
			job->func(job->user_data);
			job->state = CoroState::Finished;
		}

		if (job && job->caller) {
			fiber_context_switch(job->context, job->caller->context);
		}

		assert(0 && "Job returned without caller");
	}

	static Job* job_create(Scheduler* sched, CoroFn func, void* payload) {
		if (!sched || !func) {
			return nullptr;
		}

		Job* job = allocate_zeroed<Job>(sched->allocator, 1);
		if (!job) {
			return nullptr;
		}

		void* stack_ptr = sched_alloc_stack_ptr(sched);
		if (!stack_ptr) {
			goto failed;
		}

		job->context = fiber_context_create(sched->allocator, job_main, stack_ptr, EDGE_FIBER_STACK_SIZE);
		if (!job->context) {
			goto failed;
		}

		job->state = CoroState::Suspended;
		job->caller = nullptr;
		job->func = func;
		job->user_data = payload;

		job->priority = SchedulerPriority::Low;

		return job;

	failed:
		if (job) {
			if (job->context) {
				void* stack_ptr = fiber_get_stack_ptr(job->context);
				if (stack_ptr) {
					sched_free_stack_ptr(sched, stack_ptr);
				}

				fiber_context_destroy(sched->allocator, job->context);
			}

			deallocate(sched->allocator, job);
		}

		return nullptr;
	}

	static void job_destroy(Scheduler* sched, Job* job) {
		if (!sched || !job) {
			return;
		}

		if (job->context) {
			void* stack_ptr = fiber_get_stack_ptr(job->context);
			if (stack_ptr) {
				sched_free_stack_ptr(sched, stack_ptr);
			}

			fiber_context_destroy(sched->allocator, job->context);
		}

		deallocate(sched->allocator, job);
	}

	static bool job_state_update(Job* job, CoroState expected_state, CoroState new_state) {
		if (job->state == expected_state) {
			job->state = new_state;
			return true;
		}
		return false;
	}

	static Job* sched_pick_job(Scheduler* sched) {
		if (!sched) {
			return nullptr;
		}

		for (i32 i = static_cast<i32>(SchedulerPriority::Count) - 1; i >= 0; i--) {
			uintptr_t job_addr;
			if (mpmc_queue_dequeue(sched->queues[i], &job_addr)) {
				sched->queued_jobs.fetch_sub(1, std::memory_order_relaxed);
				return reinterpret_cast<Job*>(job_addr);
			}
		}

		return nullptr;
	}

	static void sched_enqueue_job(Scheduler* sched, Job* job) {
		if (!sched || !job) {
			return;
		}

		i32 priority_index = static_cast<i32>(job->priority);
		uintptr_t job_addr = reinterpret_cast<uintptr_t>(job);

		if (!mpmc_queue_enqueue(sched->queues[priority_index], job_addr)) {
			return;
		}

		sched->queued_jobs.fetch_add(1, std::memory_order_relaxed);
	}

	static i32 sched_worker_thread(void* arg) {
		WorkerThread* worker = static_cast<WorkerThread*>(arg);
		Scheduler* sched = worker->scheduler;

		sched_init_thread_context(worker);

		while (!worker->should_exit.load(std::memory_order_acquire)) {
			Job* job = sched_pick_job(sched);
			if (!job) {
				if (sched->shutdown.load(std::memory_order_acquire)) {
					break;
				}

				if (sched->queued_jobs.load(std::memory_order_acquire) == 0) {
					thread_yield();
				}
				continue;
			}

			bool claimed = job_state_update(job, CoroState::Suspended, CoroState::Running);
			if (!claimed) {
				sched->jobs_failed.fetch_add(1, std::memory_order_relaxed);
				continue;
			}

			Job* caller = sched_current_job();
			job->caller = caller;

			caller->state = CoroState::Suspended;

			Job* volatile target_job = job;
			thread_context.current_job = target_job;

			fiber_context_switch(caller->context, job->context);

			thread_context.current_job = caller;
			caller->state = CoroState::Running;

			CoroState job_state = job->state;
			if (job_state == CoroState::Finished) {
				job_destroy(sched, job);

				sched->active_jobs.fetch_sub(1, std::memory_order_acq_rel);
				sched->jobs_completed.fetch_add(1, std::memory_order_relaxed);
			}
			else if (job_state == CoroState::Suspended) {
				sched_enqueue_job(sched, job);
			}
		}

		sched_shutdown_thread_context();

		return 0;
	}

	Scheduler* sched_create(const Allocator* allocator) {
		if (!allocator) {
			return nullptr;
		}

		Scheduler* sched = allocate_zeroed<Scheduler>(allocator, 1);
		if (!sched) {
			return nullptr;
		}

		sched->allocator = allocator;

		sched->protected_arena = allocate<Arena>(allocator);
		if (!arena_create(allocator, sched->protected_arena, 0)) {
			goto failed;
		}

		sched->free_stacks = allocate<Array<uintptr_t>>(allocator);
		if (!array_create(allocator, sched->free_stacks, 16)) {
			goto failed;
		}

		if (mutex_init(&sched->stack_mutex, MutexType::Plain) != ThreadResult::Success) {
			goto failed;
		}

		for (i32 i = 0; i < static_cast<i32>(SchedulerPriority::Count); ++i) {
			sched->queues[i] = allocate<MPMCQueue<uintptr_t>>(allocator);
			if (!mpmc_queue_create(allocator, sched->queues[i], 1024)) {
				goto failed;
			}
		}

		CpuInfo cpu_info[256];
		i32 cpu_count = thread_get_cpu_topology(cpu_info, 256);

		i32 num_cores = thread_get_logical_core_count(cpu_info, cpu_count);
		if (num_cores <= 0) {
			num_cores = 4;
		}

		sched->worker_threads = allocate<Array<WorkerThread*>>(allocator);
		if (!array_create(allocator, sched->worker_threads, num_cores)) {
			goto failed;
		}

		sched->shutdown.store(false, std::memory_order_relaxed);
		sched->active_jobs.store(0, std::memory_order_relaxed);
		sched->queued_jobs.store(0, std::memory_order_relaxed);
		sched->jobs_completed.store(0, std::memory_order_relaxed);
		sched->jobs_failed.store(0, std::memory_order_relaxed);

		for (i32 i = 0; i < num_cores; ++i) {
			WorkerThread* worker = allocate<WorkerThread>(allocator);
			if (!worker) {
				goto failed;
			}

			worker->scheduler = sched;
			worker->thread_id = i;
			worker->should_exit.store(false, std::memory_order_relaxed);

			if (thread_create(&worker->thread, sched_worker_thread, worker) != ThreadResult::Success) {
				deallocate(allocator, worker);
				goto failed;
			}

			if (!array_push_back(sched->worker_threads, worker)) {
				thread_join(worker->thread, nullptr);
				deallocate(allocator, worker);
				goto failed;
			}

			thread_set_affinity_ex(worker->thread, cpu_info, cpu_count, i, false);

			char buffer[32] = { 0 };
			snprintf(buffer, sizeof(buffer), "worker-%d", i);
			thread_set_name(worker->thread, buffer);
		}

		return sched;

	failed:
		if (sched) {
			if (sched->worker_threads) {
				for (usize i = 0; i < array_size(sched->worker_threads); ++i) {
					WorkerThread** worker_ptr = array_at(sched->worker_threads, i);
					if (worker_ptr && *worker_ptr) {
						(*worker_ptr)->should_exit.store(true, std::memory_order_release);
					}
				}

				sched->shutdown.store(true, std::memory_order_release);

				for (usize i = 0; i < array_size(sched->worker_threads); ++i) {
					WorkerThread** worker_ptr = array_at(sched->worker_threads, i);
					if (worker_ptr && *worker_ptr) {
						thread_join((*worker_ptr)->thread, nullptr);
						deallocate(allocator, *worker_ptr);
					}
				}

				array_destroy(sched->worker_threads);
			}

			for (i32 i = 0; i < static_cast<i32>(SchedulerPriority::Count); ++i) {
				MPMCQueue<uintptr_t>* queue = sched->queues[i];
				if (queue) {
					mpmc_queue_destroy(queue);
					deallocate(allocator, queue);
				}
			}

			mutex_destroy(&sched->stack_mutex);

			if (sched->free_stacks) {
				array_destroy(sched->free_stacks);
				deallocate(allocator, sched->free_stacks);
			}

			if (sched->protected_arena) {
				arena_destroy(sched->protected_arena);
				deallocate(allocator, sched->protected_arena);
			}

			deallocate(allocator, sched);
		}

		return nullptr;
	}

	void sched_destroy(Scheduler* sched) {
		if (!sched) {
			return;
		}

		sched->shutdown.store(true, std::memory_order_release);

		if (sched->worker_threads) {
			for (usize i = 0; i < array_size(sched->worker_threads); ++i) {
				WorkerThread** worker_ptr = array_at(sched->worker_threads, i);
				if (worker_ptr && *worker_ptr) {
					(*worker_ptr)->should_exit.store(true, std::memory_order_release);
					thread_join((*worker_ptr)->thread, nullptr);
					deallocate(sched->allocator, *worker_ptr);
				}
			}

			array_destroy(sched->worker_threads);
			deallocate(sched->allocator, sched->worker_threads);
		}

		for (i32 i = 0; i < static_cast<i32>(SchedulerPriority::Count); ++i) {
			MPMCQueue<uintptr_t>* queue = sched->queues[i];
			if (queue) {
				while (true) {
					uintptr_t job_addr;
					if (!mpmc_queue_dequeue(queue, &job_addr)) {
						break;
					}

					Job* job = reinterpret_cast<Job*>(job_addr);
					job_destroy(sched, job);
				}

				mpmc_queue_destroy(queue);
				deallocate(sched->allocator, queue);
			}
		}

		mutex_destroy(&sched->stack_mutex);

		if (sched->free_stacks) {
			array_destroy(sched->free_stacks);
			deallocate(sched->allocator, sched->free_stacks);
		}

		if (sched->protected_arena) {
			arena_destroy(sched->protected_arena);
			deallocate(sched->allocator, sched->protected_arena);
		}

		const Allocator* alloc = sched->allocator;
		deallocate(alloc, sched);
	}

	void sched_schedule_job(Scheduler* sched, CoroFn func, void* payload, SchedulerPriority priority) {
		if (!sched || !func) {
			return;
		}

		Job* job = job_create(sched, func, payload);
		if (!job) {
			return;
		}

		job->priority = priority;

		sched->active_jobs.fetch_add(1, std::memory_order_relaxed);
		sched_enqueue_job(sched, job);
	}

	Job* sched_current_job(void) {
		return thread_context.current_job;
	}

	i32 sched_current_thread_id(void) {
		return thread_context.thread_id;
	}

	Scheduler* sched_current_instance(void) {
		return thread_context.scheduler;
	}

	void sched_run(Scheduler* sched) {
		if (!sched) {
			return;
		}

		while (
			sched->active_jobs.load(std::memory_order_acquire) > 0 &&
			!sched->shutdown.load(std::memory_order_acquire)) {
			thread_yield();
		}
	}

	void sched_yield(void) {
		Job* job = thread_context.current_job;

		CoroState job_state = job->state;
		if (!job || job == &thread_context.main_job || job_state == CoroState::Finished) {
			return;
		}

		Job* caller = job->caller;
		if (!caller) {
			return;
		}

		job->state = CoroState::Suspended;
		caller->state = CoroState::Running;

		fiber_context_switch(job->context, caller->context);

		job->state = CoroState::Running;
	}

	void sched_await(CoroFn func, void* payload, SchedulerPriority priority) {
		if (!func) {
			return;
		}
	}
}