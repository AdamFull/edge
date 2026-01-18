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

	enum class JobStatus {
		Running,
		Done,
		Error
	};

	template<typename T, typename E>
	struct JobPromise {
		std::atomic<JobStatus> has_result = {};
		union {
			T value = {};
			E error;
		};
	};

	template<typename E>
	struct JobPromise<void, E> {
		std::atomic<JobStatus> has_result = {};
		union {
			E error;
		};
	};

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
		JobFn func = {};
		FiberContext* context = nullptr;
		Job* caller = nullptr;
		Job* awaiter = nullptr;
		void* promise = nullptr;
		// NOTE: When we awaiting, we saving awaiter and changing context to this job, but what to do with caller? Swap callers?

		JobState state = {};
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
	void job_await(Job* child_job, void* promise = nullptr) noexcept;

	// TODO: Needed variant without promise
	template<typename T, typename E>
		requires (!std::same_as<JobPromise<T, E>*, void*>)
	void job_await(Job* child_job, JobPromise<T, E>* promise) noexcept {
		job_await(child_job, static_cast<void*>(promise));
	}

	template<typename T, typename E>
	void job_return(auto value) noexcept {
		static_assert(std::is_same_v<decltype(value), T> || std::is_same_v<decltype(value), E>,
			"Value must be either T or E");

		Job* job = job_current();
		if (!job || !job->promise) {
			return;
		}

		auto* promise = static_cast<JobPromise<T, E>*>(job->promise);
		if constexpr (std::is_same_v<decltype(value), T>) {
			promise->has_result = JobStatus::Done;
			promise->value = value;
		}
		else if constexpr (std::is_same_v<decltype(value), E>) {
			promise->has_result = JobStatus::Error;
			promise->error = value;
		}
	}
}

#endif