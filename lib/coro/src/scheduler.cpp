#include "scheduler.hpp"

#include <cstdio>
#include <cassert>
#include <ctime>

namespace edge {
	constexpr usize STACK_POOL_SIZE = 512;
	constexpr usize JOB_POOL_SIZE = 512;
	constexpr usize MAIN_QUEUE_SIZE = 64;
	constexpr usize IO_QUEUE_SIZE = 64;
	constexpr usize BACKGROUND_QUEUE_SIZE = 64;

	struct Scheduler::Worker {
		const Allocator* allocator = nullptr;
		Workgroup wg = Workgroup::Main;

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

		i32 loop() noexcept;
		bool tick() noexcept;
	};

	enum class FlowReturnType {
		None,
		Done,
		Yielded,
		Awaited,
		SwitchTo
	};

	struct FlowInfo {
		FlowReturnType type = FlowReturnType::None;
		Job* job = nullptr;
		Scheduler::Workgroup wg = Scheduler::Workgroup::Background;

		void clear() noexcept {
			type = FlowReturnType::None;
			job = nullptr;
		}
	};

	struct SchedulerThreadContext {
		Scheduler::Worker* worker = nullptr;

		Job* current_job = nullptr;
		Job main_job = {};
		FiberContext* main_context = nullptr;

		FlowInfo flow_info = {};

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
			if (!tick()) {
				break;
			}
		}

		thread_context.shutdown();

