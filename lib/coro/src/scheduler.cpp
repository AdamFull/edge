#include "scheduler.hpp"

#include <vmem.hpp>

#include <cstdio>
#include <cassert>
#include <ctime>

namespace edge {
	constexpr usize STACK_POOL_SIZE = 512;
	constexpr usize JOB_POOL_SIZE = 512;
	constexpr usize MAIN_QUEUE_SIZE = 64;
	constexpr usize IO_QUEUE_SIZE = 64;
	constexpr usize BACKGROUND_QUEUE_SIZE = 64;

	struct StackAllocatorConfig {
		usize allocation_size = 65536;
		usize allocation_alignment = 16;
		usize guard_size = 4096;
		usize allocation_count = 1048576;
	};

	struct StackAllocator {
		void* region_base = nullptr;
		usize region_size = 0;
		usize stack_size = 0;
		usize guard_size = 0;
		usize allocation_stride = 0;

		std::atomic<usize> current_offset = 0ull;
		MPMCQueue<void*> free_blocks = {};

		usize page_size = 0ull;
		usize granularity = 0ull;

		static StackAllocator* create(NotNull<const Allocator*> alloc, StackAllocatorConfig config);
		static void destroy(NotNull<const Allocator*> alloc, StackAllocator* self);

		void* allocate(usize block_size);
		void free(void* stack_ptr);
	};

	StackAllocator* StackAllocator::create(NotNull<const Allocator*> alloc, StackAllocatorConfig config) {
		StackAllocator* self = alloc->allocate<StackAllocator>();
		if (!self) {
			return nullptr;
		}

		self->page_size = vmem_page_size();
		self->granularity = vmem_allocation_granularity();

		self->stack_size = align_up(config.allocation_size, self->page_size);
		self->guard_size = align_up(config.guard_size, self->page_size);

		self->allocation_stride = self->guard_size + self->stack_size + self->guard_size;

		self->region_size = self->allocation_stride * config.allocation_count;
		self->region_size = align_up(self->region_size, self->granularity);

		if (!vmem_reserve(&self->region_base, self->region_size)) {
			destroy(alloc, self);
			return nullptr;
		}

		if (!self->free_blocks.create(alloc, config.allocation_count)) {
			destroy(alloc, self);
			return nullptr;
		}

		return self;
	}

	void StackAllocator::destroy(NotNull<const Allocator*> alloc, StackAllocator* self) {
		if (self->region_base) {
			vmem_release(self->region_base, self->region_size);
		}

		self->free_blocks.destroy(alloc);
		alloc->deallocate(self);
	}

	void* StackAllocator::allocate(usize block_size) {
		void* stack_ptr = nullptr;
		if (free_blocks.dequeue(&stack_ptr)) {
			return stack_ptr;
		}

		usize offset = current_offset.fetch_add(allocation_stride, std::memory_order_relaxed);
		if (offset + allocation_stride > region_size) {
			return nullptr;
		}

		usize total_size = guard_size + stack_size + guard_size;

		u8* allocation_base = (u8*)region_base + offset;
		if (!vmem_commit(allocation_base, total_size)) {
			return nullptr;
		}

		if (!vmem_protect(allocation_base, guard_size, VMemProt::None)) {
			return nullptr;
		}

		u8* top_guard = allocation_base + guard_size + stack_size;
		if (!vmem_protect(top_guard, guard_size, VMemProt::None)) {
			return nullptr;
		}

		return allocation_base + guard_size;
	}

	void StackAllocator::free(void* stack_ptr) {
		free_blocks.enqueue(stack_ptr);
	}

	struct Scheduler::Worker {
		const Allocator* allocator = nullptr;
		Workgroup wg = Workgroup::Main;

		Thread thread_handle = {};
		Scheduler* scheduler = nullptr;
		usize thread_id = 0;
		std::atomic<bool> should_exit = false;

		static Worker* create(NotNull<const Allocator*> alloc);
		static void destroy(NotNull<const Allocator*> alloc, Worker* self);

		static i32 entry(void* arg) {
			Worker* worker = static_cast<Worker*>(arg);
			return worker->loop();
		}

		i32 loop();
		bool tick();
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

		void clear() {
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

		bool create(Scheduler::Worker* worker);
		void shutdown();
	};

	static thread_local SchedulerThreadContext thread_context = { };

