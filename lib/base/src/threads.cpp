#include "threads.hpp"

#if EDGE_HAS_WINDOWS_API
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>

#pragma comment(lib, "synchronization.lib")

namespace edge {
	namespace detail {
		struct ThreadStartInfo {
			ThreadFunc func;
			void* arg;
		};

		static unsigned int __stdcall thread_start_wrapper(void* arg) {
			ThreadStartInfo* info = static_cast<ThreadStartInfo*>(arg);
			ThreadFunc func = info->func;
			void* user_arg = info->arg;
			delete info;

			i32 result = func(user_arg);
			return static_cast<unsigned int>(result);
		}
	}

	ThreadResult thread_create(Thread* thr, ThreadFunc func, void* arg) {
		if (!thr || !func) {
			return ThreadResult::Error;
		}

		detail::ThreadStartInfo* info = new detail::ThreadStartInfo{ func, arg };

		thr->handle = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, detail::thread_start_wrapper, info, 0, &thr->id));
		if (!thr->handle) {
			delete info;
			return ThreadResult::Error;
		}
		return ThreadResult::Success;
	}

	ThreadResult thread_join(Thread& thr, i32* res) {
		if (!thr.handle) {
			return ThreadResult::Error;
		}

		DWORD wait_result = WaitForSingleObject(thr.handle, INFINITE);
		if (wait_result != WAIT_OBJECT_0) {
			return ThreadResult::Error;
		}

		if (res) {
			DWORD exit_code;
			if (GetExitCodeThread(thr.handle, &exit_code)) {
				*res = static_cast<i32>(exit_code);
			}
		}

		CloseHandle(thr.handle);
		return ThreadResult::Success;
	}

	ThreadResult thread_detach(Thread& thr) {
		if (!thr.handle) {
			return ThreadResult::Error;
		}
		CloseHandle(thr.handle);
		return ThreadResult::Success;
	}

	Thread thread_current() {
		Thread thr;
		thr.handle = GetCurrentThread();
		thr.id = GetCurrentThreadId();
		return thr;
	}

	u32 thread_current_id() {
		Thread thrd = thread_current();
		return thrd.id;
	}

	bool thread_equal(const Thread& lhs, const Thread& rhs) {
		return lhs.id == rhs.id;
	}

	void thread_exit(i32 res) noexcept {
		_endthreadex(static_cast<unsigned int>(res));
	}

	void thread_yield() {
		SwitchToThread();
	}

	i32 thread_sleep(const std::chrono::nanoseconds& duration) {
		auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
		Sleep(static_cast<DWORD>(ms.count()));
		return 0;
	}

	FutexResult futex_wait(std::atomic<u32>* addr, u32 expected_value, std::chrono::nanoseconds timeout) {
		if (!addr) {
			return FutexResult::Error;
		}

		DWORD timeout_ms = INFINITE;
		if (timeout != std::chrono::nanoseconds::zero()) {
			auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(timeout);
			timeout_ms = static_cast<DWORD>(ms.count());
		}

		BOOL result = WaitOnAddress(addr, &expected_value, sizeof(u32), timeout_ms);
		if (!result) {
			DWORD error = GetLastError();
			if (error == ERROR_TIMEOUT) {
				return FutexResult::TimedOut;
			}
			return FutexResult::Error;
		}

		return FutexResult::Success;
	}

	i32 futex_wake(std::atomic<u32>* addr, i32 count) {
		if (!addr) {
			return 0;
		}

		if (count == 1) {
			WakeByAddressSingle(addr);
		}
		else {
			WakeByAddressAll(addr);
		}

		return count;
	}

	i32 futex_wake_all(std::atomic<u32>* addr) {
		if (!addr) {
			return 0;
		}

		WakeByAddressAll(addr);
		return 1;
	}

	ThreadResult mutex_init(Mutex* mtx, MutexType type) {
		if (!mtx) {
			return ThreadResult::Error;
		}

		mtx->type = type;
		mtx->handle = CreateMutex(nullptr, FALSE, nullptr);

		if (!mtx->handle) {
			return ThreadResult::Error;
		}
		return ThreadResult::Success;
	}

	void mutex_destroy(Mutex* mtx) {
		if (!mtx) {
			return;
		}

		if (mtx->handle) {
			CloseHandle(mtx->handle);
			mtx->handle = nullptr;
		}
	}

	ThreadResult mutex_lock(Mutex* mtx) {
		if (!mtx) {
			return ThreadResult::Error;
		}

		DWORD result = WaitForSingleObject(mtx->handle, INFINITE);
		return result == WAIT_OBJECT_0 ? ThreadResult::Success : ThreadResult::Error;
	}

	ThreadResult mutex_trylock(Mutex* mtx) {
		if (!mtx) {
			return ThreadResult::Error;
		}

		DWORD result = WaitForSingleObject(mtx->handle, 0);
		if (result == WAIT_OBJECT_0) {
			return ThreadResult::Success;
		}
		else if (result == WAIT_TIMEOUT) {
			return ThreadResult::Busy;
		}
		return ThreadResult::Error;
	}

	ThreadResult mutex_timedlock(Mutex* mtx, const std::chrono::nanoseconds& timeout) {
		if (!mtx) {
			return ThreadResult::Error;
		}

		auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(timeout);
		DWORD result = WaitForSingleObject(mtx->handle, static_cast<DWORD>(ms.count()));
		if (result == WAIT_OBJECT_0) {
			return ThreadResult::Success;
		}
		else if (result == WAIT_TIMEOUT) {
			return ThreadResult::TimedOut;
		}
		return ThreadResult::Error;
	}

	ThreadResult mutex_unlock(Mutex* mtx) {
		if (!mtx) {
			return ThreadResult::Error;
		}

		return ReleaseMutex(mtx->handle) ? ThreadResult::Success : ThreadResult::Error;
	}

	ThreadResult cond_init(ConditionVariable* cnd) {
		if (!cnd) {
			return ThreadResult::Error;
		}

		cnd->handle = CreateEvent(nullptr, TRUE, FALSE, nullptr);
		if (!cnd->handle) {
			return ThreadResult::Error;
		}
		return ThreadResult::Success;
	}

	void cond_destroy(ConditionVariable* cnd) {
		if (!cnd) {
			return;
		}

		if (cnd->handle) {
			CloseHandle(cnd->handle);
			cnd->handle = nullptr;
		}
	}

	ThreadResult cond_signal(ConditionVariable* cnd) {
		if (!cnd) {
			return ThreadResult::Error;
		}

		return SetEvent(cnd->handle) ? ThreadResult::Success : ThreadResult::Error;
	}

	ThreadResult cond_broadcast(ConditionVariable* cnd) {
		if (!cnd) {
			return ThreadResult::Error;
		}

		return SetEvent(cnd->handle) ? ThreadResult::Success : ThreadResult::Error;
	}

	ThreadResult cond_wait(ConditionVariable* cnd, Mutex* mtx) {
		if (!cnd || !mtx) {
			return ThreadResult::Error;
		}

		// Release mutex, wait for signal, reacquire mutex
		mutex_unlock(mtx);
		DWORD result = WaitForSingleObject(cnd->handle, INFINITE);
		ResetEvent(cnd->handle);
		mutex_lock(mtx);
		return result == WAIT_OBJECT_0 ? ThreadResult::Success : ThreadResult::Error;
	}

	ThreadResult cond_timedwait(ConditionVariable* cnd, Mutex* mtx, const std::chrono::nanoseconds& timeout) {
		if (!cnd || !mtx) {
			return ThreadResult::Error;
		}

		auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(timeout);

		mutex_unlock(mtx);
		DWORD result = WaitForSingleObject(cnd->handle, static_cast<DWORD>(ms.count()));
		ResetEvent(cnd->handle);
		mutex_lock(mtx);

		if (result == WAIT_OBJECT_0) {
			return ThreadResult::Success;
		}
		else if (result == WAIT_TIMEOUT) {
			return ThreadResult::TimedOut;
		}
		return ThreadResult::Error;
	}

	void call_once(OnceFlag* flag, void(*func)()) {
		if (!flag || !func) {
			return;
		}

		if (InterlockedCompareExchange(&flag->state, 1, 0) == 0) {
			func();
			InterlockedExchange(&flag->state, 2);
		}
		else {
			while (flag->state != 2) {
				SwitchToThread();
			}
		}
	}

	ThreadResult thread_set_affinity_platform(Thread& thr, i32 core_id) {
		if (core_id < 0) {
			return ThreadResult::Error;
		}

		DWORD_PTR affinity_mask = static_cast<DWORD_PTR>(1) << core_id;

		if (SetThreadAffinityMask(thr.handle, affinity_mask) == 0) {
			return ThreadResult::Error;
		}
		return ThreadResult::Success;
	}

	ThreadResult thread_set_name(Thread& thr, const char* name) {
		if (!name) {
			return ThreadResult::Error;
		}

		i32 size_needed = MultiByteToWideChar(CP_UTF8, 0, name, -1, nullptr, 0);
		if (size_needed <= 0) {
			return ThreadResult::Error;
		}

		wchar_t* wide_name = new wchar_t[size_needed];
		if (!wide_name) {
			return ThreadResult::NoMem;
		}

		if (MultiByteToWideChar(CP_UTF8, 0, name, -1, wide_name, size_needed) == 0) {
			delete[] wide_name;
			return ThreadResult::Error;
		}

		HRESULT hr = SetThreadDescription(thr.handle, wide_name);
		delete[] wide_name;

		return SUCCEEDED(hr) ? ThreadResult::Success : ThreadResult::Error;
	}

	i32 thread_get_cpu_topology(CpuInfo* cpu_info, i32 max_cpus) {
		if (!cpu_info || max_cpus <= 0) {
			return -1;
		}

		using GetLogicalProcessorInformationFunc = BOOL(WINAPI*)(PSYSTEM_LOGICAL_PROCESSOR_INFORMATION, PDWORD);

		HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
		if (!kernel32) {
			return -1;
		}

		GetLogicalProcessorInformationFunc GetLogicalProcessorInfo =
			reinterpret_cast<GetLogicalProcessorInformationFunc>(GetProcAddress(kernel32, "GetLogicalProcessorInformation"));

		if (!GetLogicalProcessorInfo) {
			return -1;
		}

		DWORD buffer_size = 0;
		GetLogicalProcessorInfo(nullptr, &buffer_size);

		if (buffer_size == 0) {
			return -1;
		}

		PSYSTEM_LOGICAL_PROCESSOR_INFORMATION buffer =
			static_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION>(malloc(buffer_size));

		if (!buffer) {
			return -1;
		}

		if (!GetLogicalProcessorInfo(buffer, &buffer_size)) {
			free(buffer);
			return -1;
		}

		DWORD count = buffer_size / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
		i32 cpu_count = 0;
		i32 physical_core = 0;

		for (DWORD i = 0; i < count && cpu_count < max_cpus; i++) {
			if (buffer[i].Relationship == RelationProcessorCore) {
				ULONG_PTR mask = buffer[i].ProcessorMask;

				for (i32 bit = 0; bit < 64 && cpu_count < max_cpus; bit++) {
					if (mask & (static_cast<ULONG_PTR>(1) << bit)) {
						cpu_info[cpu_count].logical_id = cpu_count;
						cpu_info[cpu_count].physical_id = 0;
						cpu_info[cpu_count].core_id = physical_core;
						cpu_count++;
					}
				}
				physical_core++;
			}
		}

		free(buffer);
		return cpu_count;
	}

	i32 thread_get_physical_core_count(CpuInfo* cpu_info, i32 count) {
		if (!cpu_info || count < 0) {
			return -1;
		}

		return cpu_info[count - 1].core_id + 1;
	}

	i32 thread_get_logical_core_count(CpuInfo* cpu_info, i32 count) {
		if (!cpu_info || count < 0) {
			return -1;
		}

		return cpu_info[count - 1].logical_id + 1;
	}
}