		return 0;
	}

	bool Scheduler::Worker::tick() noexcept {
		auto [job, priority] = scheduler->pick_job(wg);
		if (!job) {
			if (scheduler->shutdown.load(std::memory_order_acquire)) {
				return false;
			}

			// NOTE: The main worker does not need to sleep when there is no work.
			if (wg == Workgroup::Main) {
				return true;
			}

			scheduler->sleeping_workers.fetch_add(1, std::memory_order_relaxed);
			std::atomic_thread_fence(std::memory_order_seq_cst);
			u32 futex_val = scheduler->worker_futex.load(std::memory_order_acquire);
			futex_wait(&scheduler->worker_futex, futex_val, std::chrono::nanoseconds::max());
			scheduler->sleeping_workers.fetch_sub(1, std::memory_order_relaxed);
			return true;
		}

		Job::State expected = Job::State::Suspended;
		if (!job->state.compare_exchange_strong(expected, Job::State::Running, std::memory_order_acquire, std::memory_order_relaxed)) {
			// TODO: I think there is a bug here, and I might lose the job at this point.
			return true;
		}

		Job* caller = thread_context.current_job;
		job->caller = caller;

		caller->state.store(Job::State::Suspended, std::memory_order_release);
		thread_context.current_job = job;

		// Switch to job context
		fiber_context_switch(caller->context, job->context);

		thread_context.current_job = caller;
		caller->state.store(Job::State::Running, std::memory_order_release);

		Job::State job_state = job->state.load(std::memory_order_acquire);
		switch (thread_context.flow_info.type)
		{
		case FlowReturnType::Done: {
			thread_context.flow_info.clear();

			scheduler->active_jobs.fetch_sub(1, std::memory_order_release);

			if (job->continuation) {
				scheduler->enqueue_job(job->continuation, priority, wg);
				job->continuation = nullptr;
			}
			Job::destroy(allocator, job);

			return true;
		}
		case FlowReturnType::Yielded: {
			thread_context.flow_info.clear();
			// TODO: If it's not in suspended state, it means that soemthingis going very wrong.
			if (job_state == Job::State::Suspended) {
				scheduler->enqueue_job(job, priority, wg);
				return true;
			}
			return false;
		}
		case FlowReturnType::Awaited: {
			scheduler->active_jobs.fetch_add(1, std::memory_order_release);
			scheduler->enqueue_job(thread_context.flow_info.job, priority, wg);
			thread_context.flow_info.clear();
			return true;
		}
		case FlowReturnType::SwitchTo: {
			scheduler->enqueue_job(job, priority, thread_context.flow_info.wg);
			thread_context.flow_info.clear();
			return true;
		}
		default:
			return false;
		}
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

		thread_context.flow_info.type = FlowReturnType::Done;

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
			if (!sched->free_jobs.enqueue(self)) {
				alloc->deallocate(self);
			}
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

		if (!sched->free_stacks.create(alloc, STACK_POOL_SIZE)) {
			return nullptr;
		}

		if (!sched->free_jobs.create(alloc, JOB_POOL_SIZE)) {
			return nullptr;
		}

		if (!sched->main_queue.create(alloc, MAIN_QUEUE_SIZE)) {
			destroy(alloc, sched);
			return nullptr;
		}

		if (!sched->io_queue.create(alloc, IO_QUEUE_SIZE)) {
			destroy(alloc, sched);
			return nullptr;
		}

		Range<Job::Priority> range(Job::Priority::Low, Job::Priority::High);
		for (auto it = range.begin(); it != range.end(); ++it) {
			if (!sched->background_queues[it].create(alloc, BACKGROUND_QUEUE_SIZE)) {
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
		if (!sched->background_threads.reserve(alloc, num_cores)) {
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

			worker->wg = Workgroup::Background;
			worker->scheduler = sched;
			worker->thread_id = i;

			if (!sched->background_threads.push_back(alloc, worker)) {
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

		sched->main_thread->wg = Workgroup::Main;
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

		if (!self->background_threads.empty()) {
			for (Worker* worker_thread : self->background_threads) {
				if (worker_thread) {
					Worker::destroy(alloc, worker_thread);
				}
			}

			self->background_threads.destroy(alloc);
		}

		{
			Job* job = nullptr;
			while (self->main_queue.dequeue(&job)) {
				Job::destroy(alloc, job);
			}
			self->main_queue.destroy(alloc);
		}

		{
			Job* job = nullptr;
			while (self->io_queue.dequeue(&job)) {
				Job::destroy(alloc, job);
			}
			self->io_queue.destroy(alloc);
		}

		Range<Job::Priority> range(Job::Priority::Low, Job::Priority::High);
		for (auto it = range.begin(); it != range.end(); ++it) {
			MPMCQueue<Job*>& queue = self->background_queues[it];

			Job* job = nullptr;
			while (queue.dequeue(&job)) {
				Job::destroy(alloc, job);
			}

			queue.destroy(alloc);
		}

		for (auto& job : self->free_jobs) {
			alloc->deallocate(job);
		}
		self->free_jobs.destroy(alloc);

		self->free_stacks.destroy(alloc);
		self->stack_arena.destroy();

		alloc->deallocate(self);
	}

	void Scheduler::schedule(Job* job, Job::Priority prio, Workgroup wg) noexcept {
		active_jobs.fetch_add(1, std::memory_order_release);
		enqueue_job(job, prio, wg);
	}

	void Scheduler::tick(f32 delta_time) noexcept {
		// NOTE: Called from the main engine loop.
		// TODO: Schedule jobs that must be executed on the main thread.
		main_thread->tick();
	}

	void Scheduler::run() noexcept {
		assert(main_thread->thread_id == thread_context.worker->thread_id && "Run can be called only from main thread.");

		while (
			active_jobs.load(std::memory_order_acquire) > 0 &&
			!shutdown.load(std::memory_order_acquire)) {
			main_thread->tick();
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

	std::pair<Job*, Job::Priority> Scheduler::pick_job(Workgroup wg) noexcept {
		switch (wg)
		{
		case edge::Scheduler::Main: {
			Job* job;
			if (main_queue.dequeue(&job)) {
				return std::make_pair(job, Job::Priority::Count);
			}
			break;
		}
		case edge::Scheduler::IO: {
			Job* job;
			if (io_queue.dequeue(&job)) {
				return std::make_pair(job, Job::Priority::Count);
			}
			break;
		}
		case edge::Scheduler::Background: {
			Range<Job::Priority> range(Job::Priority::Low, Job::Priority::High);
			for (auto it = range.rbegin(); it != range.rend(); ++it) {
				Job* job;
				if (background_queues[it].dequeue(&job)) {
					return std::make_pair(job, *it);
				}
			}
			break;
		}
		}

		return std::make_pair(nullptr, Job::Priority::Count);
	}

	void Scheduler::enqueue_job(Job* job, Job::Priority prio, Workgroup wg) noexcept {

		switch (wg)
		{
		case edge::Scheduler::Main:
			main_queue.enqueue(job);
			break;
		case edge::Scheduler::IO:
			io_queue.enqueue(job);
			break;
		case edge::Scheduler::Background: {
			i32 priority_index = static_cast<i32>(prio);
			if (!background_queues[priority_index].enqueue(job)) {
				return;
			}
			break;
		}
		}

		// Wake workers
		if (sleeping_workers.load(std::memory_order_acquire) > 0) {
			worker_futex.fetch_add(1, std::memory_order_release);
			futex_wake_all(&worker_futex);
		}
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

	static void job_yield_base() noexcept {
		Job* job = thread_context.current_job;
		if (!job || job == &thread_context.main_job) {
			return;
		}

		job->state.store(Job::State::Suspended, std::memory_order_release);
		fiber_context_switch(job->context, job->caller->context);
		job->state.store(Job::State::Running, std::memory_order_release);
	}

	void job_yield() noexcept {
		thread_context.flow_info.type = FlowReturnType::Yielded;
		job_yield_base();
	}

	void job_await(Job* child_job) noexcept {
		child_job->continuation = thread_context.current_job;

		thread_context.flow_info.type = FlowReturnType::Awaited;
		thread_context.flow_info.job = child_job;
		job_yield_base();
	}

	bool is_running_in_job() noexcept {
		return thread_context.current_job != &thread_context.main_job;
	}

	bool is_running_on_main() noexcept {
		return thread_context.worker->wg == Scheduler::Workgroup::Main;
	}

	void job_switch_to_main() noexcept {
		if (thread_context.worker->wg == Scheduler::Workgroup::Main) {
			return;
		}

		thread_context.flow_info.type = FlowReturnType::SwitchTo;
		thread_context.flow_info.wg = Scheduler::Workgroup::Main;
		job_yield_base();
	}

	void job_switch_to_background() noexcept {
		if (thread_context.worker->wg == Scheduler::Workgroup::Background) {
			return;
		}

		thread_context.flow_info.type = FlowReturnType::SwitchTo;
		thread_context.flow_info.wg = Scheduler::Workgroup::Background;
		job_yield_base();
	}

	void job_switch_to_io() noexcept {
		if (thread_context.worker->wg == Scheduler::Workgroup::IO) {
			return;
		}

		thread_context.flow_info.type = FlowReturnType::SwitchTo;
		thread_context.flow_info.wg = Scheduler::Workgroup::IO;
		job_yield_base();
	}
}