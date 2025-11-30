#ifndef EDGE_TESTING_H
#define EDGE_TESTING_H

#include <stdatomic.h>
#include "edge_allocator.h"

#ifdef __cplusplus
extern "C" {
#endif

	edge_allocator_t edge_testing_allocator_create(void);

	size_t edge_testing_net_allocated(void);

#ifdef __cplusplus
}
#endif

#endif
