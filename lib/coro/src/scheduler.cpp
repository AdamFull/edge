#include "scheduler.hpp"

#include <cstdio>
#include <cassert>
#include <ctime>

namespace edge {
	struct Scheduler::Worker {
		const Allocator* allocator = nullptr;

		Thread thread_handle = {};
		Scheduler* scheduler = nullptr;
		usize thread_id = 0;
		std::atomic<bool> should_exit = false;

		static Worker* create(NotNull<const Allocator*> alloc) noexcept;
		static void destroy(NotNull<const Allocator*> alloc, Worker* self) noexcept;

		static i32 entry(void* arg) noexcept {
			Worker* worker = static_cast<Worker*>(arg);
			return worker->loop();
		}

	private:
		i32 loop() noexcept;
	};

	struct SchedulerThreadContext {
		Scheduler::Worker* worker = nullptr;

		Job* current_job = nullptr;
		Job main_job = {};
		FiberContext* main_context = nullptr;

		bool create(Scheduler::Worker* worker) noexcept;
		void shutdown() noexcept;
	};

	static thread_local SchedulerThreadContext thread_context = { };

	Scheduler::Worker* Scheduler::Worker::create(NotNull<const Allocator*> alloc) noexcept {
		Scheduler::Worker* worker = alloc->allocate<Scheduler::Worker>();
		if (!worker) {
			return nullptr;
		}

		worker->allocator = alloc.m_ptr;
		worker->should_exit.store(false, std::memory_order_relaxed);

		if (thread_create(&worker->thread_handle, Scheduler::Worker::entry, worker) != ThreadResult::Success) {
			alloc->deallocate(worker);
			return nullptr;
		}

		return worker;
	}

	void Scheduler::Worker::destroy(NotNull<const Allocator*> alloc, Scheduler::Worker* self) noexcept {
		self->should_exit.store(true, std::memory_order_release);
		thread_join(self->thread_handle, nullptr);
		alloc->deallocate(self);
	}

	i32 Scheduler::Worker::loop() noexcept {
		if (!thread_context.create(this)) {
			// TODO: LOG ERROR
			return -1;
		}

		while (!should_exit.load(std::memory_order_acquire)) {
			auto [job, priority] = scheduler->pick_job();
			if (!job) {
				if (scheduler->shutdown.load(std::memory_order_acquire)) {
					break;
				}

				scheduler->sleeping_workers.fetch_add(1, std::memory_order_relaxed);
				std::atomic_thread_fence(std::memory_order_seq_cst);
				u32 futex_val = scheduler->worker_futex.load(std::memory_order_acquire);
				futex_wait(&scheduler->worker_futex, futex_val, std::chrono::nanoseconds::max());
				scheduler->sleeping_workers.fetch_sub(1, std::memory_order_relaxed);
				continue;
			}

			Job::State expected = Job::State::Suspended;
			if (!job->state.compare_exchange_strong(expected, Job::State::Running, std::memory_order_acquire, std::memory_order_relaxed)) {
				// TODO: I think there is a bug here, and I might lose the job at this point.
				continue;
			}

			Job* caller = thread_context.current_job;
			job->caller = caller;

			caller->state.store(Job::State::Suspended, std::memory_order_release);
			thread_context.current_job = job;

			// Switch to job context
			fiber_context_switch(caller->context, job->context);

			// NOTE: I think this is a bad solution.
			// While we were doing the work, we could call await,
			// which would switch us to a sub-job context.
			job = thread_context.current_job;
			thread_context.current_job = caller;
			caller->state.store(Job::State::Running, std::memory_order_release);
			
			// NOTE: I'm not sure whether i can receive Job::State::Running here.
			Job::State job_state = job->state.load(std::memory_order_acquire);
			if (job_state == Job::State::Suspended) {
				scheduler->schedule(job, priority);
			}
			else {
				scheduler->complete_job(allocator, job, priority, job_state);
			}
		}

		thread_context.shutdown();

		return 0;
	}

	bool SchedulerThreadContext::create(Scheduler::Worker* worker) noexcept {
		this->worker = worker;
		main_context = fiber_context_create(worker->allocator, nullptr, nullptr, 0);
		main_job.context = main_context;
		current_job = &main_job;

		main_job.state.store(Job::State::Running, std::memory_order_release);

		return true;
	}

	void SchedulerThreadContext::shutdown() noexcept {
		if (main_context) {
			fiber_context_destroy(worker->allocator, main_context);
		}
	}