	Scheduler::Worker* Scheduler::Worker::create(NotNull<const Allocator*> alloc) {
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

	void Scheduler::Worker::destroy(NotNull<const Allocator*> alloc, Scheduler::Worker* self) {
		self->should_exit.store(true, std::memory_order_release);
		thread_join(self->thread_handle, nullptr);
		alloc->deallocate(self);
	}

	i32 Scheduler::Worker::loop() {
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

	bool Scheduler::Worker::tick() {
		auto job = scheduler->pick_job(wg);
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

			Job* next_job = job->continuation;
			if (next_job) {
				job->continuation = nullptr;
				scheduler->enqueue_job(next_job, next_job->priority, wg);
			}
			Job::destroy(allocator, job);

			return true;
		}
		case FlowReturnType::Yielded: {
			thread_context.flow_info.clear();
			// TODO: If it's not in suspended state, it means that soemthingis going very wrong.
			if (job_state == Job::State::Suspended) {
				scheduler->enqueue_job(job, job->priority, wg);
				return true;
			}
			return false;
		}
		case FlowReturnType::Awaited: {
			Job* awaiter = thread_context.flow_info.job;
			awaiter->priority = job->priority;

			scheduler->active_jobs.fetch_add(1, std::memory_order_release);
			scheduler->enqueue_job(awaiter, awaiter->priority, wg);
			thread_context.flow_info.clear();
			return true;
		}
		case FlowReturnType::SwitchTo: {
			scheduler->enqueue_job(job, job->priority, thread_context.flow_info.wg);
			thread_context.flow_info.clear();
			return true;
		}
		default:
			return false;
		}
	}

	bool SchedulerThreadContext::create(Scheduler::Worker* worker) {
		this->worker = worker;
		main_context = fiber_context_create(worker->allocator, nullptr, nullptr, 0);
		main_job.context = main_context;
		current_job = &main_job;

		main_job.state.store(Job::State::Running, std::memory_order_release);

		return true;
	}

