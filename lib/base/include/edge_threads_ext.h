#ifndef EDGE_THREADS_EXTENSIONS_H
#define EDGE_THREADS_EXTENSIONS_H

#include <threads.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

	/**
	 * Set the CPU affinity for a thread (pin it to a specific core)
	 *
	 * @param thr The thread to set affinity for
	 * @param core_id The CPU core number (0-indexed)
	 * @return thrd_success on success, thrd_error on failure
	 */
	int thrd_set_affinity(thrd_t thr, int core_id);

	/**
	 * Set the name of a thread (for debugging/profiling)
	 *
	 * @param thr The thread to name
	 * @param name The name to assign (max 15 chars on Linux, longer on Windows)
	 * @return thrd_success on success, thrd_error on failure
	 */
	int thrd_set_name(thrd_t thr, const char* name);

	/**
	 * Get the number of available CPU cores
	 *
	 * @return Number of CPU cores, or -1 on error
	 */
	int thrd_get_cpu_count(void);

#ifdef __cplusplus
}
#endif

#endif // EDGE_THREADS_EXTENSIONS_H