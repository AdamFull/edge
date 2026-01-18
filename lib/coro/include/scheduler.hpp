#ifndef EDGE_SCHEDULER_HPP
#define EDGE_SCHEDULER_HPP

#include <arena.hpp>
#include <array.hpp>
#include <callable.hpp>
#include <mpmc_queue.hpp>
#include <threads.hpp>

#include "fiber.hpp"

#include <atomic>

namespace edge {
	struct WorkerThread;

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

	struct Job {
		FiberContext* context = nullptr;
		JobFn func = {};
		JobState state = {};
		Job* caller = nullptr;
		Job* awaiter = nullptr;
		// NOTE: When we awaiting, we saving awaiter and changing context to this job, but what to do with caller? Swap callers?

		SchedulerPriority priority = {};

		template<typename F>
		static Job* from_lambda(NotNull<const Allocator*> alloc, F&& fn, SchedulerPriority prio = SchedulerPriority::Normal) noexcept {
			return create(alloc, callable_create_from_lambda(alloc, std::forward<F>(fn)), prio);
		}

		static Job* create(NotNull<const Allocator*> alloc, JobFn&& func, SchedulerPriority prio = SchedulerPriority::Normal) noexcept;
		static void destroy(NotNull<const Allocator*> alloc, Job* self) noexcept;

		bool update_state(JobState expected_state, JobState new_state) noexcept;
	};

	struct Scheduler {
		friend struct WorkerThread;

		const Allocator* allocator = nullptr;

		MPMCQueue<Job*> queues[static_cast<usize>(SchedulerPriority::Count)] = {};

		WorkerThread* main_thread = nullptr;
		Array<WorkerThread*> worker_threads = {};

		std::atomic<usize> active_jobs = 0;
		std::atomic<usize> queued_jobs = 0;
		std::atomic<bool> shutdown = false;

		std::atomic<usize> jobs_completed = 0;
		std::atomic<usize> jobs_failed = 0;

		std::atomic<u32> worker_futex = 0;
		std::atomic<u32> sleeping_workers = 0;

		static Scheduler* create(NotNull<const Allocator*> alloc) noexcept;
		static void destroy(NotNull<const Allocator*> alloc, Scheduler* self) noexcept;

		// TODO: May be return some kind of completion token?
		void schedule(Job* job) noexcept;

		void run() noexcept;

	private:
		Job* pick_job() noexcept;
		void enqueue_job(Job* job) noexcept;
	};

	Scheduler* sched_current() noexcept;

	Job* job_current() noexcept;
	i32 job_thread_id() noexcept;
	void job_yield() noexcept;
	void job_await(Job* child_job) noexcept;
}

#endif