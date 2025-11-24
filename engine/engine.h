#ifndef EDGE_ENGINE_H
#define EDGE_ENGINE_H

#include "runtime/platform.h"

#ifdef __cplusplus
extern "C" {
#endif

	typedef struct edge_logger edge_logger_t;
	typedef struct edge_sched edge_sched_t;
	typedef struct event_dispatcher event_dispatcher_t;

	typedef struct edge_engine_context {
		edge_allocator_t* allocator;
		edge_logger_t* logger;
		edge_sched_t* sched;

		event_dispatcher_t* event_dispatcher;

		platform_layout_t* platform_layout;
		platform_context_t* platform_context;
	} edge_engine_context_t;

#ifdef __cplusplus
}
#endif

#endif //EDGE_ENGINE_H