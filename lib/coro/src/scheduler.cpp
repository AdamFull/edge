#include "scheduler.hpp"

#include <cstdio>
#include <cassert>
#include <ctime>

namespace edge {
	struct WorkerThread {
		const Allocator* allocator = nullptr;

		Thread thread = {};
		Scheduler* scheduler = nullptr;
		usize thread_id = 0;
		std::atomic<bool> should_exit = false;

		static WorkerThread* create(NotNull<const Allocator*> alloc) noexcept;
		static void destroy(NotNull<const Allocator*> alloc, WorkerThread* self) noexcept;

		static i32 worker_entry(void* arg) noexcept {
			WorkerThread* worker = static_cast<WorkerThread*>(arg);
			return worker->loop();
		}

	private:
		i32 loop() noexcept;
	};

	struct SchedulerThreadContext {
		WorkerThread* worker = nullptr;

		Arena protected_arena = {};
		Array<void*> free_stacks = {};

		Job* current_job = nullptr;
		Job main_job = {};
		FiberContext* main_context = nullptr;

		bool create(WorkerThread* worker) noexcept;
		void shutdown() noexcept;

		void* alloc_stack_ptr() noexcept;
		void free_stack_ptr(NotNull<const Allocator*> alloc, void* ptr) noexcept;

		void switch_to_job(Job* target) noexcept;
	};

	static thread_local SchedulerThreadContext thread_context = { };

	WorkerThread* WorkerThread::create(NotNull<const Allocator*> alloc) noexcept {
		WorkerThread* worker = alloc->allocate<WorkerThread>();
		if (!worker) {
			return nullptr;
		}

		worker->allocator = alloc.m_ptr;
		worker->should_exit.store(false, std::memory_order_relaxed);

		if (thread_create(&worker->thread, WorkerThread::worker_entry, worker) != ThreadResult::Success) {
			alloc->deallocate(worker);
			return nullptr;
		}

		return worker;
	}

	void WorkerThread::destroy(NotNull<const Allocator*> alloc, WorkerThread* self) noexcept {
		self->should_exit.store(true, std::memory_order_release);
		thread_join(self->thread, nullptr);
		alloc->deallocate(self);
	}

	i32 WorkerThread::loop() noexcept {
		if (!thread_context.create(this)) {
			// TODO: LOG ERROR
			return -1;
		}

		while (!should_exit.load(std::memory_order_acquire)) {
			Job* job = scheduler->pick_job();
			if (!job) {
				if (scheduler->shutdown.load(std::memory_order_acquire)) {
					break;
				}

				scheduler->sleeping_workers.fetch_add(1, std::memory_order_relaxed);

				u32 futex_val = scheduler->worker_futex.load(std::memory_order_acquire);
				futex_wait(&scheduler->worker_futex, futex_val, std::chrono::nanoseconds::max());

				scheduler->sleeping_workers.fetch_sub(1, std::memory_order_relaxed);
				continue;
			}

			if (!job->update_state(JobState::Suspended, JobState::Running)) {
				continue;
			}

			Job* caller = thread_context.current_job;
			job->caller = caller;

			caller->state.store(JobState::Suspended, std::memory_order_release);
			thread_context.current_job = job;

			fiber_context_switch(caller->context, job->context);

			// NOTE: I think that's a bad solution. 
			// While we were doing the work, we could call await, 
			// which switched us to the sub-job context.
			job = thread_context.current_job;
			thread_context.current_job = caller;
			caller->state.store(JobState::Running, std::memory_order_release);
			
			JobState job_state = job->state.load(std::memory_order_acquire);
			if (job_state == JobState::Suspended) {
				scheduler->enqueue_job(job);
			}
			else {
				scheduler->complete_job(allocator, job, job_state);
			}
		}

		thread_context.shutdown();

		return 0;
	}

	bool SchedulerThreadContext::create(WorkerThread* worker) noexcept {
		this->worker = worker;
		main_context = fiber_context_create(worker->allocator, nullptr, nullptr, 0);
		main_job.context = main_context;
		current_job = &main_job;

		main_job.state.store(JobState::Running, std::memory_order_release);

		if (!protected_arena.create()) {
			return false;
		}

		if (!free_stacks.reserve(worker->allocator, 16)) {
			return false;
		}

		return true;
	}

	void SchedulerThreadContext::shutdown() noexcept {
		if (main_context) {
			fiber_context_destroy(worker->allocator, main_context);
		}

		free_stacks.destroy(worker->allocator);
		protected_arena.destroy();
	}

	void* SchedulerThreadContext::alloc_stack_ptr() noexcept {
		void* stack = nullptr;
		if (!free_stacks.pop_back(&stack)) {
			return protected_arena.alloc_ex(EDGE_FIBER_STACK_SIZE, EDGE_FIBER_STACK_ALIGN);
		}
		return stack;
	}

