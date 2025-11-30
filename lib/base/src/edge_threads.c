#include "edge_platform_detect.h"

#if EDGE_HAS_WINDOWS_API
#include "edge_threads_win.c"
#elif EDGE_PLATFORM_POSIX
#include "edge_threads_posix.c"
#else
#error "Unsupported platform"
#endif

i32 edge_thrd_get_physical_core_count(edge_cpu_info_t* cpu_info, i32 count) {
    if (!cpu_info || count < 0) {
        return -1;
    }

    return cpu_info[count - 1].core_id + 1;
}

i32 edge_thrd_get_logical_core_count(edge_cpu_info_t* cpu_info, i32 count) {
    if (!cpu_info || count < 0) {
        return -1;
    }

    return cpu_info[count - 1].logical_id + 1;
}

i32 edge_thrd_set_affinity_ex(edge_thrd_t thr, edge_cpu_info_t* cpu_info, i32 cpu_count, i32 core_id, bool prefer_physical) {
    if (core_id < 0) {
        return edge_thrd_error;
    }

    if (!prefer_physical) {
        return edge_thrd_set_affinity_platform(thr, core_id);
    }

    /* Find the first logical CPU that corresponds to this physical core */
    i32 target_logical_id = -1;
    for (i32 i = 0; i < cpu_count; i++) {
        if (cpu_info[i].core_id == core_id) {
            target_logical_id = cpu_info[i].logical_id;
            break;
        }
    }

    if (target_logical_id < 0) {
        return edge_thrd_error;
    }

    return edge_thrd_set_affinity_platform(thr, target_logical_id);
}

i32 edge_thrd_set_affinity(edge_thrd_t thr, i32 core_id, bool prefer_physical) {
    edge_cpu_info_t cpu_info[256];
    i32 cpu_count = edge_thrd_get_cpu_topology(cpu_info, 256);
    if (cpu_count <= 0) {
        return edge_thrd_error;
    }

    return edge_thrd_set_affinity_ex(thr, cpu_info, cpu_count, core_id, prefer_physical);
}

