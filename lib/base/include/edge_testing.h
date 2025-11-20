#ifndef EDGE_TESTING_H
#define EDGE_TESTING_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdatomic.h>
#include "edge_allocator.h"

	edge_allocator_t edge_testing_allocator_create(void);

	size_t edge_testing_net_allocated(void);

#ifdef __cplusplus
}
#endif

#endif
