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
	struct Job {
		enum class State {
			Suspended,
			Running,
			Completed,
			Failed
		};

		enum class Priority {
			Low = 0,
			Normal = 1,
			High = 2,
			Count
		};

		template<typename T, typename E>
		struct Promise {
			std::atomic<State> status = State::Running;

			union {
				std::conditional_t<std::is_void_v<T>, std::monostate, T> value = {};
				std::conditional_t<std::is_void_v<E>, std::monostate, E> error;
			};

			bool is_done() const noexcept {
				State s = status.load(std::memory_order_acquire);
				return s == State::Completed || s == State::Failed;
			}

			decltype(auto) get_value() noexcept requires (!std::is_void_v<T>) {
				assert(status.load(std::memory_order_acquire) == State::Completed);
				return value;
			}

			decltype(auto) get_error() noexcept requires (!std::is_void_v<E>) {
				assert(status.load(std::memory_order_acquire) == State::Failed);
				return error;
			}
		};

		using JobFn = Callable<void()>;
		JobFn func = {};
		FiberContext* context = nullptr;

		Job* caller = nullptr;
		Job* continuation = nullptr;
		void* promise = nullptr;

		std::atomic<State> state = State::Running;

		template<typename F>
		static Job* from_lambda(NotNull<const Allocator*> alloc, F&& fn) noexcept {
			return create(alloc, callable_create_from_lambda(alloc, std::forward<F>(fn)));
		}

		static Job* create(NotNull<const Allocator*> alloc, JobFn&& func) noexcept;
		static void destroy(NotNull<const Allocator*> alloc, Job* self) noexcept;

		template<typename T, typename E>
		void set_promise(Promise<T, E>* promise_ptr) noexcept {
			promise = promise_ptr;
		}
	};

	struct Scheduler {
		struct Worker;

		friend struct Job;
		friend struct Worker;

		Arena stack_arena = {};
		// NOTE: To reuse allocated stacks.
		MPMCQueue<void*> free_stacks = {};

		// NOTE: To reuse allocated jobs.
		MPMCQueue<Job*> free_jobs = {};

		MPMCQueue<Job*> queues[static_cast<usize>(Job::Priority::Count)] = {};

		Worker* main_thread = nullptr;
		Array<Worker*> scheduler_threads = {};

		std::atomic<u32> active_jobs = 0;
		std::atomic<bool> shutdown = false;

		std::atomic<u32> worker_futex = 0;
		std::atomic<u32> sleeping_workers = 0;

		static Scheduler* create(NotNull<const Allocator*> alloc) noexcept;
		static void destroy(NotNull<const Allocator*> alloc, Scheduler* self) noexcept;

		void schedule(Job* job, Job::Priority prio = Job::Priority::Normal) noexcept;
		void tick(f32 delta_time) noexcept;

		void run() noexcept;

	private:
		void* alloc_stack() noexcept;
		void free_stack(void* stack_ptr) noexcept;

		std::pair<Job*, Job::Priority> pick_job() noexcept;
		void complete_job(NotNull<const Allocator*> alloc, Job* job, Job::Priority priority, Job::State final_status) noexcept;
	};

	Scheduler* sched_current() noexcept;

	Job* job_current() noexcept;
	i32 job_thread_id() noexcept;
	void job_yield() noexcept;
	void job_await(Job* child_job) noexcept;

	bool is_running_in_job() noexcept;
	bool is_running_on_main() noexcept;

	// NOTE: Yields job and runs it on main/background threads
	void job_switch_to_main() noexcept;
	void job_switch_to_background() noexcept;

	template<typename T>
	void job_return(T&& value) noexcept {
		Job* job = job_current();
		if (!job || !job->promise) {
			return;
		}

		auto* promise = static_cast<Job::Promise<std::decay_t<T>, void>*>(job->promise);
		promise->value = std::forward<T>(value);
		promise->status.store(Job::State::Completed, std::memory_order_release);
	}

	template<typename E>
	void job_failed(E&& error) noexcept {
		Job* job = job_current();
		if (!job || !job->promise) {
			return;
		}

		auto* promise = static_cast<Job::Promise<void, std::decay_t<E>>*>(job->promise);
		promise->error = std::forward<E>(error);
		promise->status.store(Job::State::Failed, std::memory_order_release);
	}
}

#endif