#elif EDGE_PLATFORM_POSIX
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>

namespace edge {
	namespace detail {
		struct ThreadStartInfo {
			ThreadFunc func;
			void* arg;
		};

		static void* thread_start_wrapper(void* arg) {
			ThreadStartInfo* info = static_cast<ThreadStartInfo*>(arg);
			ThreadFunc func = info->func;
			void* user_arg = info->arg;
			delete info;

			i32 result = func(user_arg);
			return reinterpret_cast<void*>(static_cast<intptr_t>(result));
		}
	}

	ThreadResult thread_create(Thread* thr, ThreadFunc func, void* arg) {
		if (!thr || !func) {
			return ThreadResult::Error;
		}

		detail::ThreadStartInfo* info = new detail::ThreadStartInfo{ func, arg };

		i32 result = pthread_create(&thr->handle, nullptr, detail::thread_start_wrapper, info);
		if (result != 0) {
			delete info;
			return result == ENOMEM ? ThreadResult::NoMem : ThreadResult::Error;
		}
		return ThreadResult::Success;
	}

	ThreadResult thread_join(Thread& thr, i32* res) {
		void* result_ptr;
		i32 join_result = pthread_join(thr.handle, &result_ptr);
		if (join_result != 0) {
			return ThreadResult::Error;
		}

		if (res) {
			*res = static_cast<i32>(reinterpret_cast<intptr_t>(result_ptr));
		}

		return ThreadResult::Success;
	}

