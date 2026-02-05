#ifndef EDGE_SCHEDULER_HPP
#define EDGE_SCHEDULER_HPP

#include "array.hpp"
#include "callable.hpp"
#include "mpmc_queue.hpp"
#include "span.hpp"
#include "threads.hpp"

#include "fiber.hpp"

#include <atomic>

namespace edge {
struct Scheduler;
struct StackAllocator;

static constexpr usize BACKGROUND_QUEUE_COUNT = 2;

struct Job {
  enum class State { Suspended, Running, Completed, Failed };

  enum class Priority { Low = 0, High = 1 };

  template <typename T, typename E> struct Promise {
    std::atomic<State> status = State::Running;

    union {
      std::conditional_t<std::is_void_v<T>, std::monostate, T> value = {};
      std::conditional_t<std::is_void_v<E>, std::monostate, E> error;
    };

    bool is_done() const {
      State s = status.load(std::memory_order_acquire);
      return s == State::Completed || s == State::Failed;
    }

    decltype(auto) get_value()
      requires(!std::is_void_v<T>)
    {
      assert(status.load(std::memory_order_acquire) == State::Completed);
      return value;
    }

    decltype(auto) get_error()
      requires(!std::is_void_v<E>)
    {
      assert(status.load(std::memory_order_acquire) == State::Failed);
      return error;
    }
  };

  using JobFn = Callable<void()>;
  JobFn func = {};
  FiberContext *context = nullptr;

  Job *caller = nullptr;
  Job *continuation = nullptr;
  void *promise = nullptr;

  std::atomic<State> state = State::Running;
  Priority priority = Priority::Low;

  template <typename F>
  static Job *from_lambda(NotNull<const Allocator *> alloc,
                          const NotNull<Scheduler *> sched, F &&fn,
                          Priority prio = Priority::High) {
    return create(alloc, sched,
                  callable_create_from_lambda(alloc, std::forward<F>(fn)), prio);
  }

  static Job *create(NotNull<const Allocator *> alloc,
                     NotNull<Scheduler *> sched, JobFn &&func,
                     Priority prio = Priority::High);
  static void destroy(NotNull<const Allocator *> alloc, Job *self);

  template <typename T, typename E>
  void set_promise(Promise<T, E> *promise_ptr) {
    promise = promise_ptr;
  }
};

struct Scheduler {
  struct Worker;

  friend struct Job;
  friend struct Worker;

  enum Workgroup { Main, IO, Background };

  StackAllocator *stack_alloc = nullptr;

  // NOTE: To reuse allocated jobs.
  MPMCQueue<Job *> free_jobs = {};

  MPMCQueue<Job *> main_queue = {};
  Worker *main_thread = nullptr;

  // TODO: Implement io
  MPMCQueue<Job *> io_queue = {};
  Array<Worker *> io_threads = {};

  MPMCQueue<Job *> background_queues[BACKGROUND_QUEUE_COUNT] = {};
  Array<Worker *> background_threads = {};

  std::atomic<u32> active_jobs = 0;
  std::atomic<bool> shutdown = false;

  std::atomic<u32> worker_futex = 0;
  std::atomic<u32> sleeping_workers = 0;

  static Scheduler *create(NotNull<const Allocator *> alloc);
  static void destroy(NotNull<const Allocator *> alloc, Scheduler *self);

  void schedule(Job *job, Workgroup wg = Background);
  void schedule(Span<Job *> jobs, Workgroup wg = Background);
  void tick() const;

  void run() const;

private:
  Job *pick_job(Workgroup wg);
  void enqueue_job(Job *job, Job::Priority prio, Workgroup wg);
  void enqueue_jobs(Span<Job *> job, Workgroup wg);
};

Scheduler *sched_current();

Job *job_current();
i32 job_thread_id();

bool is_running_in_job();
bool is_running_on_main();

void job_yield();
void job_await(Job *child_job);

// NOTE: Yields job and runs it on main/background/io threads
void job_continue_on_main();
void job_continue_on_background();
void job_continue_on_io();

template <typename T> void job_return(T &&value) {
  Job *job = job_current();
  if (!job || !job->promise) {
    return;
  }

  auto *promise =
      static_cast<Job::Promise<std::decay_t<T>, void> *>(job->promise);
  promise->value = std::forward<T>(value);
  promise->status.store(Job::State::Completed, std::memory_order_release);
}

template <typename E> void job_failed(E &&error) {
  Job *job = job_current();
  if (!job || !job->promise) {
    return;
  }

  auto *promise =
      static_cast<Job::Promise<void, std::decay_t<E>> *>(job->promise);
  promise->error = std::forward<E>(error);
  promise->status.store(Job::State::Failed, std::memory_order_release);
}
} // namespace edge

#endif