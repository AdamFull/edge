#ifndef EDGE_ENGINE_H
#define EDGE_ENGINE_H

#include "runtime/platform.h"

#ifdef __cplusplus
extern "C" {
#endif

	typedef struct edge_logger edge_logger_t;

	typedef struct edge_engine_context {
		edge_allocator_t* allocator;
		edge_logger_t* logger;
		edge_platform_layout_t* platform_layout;

		edge_window_t* window;
	} edge_engine_context_t;

#ifdef __cplusplus
}
#endif

#endif //EDGE_ENGINE_H