	ThreadResult thread_detach(Thread& thr) {
		i32 result = pthread_detach(thr.handle);
		return result == 0 ? ThreadResult::Success : ThreadResult::Error;
	}

	Thread thread_current() {
		Thread thr;
		thr.handle = pthread_self();
		return thr;
	}

	u32 thread_current_id() {
		Thread thrd = thread_current();
		return static_cast<u32>(thrd.handle);
	}

	bool thread_equal(const Thread& lhs, const Thread& rhs) {
		return pthread_equal(lhs.handle, rhs.handle) != 0;
	}

	void thread_exit(i32 res) noexcept {
		pthread_exit(reinterpret_cast<void*>(static_cast<intptr_t>(res)));
	}

	void thread_yield() {
		sched_yield();
	}

	i32 thread_sleep(const std::chrono::nanoseconds& duration) {
		auto secs = std::chrono::duration_cast<std::chrono::seconds>(duration);
		auto nsecs = duration - std::chrono::duration_cast<std::chrono::nanoseconds>(secs);

		timespec req;
		req.tv_sec = secs.count();
		req.tv_nsec = nsecs.count();

		timespec rem;
		return nanosleep(&req, &rem);
	}

	static long futex_syscall(std::atomic<u32>* addr, i32 op, u32 val, const timespec* timeout = nullptr) {
		return syscall(SYS_futex, addr, op, val, timeout, nullptr, 0);
	}

