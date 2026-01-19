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
		Suspended,
		Running,
		Completed,
		Failed
	};

	template<typename T, typename E>
	struct JobPromise;

	template<typename T, typename E>
	struct JobPromise {
		std::atomic<JobState> status = JobState::Running;

		union {
			T value = {};
			E error;
		};

		bool is_done() const noexcept {
			JobState s = status.load(std::memory_order_acquire);
			return s == JobState::Completed || s == JobState::Failed;
		}

		T get_value() noexcept {
			assert(status.load(std::memory_order_acquire) == JobState::Completed);
			return value;
		}

		E get_error() noexcept {
			assert(status.load(std::memory_order_acquire) == JobState::Failed);
			return error;
		}
	};

	template<typename E>
	struct JobPromise<void, E> {
		std::atomic<JobState> status = JobState::Running;
		E error = {};

		bool is_done() const noexcept {
			JobState s = status.load(std::memory_order_acquire);
			return s == JobState::Completed || s == JobState::Failed;
		}

		E get_error() noexcept {
			assert(status.load(std::memory_order_acquire) == JobState::Failed);
			return error;
		}
	};

	template<typename T>
	struct JobPromise<T, void> {
		std::atomic<JobState> status = JobState::Running;
		T value = {};

		T get_value() noexcept {
			assert(status.load(std::memory_order_acquire) == JobState::Completed);
			return value;
		}
	};

	template<>
	struct JobPromise<void, void> {
		std::atomic<JobState> status = JobState::Running;

		bool is_done() const noexcept {
			JobState s = status.load(std::memory_order_acquire);
			return s == JobState::Completed || s == JobState::Failed;
		}
	};

	enum class SchedulerPriority {
		Low = 0,
		Normal = 1,
		High = 2,
		Count
	};

	using JobFn = Callable<void()>;

	struct Job {
		JobFn func = {};
		FiberContext* context = nullptr;

		Job* caller = nullptr;
		Job* continuation = nullptr;
		void* promise = nullptr;

		std::atomic<JobState> state = JobState::Running;
		SchedulerPriority priority = SchedulerPriority::Normal;

		template<typename F>
		static Job* from_lambda(NotNull<const Allocator*> alloc, F&& fn, SchedulerPriority prio = SchedulerPriority::Normal) noexcept {
			return create(alloc, callable_create_from_lambda(alloc, std::forward<F>(fn)), prio);
		}

		static Job* create(NotNull<const Allocator*> alloc, JobFn&& func, SchedulerPriority prio = SchedulerPriority::Normal) noexcept;
		static void destroy(NotNull<const Allocator*> alloc, Job* self) noexcept;

		bool update_state(JobState expected_state, JobState new_state) noexcept;
	};

	struct Scheduler {
		friend struct Job;
		friend struct WorkerThread;

		Arena stack_arena = {};
		MPMCQueue<void*> free_stacks = {};

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

		void schedule(Job* job) noexcept;

		void run() noexcept;

	private:
		void* alloc_stack() noexcept;
		void free_stack(void* stack_ptr) noexcept;

		Job* pick_job() noexcept;
		void enqueue_job(Job* job) noexcept;
		void complete_job(NotNull<const Allocator*> alloc, Job* job, JobState final_status) noexcept;
	};

	Scheduler* sched_current() noexcept;

	Job* job_current() noexcept;
	i32 job_thread_id() noexcept;
	void job_yield() noexcept;
	void job_await(Job* child_job, void* promise = nullptr) noexcept;

	template<typename T, typename E>
		requires (!std::same_as<JobPromise<T, E>*, void*>)
	void job_await(Job* child_job, JobPromise<T, E>* promise) noexcept {
		job_await(child_job, static_cast<void*>(promise));
	}

	template<typename T>
	void job_return(T&& value) noexcept {
		Job* job = job_current();
		if (!job || !job->promise) {
			return;
		}

		auto* promise = static_cast<JobPromise<std::decay_t<T>, void>*>(job->promise);
		promise->value = std::forward<T>(value);
		promise->status.store(JobState::Completed, std::memory_order_release);
	}

	template<typename E>
	void job_failed(E&& error) noexcept {
		Job* job = job_current();
		if (!job || !job->promise) {
			return;
		}

		auto* promise = static_cast<JobPromise<void, std::decay_t<E>>*>(job->promise);
		promise->error = std::forward<E>(error);
		promise->status.store(JobState::Failed, std::memory_order_release);
	}
}

#endif