#include <edge_rng.h>
#include <edge_uuid.h>
#include <edge_testing.h>

#include <assert.h>

int main(void) {
	edge_allocator_t allocator = edge_testing_allocator_create();

	edge_rng_t rng;
	edge_rng_create(EDGE_RNG_XOSHIRO256, 0, &rng);
	edge_rng_seed_entropy_secure(&rng);

	edge_uuid_t uuid;
	edge_uuid_v4(&rng, &uuid);

	char buffer[64];
	int uuid_size = 0;
	char* uuid_str = edge_uuid_to_string(&uuid, buffer, &uuid_size);

	size_t alloc_net = edge_testing_net_allocated();
	assert(alloc_net == 0 && "Memory leaks detected, some data was not freed.");

	return 0;
}