	FutexResult futex_wait(std::atomic<u32>* addr, u32 expected_value, std::chrono::nanoseconds timeout) {
		if (!addr) {
			return FutexResult::Error;
		}

		timespec ts;
		timespec* ts_ptr = nullptr;

		if (timeout != std::chrono::nanoseconds::zero()) {
			auto secs = std::chrono::duration_cast<std::chrono::seconds>(timeout);
			auto nsecs = timeout - std::chrono::duration_cast<std::chrono::nanoseconds>(secs);

			ts.tv_sec = secs.count();
			ts.tv_nsec = nsecs.count();
			ts_ptr = &ts;
		}

		long result = futex_syscall(addr, FUTEX_WAIT_PRIVATE, expected_value, ts_ptr);

		if (result == 0) {
			return FutexResult::Success;
		}

		if (errno == ETIMEDOUT) {
			return FutexResult::TimedOut;
		}

		if (errno == EAGAIN) {
			return FutexResult::Success;
		}

		return FutexResult::Error;
	}

	i32 futex_wake(std::atomic<u32>* addr, i32 count) {
		if (!addr) {
			return 0;
		}

		long result = futex_syscall(addr, FUTEX_WAKE_PRIVATE, count);
		return result >= 0 ? static_cast<i32>(result) : 0;
	}

	i32 futex_wake_all(std::atomic<u32>* addr) {
		if (!addr) {
			return 0;
		}

		long result = futex_syscall(addr, FUTEX_WAKE_PRIVATE, INT32_MAX);
		return result >= 0 ? static_cast<i32>(result) : 0;
	}

	ThreadResult mutex_init(Mutex* mtx, MutexType type) {
		if (!mtx) {
			return ThreadResult::Error;
		}

		mtx->type = type;

		pthread_mutexattr_t attr;
		if (pthread_mutexattr_init(&attr) != 0) {
			return ThreadResult::Error;
		}

		if (type == MutexType::Recursive) {
			pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
		}

		i32 result = pthread_mutex_init(&mtx->data, &attr);
		pthread_mutexattr_destroy(&attr);

		return result == 0 ? ThreadResult::Success : ThreadResult::Error;
	}