	extern "C" void job_main(void) {
		Job* job = thread_context.current_job;

		if (!job || !job->func.is_valid()) {
			assert(false && "Invalid job in fiber_main");
			return;
		}

		// NOTE: Do i need to check here that job was in suspended state?
		job->state.store(Job::State::Running, std::memory_order_release);
		job->func.invoke();
		job->state.store(Job::State::Completed, std::memory_order_release);

		if (job->promise) {
			auto* promise = static_cast<Job::Promise<void, void>*>(job->promise);
			// TODO: Need to check was promise value already set 
			promise->status.store(Job::State::Completed, std::memory_order_release);
		}

		if (job->caller) {
			fiber_context_switch(job->context, job->caller->context);
		}

		assert(0 && "Job returned without caller");
	}

	Job* Job::create(NotNull<const Allocator*> alloc, JobFn&& func) noexcept {
		if (!func.is_valid()) {
			return nullptr;
		}

		Scheduler* sched = thread_context.worker->scheduler;
		if (!sched) {
			return nullptr;
		}

		Job* job = nullptr;
		if (!sched->free_jobs.dequeue(&job)) {
			job = alloc->allocate<Job>();
		}

		if (!job) {
			return nullptr;
		}

		void* stack_ptr = thread_context.worker->scheduler->alloc_stack();
		if (!stack_ptr) {
			alloc->deallocate(job);
			return nullptr;
		}

		job->context = fiber_context_create(alloc, job_main, stack_ptr, EDGE_FIBER_STACK_SIZE);
		if (!job->context) {
			alloc->deallocate(job);
			thread_context.worker->scheduler->free_stack(stack_ptr);
			return nullptr;
		}

		job->state.store(Job::State::Suspended, std::memory_order_release);
		job->caller = nullptr;
		job->func = std::move(func);

		return job;
	}

	void Job::destroy(NotNull<const Allocator*> alloc, Job* self) noexcept {
		if (self->func.is_valid()) {
			self->func.destroy(alloc);
		}

		if (self->context) {
			void* stack_ptr = fiber_get_stack_ptr(self->context);
			if (stack_ptr) {
				thread_context.worker->scheduler->free_stack(stack_ptr);
			}

			fiber_context_destroy(alloc, self->context);
			self->context = nullptr;
		}

		Scheduler* sched = thread_context.worker->scheduler;
		if (sched) {
			sched->free_jobs.enqueue(self);
		}
	}

