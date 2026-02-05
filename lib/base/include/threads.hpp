#ifndef EDGE_THREADS_H
#define EDGE_THREADS_H

#include "stddef.hpp"

#include <atomic>
#include <chrono>

namespace edge {
enum class ThreadResult {
  Success = 0,
  Error = 1,
  NoMem = 2,
  TimedOut = 3,
  Busy = 4
};

enum class FutexResult { Success = 0, TimedOut = 1, Error = 2 };

enum class MutexType { Plain = 0, Recursive = 1, Timed = 2 };

struct CpuInfo {
  i32 logical_id;
  i32 physical_id;
  i32 core_id;
};

using ThreadFunc = i32 (*)(void *arg);

#if EDGE_HAS_WINDOWS_API
struct Thread {
  void *handle;
  u32 id;
};

struct Mutex {
  void *handle;
  MutexType type;
};

struct ConditionVariable {
  void *handle;
};

struct OnceFlag {
  long state;
};
#elif EDGE_PLATFORM_POSIX
struct Thread {
  unsigned long handle;
};

struct Mutex {
  unsigned char data[40];
  MutexType type;
};

struct ConditionVariable {
  unsigned char data[48];
};

struct OnceFlag {
  i32 state;
};
#else
#error "Unsupported platform"
#endif

ThreadResult thread_create(Thread *thr, ThreadFunc func, void *arg);
ThreadResult thread_join(const Thread &thr, i32 *res = nullptr);
ThreadResult thread_detach(const Thread &thr);
Thread thread_current();
u32 thread_current_id();
bool thread_equal(const Thread &lhs, const Thread &rhs);
void thread_exit(i32 res);
void thread_yield();
i32 thread_sleep(const std::chrono::nanoseconds &duration);

FutexResult futex_wait(std::atomic<u32> *addr, u32 expected_value,
                       std::chrono::nanoseconds timeout);
i32 futex_wake(std::atomic<u32> *addr, i32 count = 1);
i32 futex_wake_all(std::atomic<u32> *addr);

ThreadResult mutex_init(Mutex *mtx, MutexType type = MutexType::Plain);
void mutex_destroy(Mutex *mtx);
ThreadResult mutex_lock(const Mutex *mtx);
ThreadResult mutex_trylock(const Mutex *mtx);
ThreadResult mutex_timedlock(const Mutex *mtx,
                             const std::chrono::nanoseconds &timeout);
ThreadResult mutex_unlock(const Mutex *mtx);

ThreadResult cond_init(ConditionVariable *cnd);
void cond_destroy(ConditionVariable *cnd);
ThreadResult cond_signal(const ConditionVariable *cnd);
ThreadResult cond_broadcast(const ConditionVariable *cnd);
ThreadResult cond_wait(const ConditionVariable *cnd, const Mutex *mtx);
ThreadResult cond_timedwait(const ConditionVariable *cnd, const Mutex *mtx,
                            const std::chrono::nanoseconds &timeout);

void call_once(OnceFlag *flag, void (*func)());

ThreadResult thread_set_affinity(const Thread &thr, i32 core_id,
                                 bool prefer_physical = true);
ThreadResult thread_set_affinity_ex(const Thread &thr, const CpuInfo *cpu_info,
                                    i32 cpu_count, i32 core_id,
                                    bool prefer_physical = true);
ThreadResult thread_set_name(const Thread &thr, const char *name);

i32 thread_get_cpu_topology(CpuInfo *cpu_info, i32 max_cpus);
i32 thread_get_physical_core_count(const CpuInfo *cpu_info, i32 count);
i32 thread_get_logical_core_count(const CpuInfo *cpu_info, i32 count);

ThreadResult thread_set_affinity_platform(const Thread &thr, i32 core_id);
} // namespace edge

#endif