	void SchedulerThreadContext::shutdown() {
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

	Job* Job::create(NotNull<const Allocator*> alloc, JobFn&& func) {
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

		void* stack_ptr = thread_context.worker->scheduler->stack_alloc->allocate(EDGE_FIBER_STACK_SIZE);
		if (!stack_ptr) {
			alloc->deallocate(job);
			return nullptr;
		}

		job->context = fiber_context_create(alloc, job_main, stack_ptr, EDGE_FIBER_STACK_SIZE);
		if (!job->context) {
			alloc->deallocate(job);
			thread_context.worker->scheduler->stack_alloc->free(stack_ptr);
			return nullptr;
		}

		job->state.store(Job::State::Suspended, std::memory_order_release);
		job->caller = nullptr;
		job->func = std::move(func);

		return job;
	}

	void Job::destroy(NotNull<const Allocator*> alloc, Job* self) {
		if (self->func.is_valid()) {
			self->func.destroy(alloc);
		}

		if (self->context) {
			void* stack_ptr = fiber_get_stack_ptr(self->context);
			if (stack_ptr) {
				thread_context.worker->scheduler->stack_alloc->free(stack_ptr);
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

	Scheduler* Scheduler::create(NotNull<const Allocator*> alloc) {
		// NOTE: Should be created only on main thread, or on thread that i consider to be the main one.
		Scheduler* sched = alloc->allocate<Scheduler>();
		if (!sched) {
			return nullptr;
		}

		StackAllocatorConfig stack_alloc_cfg = {};
		sched->stack_alloc = StackAllocator::create(alloc, stack_alloc_cfg);
		if (!sched->stack_alloc) {
			destroy(alloc, sched);
			return nullptr;
		}

		if (!sched->free_jobs.create(alloc, JOB_POOL_SIZE)) {
			destroy(alloc, sched);
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

		i32 num_cores = thread_get_physical_core_count(cpu_info, cpu_count);
		if (num_cores <= 0) {
			num_cores = 4;
		}

		if (!sched->io_threads.reserve(alloc, num_cores)) {
			destroy(alloc, sched);
			return nullptr;
		}

		if (!sched->background_threads.reserve(alloc, num_cores)) {
			destroy(alloc, sched);
			return nullptr;
		}

		sched->shutdown.store(false, std::memory_order_relaxed);
		sched->active_jobs.store(0, std::memory_order_relaxed);
		sched->worker_futex.store(0, std::memory_order_relaxed);
		sched->sleeping_workers.store(0, std::memory_order_relaxed);

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

		for (i32 i = 0; i < num_cores; ++i) {
			char buffer[32] = { 0 };

			{
				Worker* worker = Worker::create(alloc);
				if (!worker) {
					destroy(alloc, sched);
					return nullptr;
				}

				worker->wg = Workgroup::IO;
				worker->scheduler = sched;
				worker->thread_id = i;

				if (!sched->io_threads.push_back(alloc, worker)) {
					destroy(alloc, sched);
					return nullptr;
				}

				thread_set_affinity_ex(worker->thread_handle, cpu_info, cpu_count, i, false);

				snprintf(buffer, sizeof(buffer), "io-%d", i);
				thread_set_name(worker->thread_handle, buffer);
			}

			{
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

				snprintf(buffer, sizeof(buffer), "background-%d", i);
				thread_set_name(worker->thread_handle, buffer);
			}
		}

		return sched;
	}

	void Scheduler::destroy(NotNull<const Allocator*> alloc, Scheduler* self) {
		assert(self->main_thread->thread_id == thread_context.worker->thread_id && "Destroy can be called only from main thread.");

		thread_context.shutdown();

		if (self->main_thread) {
			alloc->deallocate(self->main_thread);
		}

		self->shutdown.store(true, std::memory_order_release);

		self->worker_futex.fetch_add(1, std::memory_order_release);
		futex_wake_all(&self->worker_futex);

		if (!self->io_threads.empty()) {
			for (Worker* worker_thread : self->io_threads) {
				if (worker_thread) {
					Worker::destroy(alloc, worker_thread);
				}
			}

			self->io_threads.destroy(alloc);
		}

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

		if (self->stack_alloc) {
			StackAllocator::destroy(alloc, self->stack_alloc);
		}

		alloc->deallocate(self);
	}

	void Scheduler::schedule(Job* job, Job::Priority prio, Workgroup wg) {
		active_jobs.fetch_add(1, std::memory_order_release);
		job->priority = prio;
		enqueue_job(job, prio, wg);
	}

	void Scheduler::tick() {
		// NOTE: Called from the main engine loop.
		// TODO: Schedule jobs that must be executed on the main thread.
		main_thread->tick();
	}

	void Scheduler::run() {
		assert(main_thread->thread_id == thread_context.worker->thread_id && "Run can be called only from main thread.");

		while (
			active_jobs.load(std::memory_order_acquire) > 0 &&
			!shutdown.load(std::memory_order_acquire)) {
			main_thread->tick();
			thread_yield();
		}
	}

	Job* Scheduler::pick_job(Workgroup wg) {
		Job* job = nullptr;
		switch (wg)
		{
		case edge::Scheduler::Main: {
			if (main_queue.dequeue(&job)) {
				return job;
			}
			break;
		}
		case edge::Scheduler::IO: {
			if (io_queue.dequeue(&job)) {
				return job;
			}
			break;
		}
		case edge::Scheduler::Background: {
			Range<Job::Priority> range(Job::Priority::Low, Job::Priority::High);
			for (auto it = range.rbegin(); it != range.rend(); ++it) {
				if (background_queues[it].dequeue(&job)) {
					return job;
				}
			}
			break;
		}
		}

		return job;
	}

	void Scheduler::enqueue_job(Job* job, Job::Priority prio, Workgroup wg) {
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

	Scheduler* sched_current() {
		return thread_context.worker->scheduler;
	}

	Job* job_current() {
		return thread_context.current_job;
	}

	i32 job_thread_id() {
		return thread_context.worker->thread_id;
	}

	bool is_running_in_job() {
		return thread_context.current_job != &thread_context.main_job;
	}

	bool is_running_on_main() {
		return thread_context.worker->wg == Scheduler::Workgroup::Main;
	}

	static void job_yield_base() {
		Job* job = thread_context.current_job;
		if (!job || job == &thread_context.main_job) {
			return;
		}

		job->state.store(Job::State::Suspended, std::memory_order_release);
		fiber_context_switch(job->context, job->caller->context);
		job->state.store(Job::State::Running, std::memory_order_release);
	}

	void job_yield() {
		thread_context.flow_info.type = FlowReturnType::Yielded;
		job_yield_base();
	}

	void job_await(Job* child_job) {
		child_job->continuation = thread_context.current_job;

		thread_context.flow_info.type = FlowReturnType::Awaited;
		thread_context.flow_info.job = child_job;
		job_yield_base();
	}

	void job_continue_on_main() {
		if (thread_context.worker->wg == Scheduler::Workgroup::Main) {
			return;
		}

		thread_context.flow_info.type = FlowReturnType::SwitchTo;
		thread_context.flow_info.wg = Scheduler::Workgroup::Main;
		job_yield_base();
	}

	void job_continue_on_background() {
		if (thread_context.worker->wg == Scheduler::Workgroup::Background) {
			return;
		}

		thread_context.flow_info.type = FlowReturnType::SwitchTo;
		thread_context.flow_info.wg = Scheduler::Workgroup::Background;
		job_yield_base();
	}

	void job_continue_on_io() {
		if (thread_context.worker->wg == Scheduler::Workgroup::IO) {
			return;
		}

		thread_context.flow_info.type = FlowReturnType::SwitchTo;
		thread_context.flow_info.wg = Scheduler::Workgroup::IO;
		job_yield_base();
	}
}