#ifndef EDGE_UUID_H
#define EDGE_UUID_H

#include "edge_base.h"

#ifdef __cplusplus
extern "C" {
#endif

	typedef struct edge_allocator edge_allocator_t;
	typedef struct edge_rng edge_rng_t;

	typedef struct edge_uuid {
		u8 bytes[16];
	} edge_uuid_t;

	void edge_uuid_v4(edge_rng_t* rng, edge_uuid_t* uuid);

	char* edge_uuid_to_string(const edge_uuid_t* uuid, char* out_str, int* out_size);
	bool edge_uuid_parse(const char* str, edge_uuid_t* out_uuid);

	bool edge_uuid_equals(const edge_uuid_t* a, const edge_uuid_t* b);
	i32 edge_uuid_compare(const edge_uuid_t* a, const edge_uuid_t* b);
	bool edge_uuid_is_nil(const edge_uuid_t* uuid);
	bool edge_uuid_is_valid_v4(const edge_uuid_t* uuid);
	u8 edge_uuid_version(const edge_uuid_t* uuid);

#ifdef __cplusplus
}
#endif

#endif