	Scheduler* Scheduler::create(NotNull<const Allocator*> alloc) noexcept {
		// NOTE: Should be created only on main thread, or on thread that i consider to be the main one.
		Scheduler* sched = alloc->allocate<Scheduler>();
		if (!sched) {
			return nullptr;
		}

		if (!sched->stack_arena.create()) {
			return nullptr;
		}

		if (!sched->free_stacks.create(alloc, 512)) {
			return nullptr;
		}

		if (!sched->free_jobs.create(alloc, 512)) {
			return nullptr;
		}

		for (i32 i = 0; i < static_cast<i32>(Job::Priority::Count); ++i) {
			if (!sched->queues[i].create(alloc, 1024)) {
				destroy(alloc, sched);
				return nullptr;
			}
		}

		CpuInfo cpu_info[128];
		i32 cpu_count = thread_get_cpu_topology(cpu_info, 128);

		i32 num_cores = thread_get_logical_core_count(cpu_info, cpu_count);
		if (num_cores <= 0) {
			num_cores = 4;
		}

		// TODO: Use fixed runtime array
		if (!sched->scheduler_threads.reserve(alloc, num_cores)) {
			destroy(alloc, sched);
			return nullptr;
		}

		sched->shutdown.store(false, std::memory_order_relaxed);
		sched->active_jobs.store(0, std::memory_order_relaxed);
		sched->worker_futex.store(0, std::memory_order_relaxed);
		sched->sleeping_workers.store(0, std::memory_order_relaxed);

		for (i32 i = 0; i < num_cores; ++i) {
			Worker* worker = Worker::create(alloc);
			if (!worker) {
				destroy(alloc, sched);
				return nullptr;
			}

			worker->scheduler = sched;
			worker->thread_id = i;

			if (!sched->scheduler_threads.push_back(alloc, worker)) {
				destroy(alloc, sched);
				return nullptr;
			}

			thread_set_affinity_ex(worker->thread_handle, cpu_info, cpu_count, i, false);

			char buffer[32] = { 0 };
			snprintf(buffer, sizeof(buffer), "worker-%d", i);
			thread_set_name(worker->thread_handle, buffer);
		}

		// NOTE: Does not create a real thread
		sched->main_thread = alloc->allocate<Worker>();
		if (!sched->main_thread) {
			destroy(alloc, sched);
			return nullptr;
		}

		sched->main_thread->allocator = alloc.m_ptr;
		sched->main_thread->thread_handle = thread_current();
		sched->main_thread->thread_id = thread_current_id();
		sched->main_thread->scheduler = sched;

		// NOTE: Init main therad context
		if (!thread_context.create(sched->main_thread)) {
			destroy(alloc, sched);
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

		if (!self->scheduler_threads.empty()) {
			for (Worker* worker_thread : self->scheduler_threads) {
				if (worker_thread) {
					Worker::destroy(alloc, worker_thread);
				}
			}

			self->scheduler_threads.destroy(alloc);
		}

		for (i32 i = 0; i < static_cast<i32>(Job::Priority::Count); ++i) {
			while (true) {
				Job* job;
				if (!self->queues[i].dequeue(&job)) {
					break;
				}
				Job::destroy(alloc, job);
			}

			self->queues[i].destroy(alloc);
		}

		for (auto& job : self->free_jobs) {
			alloc->deallocate(job);
		}
		self->free_jobs.destroy(alloc);

		self->free_stacks.destroy(alloc);
		self->stack_arena.destroy();

		alloc->deallocate(self);
	}

	void Scheduler::schedule(Job* job, Job::Priority prio) noexcept {
		active_jobs.fetch_add(1, std::memory_order_release);

		i32 priority_index = static_cast<i32>(prio);
		if (!queues[priority_index].enqueue(job)) {
			return;
		}

		// Wake workers
		if (sleeping_workers.load(std::memory_order_acquire) > 0) {
			worker_futex.fetch_add(1, std::memory_order_release);
			futex_wake_all(&worker_futex);
		}
	}

	void Scheduler::tick(f32 delta_time) noexcept {
		// NOTE: Called from the main engine loop.
		// TODO: Schedule jobs that must be executed on the main thread.
	}

	void Scheduler::run() noexcept {
		assert(main_thread->thread_id == thread_context.worker->thread_id && "Run can be called only from main thread.");

		while (
			active_jobs.load(std::memory_order_acquire) > 0 &&
			!shutdown.load(std::memory_order_acquire)) {
			thread_yield();
		}
	}

	void* Scheduler::alloc_stack() noexcept {
		void* stack = nullptr;
		if (!free_stacks.dequeue(&stack)) {
			return stack_arena.alloc_ex(EDGE_FIBER_STACK_SIZE, EDGE_FIBER_STACK_ALIGN);
		}
		return stack;
	}

	void Scheduler::free_stack(void* stack_ptr) noexcept {
		free_stacks.enqueue(stack_ptr);
	}

	std::pair<Job*, Job::Priority> Scheduler::pick_job() noexcept {
		for (i32 i = static_cast<i32>(Job::Priority::Count) - 1; i >= 0; i--) {
			Job* job;
			if (queues[i].dequeue(&job)) {
				return std::make_pair(job, static_cast<Job::Priority>(i));
			}
		}

		return std::make_pair(nullptr, Job::Priority::Count);
	}

	void Scheduler::complete_job(NotNull<const Allocator*> alloc, Job* job, Job::Priority priority, Job::State final_status) noexcept {
		// TODO: Check that job is completed or failed

		Job* continuation = job->continuation;
		if (continuation) {
			job->continuation = nullptr;
			schedule(continuation, priority);
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

		job->state.store(Job::State::Suspended, std::memory_order_release);
		fiber_context_switch(job->context, job->caller->context);
		job->state.store(Job::State::Running, std::memory_order_release);
	}

	void job_await(Job* child_job) noexcept {
		Job* parent_job = thread_context.current_job;
		thread_context.current_job = child_job;

		child_job->caller = parent_job->caller;
		child_job->continuation = parent_job;

		parent_job->state.store(Job::State::Suspended, std::memory_order_release);
		// NOTE: Switches to the child job, not back to the caller, like a yield.
		// This is somewhat hacky, but for now i'm not sure how to do it properly.
		// Also inherits parent's priority.
		fiber_context_switch(parent_job->context, child_job->context);
	}

	bool is_running_in_job() noexcept {
		return thread_context.current_job != &thread_context.main_job;
	}

	bool is_running_on_main() noexcept {
		Scheduler* sched = thread_context.worker->scheduler;
		return sched->main_thread == thread_context.worker;
	}

	void job_switch_to_main() noexcept {
		if (is_running_on_main()) {
			// Already in main
			return;
		}

		// NOTE: Should reschedule job on main thread
		// TODO: Not implemented
	}

	void job_switch_to_background() noexcept {
		if (!is_running_on_main()) {
			// Already on one of 
			return;
		}

		// TODO: Not implemented
	}
}