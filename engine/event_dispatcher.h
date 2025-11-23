#ifndef EDGE_EVENT_DISPATCHER_H
#define EDGE_EVENT_DISPATCHER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

	typedef struct edge_allocator edge_allocator_t;

	typedef struct edge_event_header {
		uint64_t categories; // Contain flags for filtering
		uint64_t type;
	} edge_event_header_t;

	typedef void (*edge_event_listener_fn)(edge_event_header_t* evt, void* user_data);

	typedef struct event_dispatcher event_dispatcher_t;

	event_dispatcher_t* event_dispatcher_create(edge_allocator_t* alloc);
	void event_dispatcher_destroy(event_dispatcher_t* dispatcher);

	uint64_t event_dispatcher_add_listener(event_dispatcher_t* dispatcher, uint64_t listen_categories, edge_event_listener_fn listener_fn, void* user_data);
	void event_dispatcher_remove_listener(event_dispatcher_t* dispatcher, uint64_t listener_id);

	void event_dispatcher_dispatch(event_dispatcher_t* dispatcher, edge_event_header_t* event);

#ifdef __cplusplus
}
#endif

#endif // EDGE_EVENT_DISPATCHER_H