	void mutex_destroy(Mutex* mtx) {
		if (!mtx) {
			return;
		}

		pthread_mutex_destroy(&mtx->data);
	}

	ThreadResult mutex_lock(Mutex* mtx) {
		if (!mtx) {
			return ThreadResult::Error;
		}

		i32 result = pthread_mutex_lock(&mtx->data);
		return result == 0 ? ThreadResult::Success : ThreadResult::Error;
	}

	ThreadResult mutex_trylock(Mutex* mtx) {
		if (!mtx) {
			return ThreadResult::Error;
		}

		i32 result = pthread_mutex_trylock(&mtx->data);
		if (result == 0) {
			return ThreadResult::Success;
		}
		else if (result == EBUSY) {
			return ThreadResult::Busy;
		}
		return ThreadResult::Error;
	}

	ThreadResult mutex_timedlock(Mutex* mtx, const std::chrono::nanoseconds& timeout) {
		if (!mtx) {
			return ThreadResult::Error;
		}

		timespec now;
		clock_gettime(CLOCK_REALTIME, &now);

		auto secs = std::chrono::duration_cast<std::chrono::seconds>(timeout);
		auto nsecs = timeout - std::chrono::duration_cast<std::chrono::nanoseconds>(secs);

		timespec ts;
		ts.tv_sec = now.tv_sec + secs.count();
		ts.tv_nsec = now.tv_nsec + nsecs.count();
		if (ts.tv_nsec >= 1000000000) {
			ts.tv_sec += 1;
			ts.tv_nsec -= 1000000000;
		}

		i32 result = pthread_mutex_timedlock(&mtx->data, &ts);
		if (result == 0) {
			return ThreadResult::Success;
		}
		else if (result == ETIMEDOUT) {
			return ThreadResult::TimedOut;
		}
		return ThreadResult::Error;
	}

	ThreadResult mutex_unlock(Mutex* mtx) {
		if (!mtx) {
			return ThreadResult::Error;
		}

		i32 result = pthread_mutex_unlock(&mtx->data);
		return result == 0 ? ThreadResult::Success : ThreadResult::Error;
	}

	ThreadResult cond_init(ConditionVariable* cnd) {
		if (!cnd) {
			return ThreadResult::Error;
		}

		i32 result = pthread_cond_init(&cnd->data, nullptr);
		return result == 0 ? ThreadResult::Success : ThreadResult::Error;
	}

	void cond_destroy(ConditionVariable* cnd) {
		if (!cnd) {
			return;
		}

		pthread_cond_destroy(&cnd->data);
	}

	ThreadResult cond_signal(ConditionVariable* cnd) {
		if (!cnd) {
			return ThreadResult::Error;
		}

		i32 result = pthread_cond_signal(&cnd->data);
		return result == 0 ? ThreadResult::Success : ThreadResult::Error;
	}

	ThreadResult cond_broadcast(ConditionVariable* cnd) {
		if (!cnd) {
			return ThreadResult::Error;
		}

		i32 result = pthread_cond_broadcast(&cnd->data);
		return result == 0 ? ThreadResult::Success : ThreadResult::Error;
	}

	ThreadResult cond_wait(ConditionVariable* cnd, Mutex* mtx) {
		if (!cnd || !mtx) {
			return ThreadResult::Error;
		}

		i32 result = pthread_cond_wait(&cnd->data, &mtx->data);
		return result == 0 ? ThreadResult::Success : ThreadResult::Error;
	}

	ThreadResult cond_timedwait(ConditionVariable* cnd, Mutex* mtx, const std::chrono::nanoseconds& timeout) {
		if (!cnd || !mtx) {
			return ThreadResult::Error;
		}

		timespec now;
		clock_gettime(CLOCK_REALTIME, &now);

		auto secs = std::chrono::duration_cast<std::chrono::seconds>(timeout);
		auto nsecs = timeout - std::chrono::duration_cast<std::chrono::nanoseconds>(secs);

		timespec ts;
		ts.tv_sec = now.tv_sec + secs.count();
		ts.tv_nsec = now.tv_nsec + nsecs.count();
		if (ts.tv_nsec >= 1000000000) {
			ts.tv_sec += 1;
			ts.tv_nsec -= 1000000000;
		}

		i32 result = pthread_cond_timedwait(&cnd->data, &mtx->data, &ts);
		if (result == 0) {
			return ThreadResult::Success;
		}
		else if (result == ETIMEDOUT) {
			return ThreadResult::TimedOut;
		}
		return ThreadResult::Error;
	}