	void SchedulerThreadContext::free_stack_ptr(NotNull<const Allocator*> alloc, void* ptr) noexcept {
		free_stacks.push_back(alloc, ptr);
	}

	void SchedulerThreadContext::switch_to_job(Job* target) noexcept {
		Job* previous = current_job;
		current_job = target;

		target->caller = previous;
		fiber_context_switch(previous->context, target->context);
		current_job = previous;
	}

	extern "C" void job_main(void) {
		Job* job = thread_context.current_job;

		if (!job || !job->func.is_valid()) {
			assert(false && "Invalid job in fiber_main");
			return;
		}

		job->state.store(JobState::Running, std::memory_order_release);
		job->func.invoke();
		job->state.store(JobState::Completed, std::memory_order_release);

		if (job->promise) {
			auto* promise = static_cast<JobPromise<void, void>*>(job->promise);
			promise->status.store(JobState::Completed, std::memory_order_release);
		}

		if (job->caller) {
			fiber_context_switch(job->context, job->caller->context);
		}

		assert(0 && "Job returned without caller");
	}

	Job* Job::create(NotNull<const Allocator*> alloc, JobFn&& func, SchedulerPriority prio) noexcept {
		if (!func.is_valid()) {
			return nullptr;
		}

		Job* job = alloc->allocate<Job>();
		if (!job) {
			return nullptr;
		}

		void* stack_ptr = thread_context.alloc_stack_ptr();
		if (!stack_ptr) {
			return nullptr;
		}

		job->context = fiber_context_create(alloc, job_main, stack_ptr, EDGE_FIBER_STACK_SIZE);
		if (!job->context) {
			thread_context.free_stack_ptr(alloc, stack_ptr);
			return nullptr;
		}

		job->state.store(JobState::Suspended, std::memory_order_release);
		job->caller = nullptr;
		job->func = std::move(func);
		job->priority = prio;

		return job;
	}

	void Job::destroy(NotNull<const Allocator*> alloc, Job* self) noexcept {
		if (self->func.is_valid()) {
			self->func.destroy(alloc);
		}

		if (self->context) {
			void* stack_ptr = fiber_get_stack_ptr(self->context);
			if (stack_ptr) {
				thread_context.free_stack_ptr(alloc, stack_ptr);
			}

			fiber_context_destroy(alloc, self->context);
		}

		alloc->deallocate(self);
	}

	bool Job::update_state(JobState expected_state, JobState new_state) noexcept {
		return state.compare_exchange_strong(expected_state, new_state, std::memory_order_acquire, std::memory_order_relaxed);
	}

	Scheduler* Scheduler::create(NotNull<const Allocator*> alloc) noexcept {
		Scheduler* sched = alloc->allocate<Scheduler>();
		if (!sched) {
			return nullptr;
		}

		for (i32 i = 0; i < static_cast<i32>(SchedulerPriority::Count); ++i) {
			if (!sched->queues[i].create(alloc, 1024)) {
				Scheduler::destroy(alloc, sched);
				return nullptr;
			}
		}

		CpuInfo cpu_info[256];
		i32 cpu_count = thread_get_cpu_topology(cpu_info, 256);

		i32 num_cores = thread_get_logical_core_count(cpu_info, cpu_count);
		if (num_cores <= 0) {
			num_cores = 4;
		}

		// TODO: Use fixed runtime array
		if (!sched->worker_threads.reserve(alloc, num_cores)) {
			Scheduler::destroy(alloc, sched);
			return nullptr;
		}

		sched->shutdown.store(false, std::memory_order_relaxed);
		sched->active_jobs.store(0, std::memory_order_relaxed);
		sched->queued_jobs.store(0, std::memory_order_relaxed);
		sched->jobs_completed.store(0, std::memory_order_relaxed);
		sched->jobs_failed.store(0, std::memory_order_relaxed);
		sched->worker_futex.store(0, std::memory_order_relaxed);
		sched->sleeping_workers.store(0, std::memory_order_relaxed);

		for (i32 i = 0; i < num_cores; ++i) {
			WorkerThread* worker = WorkerThread::create(alloc);
			if (!worker) {
				Scheduler::destroy(alloc, sched);
				return nullptr;
			}

			worker->scheduler = sched;
			worker->thread_id = i;

			if (!sched->worker_threads.push_back(alloc, worker)) {
				Scheduler::destroy(alloc, sched);
				return nullptr;
			}

			thread_set_affinity_ex(worker->thread, cpu_info, cpu_count, i, false);

			char buffer[32] = { 0 };
			snprintf(buffer, sizeof(buffer), "worker-%d", i);
			thread_set_name(worker->thread, buffer);
		}

		sched->main_thread = alloc->allocate<WorkerThread>();
		if (!sched->main_thread) {
			Scheduler::destroy(alloc, sched);
			return nullptr;
		}

		sched->main_thread->allocator = alloc.m_ptr;
		sched->main_thread->thread = thread_current();
		sched->main_thread->thread_id = thread_current_id();
		sched->main_thread->scheduler = sched;

		// NOTE: Init main therad context
		if (!thread_context.create(sched->main_thread)) {
			Scheduler::destroy(alloc, sched);
			return nullptr;
		}

		return sched;
	}

