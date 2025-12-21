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
		FiberContext* context = nullptr;
		JobFn func = {};
		JobState state = {};
		Job* caller = nullptr;

		SchedulerPriority priority = {};
	};

	struct WorkerThread {
		Thread thread = {};
		Scheduler* scheduler = nullptr;
		usize thread_id = 0;
		std::atomic<bool> should_exit = false;
	};

	struct Scheduler {
		const Allocator* allocator = nullptr;

		Arena protected_arena = {};
		Array<uintptr_t> free_stacks = {};
		Mutex stack_mutex = {};

		MPMCQueue<uintptr_t> queues[static_cast<usize>(SchedulerPriority::Count)] = {};

		Array<WorkerThread*> worker_threads = {};

		std::atomic<usize> active_jobs = 0;
		std::atomic<usize> queued_jobs = 0;
		std::atomic<bool> shutdown = false;

		std::atomic<usize> jobs_completed = 0;
		std::atomic<usize> jobs_failed = 0;
	};

	struct SchedulerThreadContext {
		Scheduler* scheduler = nullptr;

		Job* current_job = nullptr;
		Job main_job = {};
		FiberContext* main_context = nullptr;

		i32 thread_id = 0;
	};

	struct SchedulerEvent {
		std::atomic<bool> done = false;
	};

	static thread_local SchedulerThreadContext thread_context = { };

	static void sched_init_thread_context(WorkerThread* worker) {
		thread_context.scheduler = worker->scheduler;
		thread_context.main_context = fiber_context_create(worker->scheduler->allocator, nullptr, nullptr, 0);
		thread_context.main_job.context = thread_context.main_context;
		thread_context.current_job = &thread_context.main_job;
		thread_context.thread_id = static_cast<i32>(worker->thread_id);

		thread_context.main_job.state = JobState::Running;
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

		SchedulerEvent* event = sched->allocator->allocate<SchedulerEvent>();
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

		thread_context.scheduler->allocator->deallocate(event);
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
		if (!sched->free_stacks.pop_back(&ptr_addr)) {
			mutex_unlock(&sched->stack_mutex);
			return sched->protected_arena.alloc_ex(EDGE_FIBER_STACK_SIZE, EDGE_FIBER_STACK_ALIGN);
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
		sched->free_stacks.push_back(sched->allocator, ptr_addr);

		mutex_unlock(&sched->stack_mutex);
	}

	extern "C" void job_main(void) {
		Job* job = sched_current_job();

		if (job && job->func.is_valid()) {
			job->state = JobState::Running;
			job->func.invoke();
			job->state = JobState::Finished;
		}

		if (job && job->caller) {
			fiber_context_switch(job->context, job->caller->context);
		}

		assert(0 && "Job returned without caller");
	}

	static Job* job_create(Scheduler* sched, JobFn func) {
		if (!sched || !func.is_valid()) {
			return nullptr;
		}

		Job* job = sched->allocator->allocate<Job>();
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

		job->state = JobState::Suspended;
		job->caller = nullptr;
		job->func = func;

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

			sched->allocator->deallocate(job);
		}

		return nullptr;
	}

	static void job_destroy(Scheduler* sched, Job* job) {
		if (!sched || !job) {
			return;
		}

		if (job->func.is_valid()) {
			job->func.destroy(sched->allocator);
		}

		if (job->context) {
			void* stack_ptr = fiber_get_stack_ptr(job->context);
			if (stack_ptr) {
				sched_free_stack_ptr(sched, stack_ptr);
			}

			fiber_context_destroy(sched->allocator, job->context);
		}

		sched->allocator->deallocate(job);
	}

	static bool job_state_update(Job* job, JobState expected_state, JobState new_state) {
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
			if (sched->queues[i].dequeue(&job_addr)) {
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

		if (!sched->queues[priority_index].enqueue(job_addr)) {
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

			bool claimed = job_state_update(job, JobState::Suspended, JobState::Running);
			if (!claimed) {
				sched->jobs_failed.fetch_add(1, std::memory_order_relaxed);
				continue;
			}

			Job* caller = sched_current_job();
			job->caller = caller;

			caller->state = JobState::Suspended;

			Job* volatile target_job = job;
			thread_context.current_job = target_job;

			fiber_context_switch(caller->context, job->context);

			thread_context.current_job = caller;
			caller->state = JobState::Running;

			JobState job_state = job->state;
			if (job_state == JobState::Finished) {
				job_destroy(sched, job);

				sched->active_jobs.fetch_sub(1, std::memory_order_acq_rel);
				sched->jobs_completed.fetch_add(1, std::memory_order_relaxed);
			}
			else if (job_state == JobState::Suspended) {
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

		Scheduler* sched = allocator->allocate<Scheduler>();
		if (!sched) {
			return nullptr;
		}

		sched->allocator = allocator;

		if (!sched->protected_arena.create()) {
			goto failed;
		}

		if (!sched->free_stacks.reserve(allocator, 16)) {
			goto failed;
		}

		if (mutex_init(&sched->stack_mutex, MutexType::Plain) != ThreadResult::Success) {
			goto failed;
		}

		for (i32 i = 0; i < static_cast<i32>(SchedulerPriority::Count); ++i) {
			if (!sched->queues[i].create(allocator, 1024)) {
				goto failed;
			}
		}

		CpuInfo cpu_info[256];
		i32 cpu_count = thread_get_cpu_topology(cpu_info, 256);

		i32 num_cores = thread_get_logical_core_count(cpu_info, cpu_count);
		if (num_cores <= 0) {
			num_cores = 4;
		}

		if (!sched->worker_threads.reserve(allocator, num_cores)) {
			goto failed;
		}

		sched->shutdown.store(false, std::memory_order_relaxed);
		sched->active_jobs.store(0, std::memory_order_relaxed);
		sched->queued_jobs.store(0, std::memory_order_relaxed);
		sched->jobs_completed.store(0, std::memory_order_relaxed);
		sched->jobs_failed.store(0, std::memory_order_relaxed);

		for (i32 i = 0; i < num_cores; ++i) {
			WorkerThread* worker = allocator->allocate<WorkerThread>();
			if (!worker) {
				goto failed;
			}

			worker->scheduler = sched;
			worker->thread_id = i;
			worker->should_exit.store(false, std::memory_order_relaxed);

			if (thread_create(&worker->thread, sched_worker_thread, worker) != ThreadResult::Success) {
				allocator->deallocate(worker);
				goto failed;
			}

			if (!sched->worker_threads.push_back(sched->allocator, worker)) {
				thread_join(worker->thread, nullptr);
				allocator->deallocate(worker);
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
			if (sched->worker_threads.m_data) {
				for (usize i = 0; i < sched->worker_threads.m_size; ++i) {
					WorkerThread** worker_ptr = sched->worker_threads.get(i);
					if (worker_ptr && *worker_ptr) {
						(*worker_ptr)->should_exit.store(true, std::memory_order_release);
					}
				}

				sched->shutdown.store(true, std::memory_order_release);

				for (usize i = 0; i < sched->worker_threads.m_size; ++i) {
					WorkerThread** worker_ptr = sched->worker_threads.get(i);
					if (worker_ptr && *worker_ptr) {
						thread_join((*worker_ptr)->thread, nullptr);
						allocator->deallocate(*worker_ptr);
					}
				}

				sched->worker_threads.destroy(sched->allocator);
			}

			for (i32 i = 0; i < static_cast<i32>(SchedulerPriority::Count); ++i) {
				sched->queues[i].destroy(allocator);
			}

			mutex_destroy(&sched->stack_mutex);

			if (sched->free_stacks.m_data) {
				sched->free_stacks.destroy(sched->allocator);
			}

			sched->protected_arena.destroy();
			allocator->deallocate(sched);
		}

		return nullptr;
	}

	void sched_destroy(Scheduler* sched) {
		if (!sched) {
			return;
		}

		sched->shutdown.store(true, std::memory_order_release);

		if (sched->worker_threads.m_data) {
			for (usize i = 0; i < sched->worker_threads.m_size; ++i) {
				WorkerThread** worker_ptr = sched->worker_threads.get(i);
				if (worker_ptr && *worker_ptr) {
					(*worker_ptr)->should_exit.store(true, std::memory_order_release);
					thread_join((*worker_ptr)->thread, nullptr);
					sched->allocator->deallocate(*worker_ptr);
				}
			}

			sched->worker_threads.destroy(sched->allocator);
		}

		for (i32 i = 0; i < static_cast<i32>(SchedulerPriority::Count); ++i) {
			while (true) {
				uintptr_t job_addr;
				if (!sched->queues[i].dequeue(&job_addr)) {
					break;
				}

				Job* job = reinterpret_cast<Job*>(job_addr);
				job_destroy(sched, job);
			}

			sched->queues[i].destroy(sched->allocator);
		}

		mutex_destroy(&sched->stack_mutex);

		if (sched->free_stacks.m_data) {
			sched->free_stacks.destroy(sched->allocator);
		}

		sched->protected_arena.destroy();

		const Allocator* alloc = sched->allocator;
		alloc->deallocate(sched);
	}

	void sched_schedule_job(Scheduler* sched, JobFn func, SchedulerPriority priority) {
		if (!sched || !func.is_valid()) {
			return;
		}

		Job* job = job_create(sched, func);
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

	const Allocator* sched_get_allocator(Scheduler* sched) {
		return sched->allocator;
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

		JobState job_state = job->state;
		if (!job || job == &thread_context.main_job || job_state == JobState::Finished) {
			return;
		}

		Job* caller = job->caller;
		if (!caller) {
			return;
		}

		job->state = JobState::Suspended;
		caller->state = JobState::Running;

		fiber_context_switch(job->context, caller->context);

		job->state = JobState::Running;
	}
}