	void call_once(OnceFlag* flag, void(*func)()) {
		if (!flag || !func) {
			return;
		}

		pthread_once(&flag->state, func);
	}

	ThreadResult thread_set_affinity_platform(Thread& thr, i32 core_id) {
		if (core_id < 0) {
			return ThreadResult::Error;
		}

		cpu_set_t cpuset;
		CPU_ZERO(&cpuset);
		CPU_SET(core_id, &cpuset);

#ifdef __ANDROID__
		pid_t tid = pthread_gettid_np(thr.handle);
		if (sched_setaffinity(tid, sizeof(cpu_set_t), &cpuset) != 0) {
#else
		if (pthread_setaffinity_np(thr.handle, sizeof(cpu_set_t), &cpuset) != 0) {
#endif
			return ThreadResult::Error;
		}
		return ThreadResult::Success;
		}

	ThreadResult thread_set_name(Thread & thr, const char* name) {
		if (!name) {
			return ThreadResult::Error;
		}

		if (pthread_setname_np(thr.handle, name) != 0) {
			return ThreadResult::Error;
		}
		return ThreadResult::Success;
	}

	i32 thread_get_cpu_topology(CpuInfo * cpu_info, i32 max_cpus) {
		if (!cpu_info || max_cpus <= 0) {
			return -1;
		}

		i32 cpu_count = 0;

		for (i32 i = 0; i < max_cpus; i++) {
			char path[256];

			// Check if CPU exists
			snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d", i);
			FILE* test = fopen(path, "r");
			if (!test) {
				break;
			}
			fclose(test);

			cpu_info[cpu_count].logical_id = i;

			// Get physical package ID
			snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/topology/physical_package_id", i);
			FILE* f = fopen(path, "r");
			if (f) {
				fscanf(f, "%d", &cpu_info[cpu_count].physical_id);
				fclose(f);
			}
			else {
				cpu_info[cpu_count].physical_id = 0;
			}

			// Get core ID
			snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/topology/core_id", i);
			f = fopen(path, "r");
			if (f) {
				fscanf(f, "%d", &cpu_info[cpu_count].core_id);
				fclose(f);
			}
			else {
				cpu_info[cpu_count].core_id = i;
			}

			cpu_count++;
		}

		return cpu_count;
	}

	i32 thread_get_physical_core_count(CpuInfo * cpu_info, i32 count) {
		if (!cpu_info || count < 0) {
			return -1;
		}

		return cpu_info[count - 1].core_id + 1;
	}

	i32 thread_get_logical_core_count(CpuInfo * cpu_info, i32 count) {
		if (!cpu_info || count < 0) {
			return -1;
		}

		return cpu_info[count - 1].logical_id + 1;
	}
}
#else
#error "Unsupported platform"
#endif

namespace edge {
	ThreadResult thread_set_affinity_ex(Thread& thr, CpuInfo* cpu_info, i32 cpu_count, i32 core_id, bool prefer_physical) {
		if (core_id < 0) {
			return ThreadResult::Error;
		}

		if (!prefer_physical) {
			return thread_set_affinity_platform(thr, core_id);
		}

		// Find the first logical CPU that corresponds to this physical core
		i32 target_logical_id = -1;
		for (i32 i = 0; i < cpu_count; i++) {
			if (cpu_info[i].core_id == core_id) {
				target_logical_id = cpu_info[i].logical_id;
				break;
			}
		}

		if (target_logical_id < 0) {
			return ThreadResult::Error;
		}

		return thread_set_affinity_platform(thr, target_logical_id);
	}

	ThreadResult thread_set_affinity(Thread& thr, i32 core_id, bool prefer_physical) {
		CpuInfo cpu_info[256];
		i32 cpu_count = thread_get_cpu_topology(cpu_info, 256);
		if (cpu_count <= 0) {
			return ThreadResult::Error;
		}

		return thread_set_affinity_ex(thr, cpu_info, cpu_count, core_id, prefer_physical);
	}
}