	void Scheduler::destroy(NotNull<const Allocator*> alloc, Scheduler* self) noexcept {
		assert(self->main_thread->thread_id == thread_context.worker->thread_id && "Destroy can be called only from main thread.");

		thread_context.shutdown();

		if (self->main_thread) {
			alloc->deallocate(self->main_thread);
		}

		self->shutdown.store(true, std::memory_order_release);

		self->worker_futex.fetch_add(1, std::memory_order_release);
		futex_wake_all(&self->worker_futex);

		if (!self->worker_threads.empty()) {
			for (WorkerThread* worker_thread : self->worker_threads) {
				if (worker_thread) {
					WorkerThread::destroy(alloc, worker_thread);
				}
			}

			self->worker_threads.destroy(alloc);
		}

		for (i32 i = 0; i < static_cast<i32>(SchedulerPriority::Count); ++i) {
			while (true) {
				Job* job;
				if (!self->queues[i].dequeue(&job)) {
					break;
				}
				Job::destroy(alloc, job);
			}

			self->queues[i].destroy(alloc);
		}

		alloc->deallocate(self);
	}

	void Scheduler::schedule(Job* job) noexcept {
		active_jobs.fetch_add(1, std::memory_order_relaxed);
		enqueue_job(job);
	}

	void Scheduler::run() noexcept {
		assert(main_thread->thread_id == thread_context.worker->thread_id && "Run can be called only from main thread.");

		while (
			active_jobs.load(std::memory_order_acquire) > 0 &&
			!shutdown.load(std::memory_order_acquire)) {
			thread_yield();
		}
	}

	Job* Scheduler::pick_job() noexcept {
		for (i32 i = static_cast<i32>(SchedulerPriority::Count) - 1; i >= 0; i--) {
			Job* job;
			if (queues[i].dequeue(&job)) {
				queued_jobs.fetch_sub(1, std::memory_order_relaxed);
				return job;
			}
		}

		return nullptr;
	}

	void Scheduler::enqueue_job(Job* job) noexcept {
		i32 priority_index = static_cast<i32>(job->priority);
		if (!queues[priority_index].enqueue(job)) {
			return;
		}

		if (sleeping_workers.load(std::memory_order_acquire) > 0) {
			worker_futex.fetch_add(1, std::memory_order_release);
			futex_wake_all(&worker_futex);
		}

		queued_jobs.fetch_add(1, std::memory_order_relaxed);
	}

	void Scheduler::complete_job(NotNull<const Allocator*> alloc, Job* job, JobState final_status) noexcept {
		if (final_status == JobState::Completed) {
			jobs_completed.fetch_add(1, std::memory_order_relaxed);
		}
		else if (final_status == JobState::Failed) {
			jobs_failed.fetch_add(1, std::memory_order_relaxed);
		}

		Job* continuation = job->continuation;
		if (continuation) {
			job->continuation = nullptr;
			enqueue_job(continuation);
		}
		else {
			active_jobs.fetch_sub(1, std::memory_order_release);
		}

		Job::destroy(alloc, job);
	}

	Scheduler* sched_current() noexcept {
		return thread_context.worker->scheduler;
	}

	Job* job_current() noexcept {
		return thread_context.current_job;
	}

	i32 job_thread_id() noexcept {
		return thread_context.worker->thread_id;
	}

	void job_yield() noexcept {
		Job* job = thread_context.current_job;
		if (!job || job == &thread_context.main_job) {
			return;
		}

		job->state.store(JobState::Suspended, std::memory_order_release);
		fiber_context_switch(job->context, job->caller->context);
		job->state.store(JobState::Running, std::memory_order_release);
	}

	void job_await(Job* child_job, void* promise) noexcept {
		Job* parent_job = thread_context.current_job;
		thread_context.current_job = child_job;

		child_job->caller = parent_job->caller;
		child_job->continuation = parent_job;
		child_job->promise = promise;

		parent_job->state.store(JobState::Suspended, std::memory_order_release);
		fiber_context_switch(parent_job->context, child_job->context);

		// TODO: There is a problem. If this child process is terminated, it will never be deleted. 
		// When it is suspended, it should return to the worker's thread, but the worker will think that the 
		// main process is suspended, not the child process. Something needs to be done about this as well.
	}
}