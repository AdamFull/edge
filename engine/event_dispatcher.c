#include "event_dispatcher.h"

#include <edge_allocator.h>
#include <edge_vector.h>

#include <stdint.h>
#include <stdbool.h>

typedef struct edge_event_listener {
	uint64_t id;
	uint64_t listen_categories;
	edge_event_listener_fn listener_fn;
	void* user_data;
} edge_event_listener_t;

struct edge_event_dispatcher {
	edge_vector_t* listeners;
	edge_allocator_t* allocator;
	uint64_t next_listener_id;
};

edge_event_dispatcher_t* event_dispatcher_create(edge_allocator_t* alloc) {
	if (!alloc) {
		return NULL;
	}

	edge_event_dispatcher_t* dispatcher = (edge_event_dispatcher_t*)edge_allocator_malloc(alloc, sizeof(edge_event_dispatcher_t));
	if (!dispatcher) {
		return NULL;
	}

	dispatcher->listeners = edge_vector_create(alloc, sizeof(edge_event_listener_t), 16);
	if (!dispatcher->listeners) {
		edge_allocator_free(alloc, dispatcher);
		return NULL;
	}

	dispatcher->allocator = alloc;
	dispatcher->next_listener_id = 1;

	return dispatcher;
}

void event_dispatcher_destroy(edge_event_dispatcher_t* dispatcher) {
	if (!dispatcher) {
		return;
	}

	if (dispatcher->listeners) {
		edge_vector_destroy(dispatcher->listeners);
	}

	edge_allocator_free(dispatcher->allocator, dispatcher);
}

uint64_t event_dispatcher_add_listener(edge_event_dispatcher_t* dispatcher, uint64_t listen_categories, edge_event_listener_fn listener_fn, void* user_data) {
	if (!dispatcher || !listener_fn) {
		return 0;
	}

	edge_event_listener_t listener;
	listener.id = dispatcher->next_listener_id++;
	listener.listen_categories = listen_categories;
	listener.listener_fn = listener_fn;
	listener.user_data = user_data;

	if (!edge_vector_push_back(dispatcher->listeners, &listener)) {
		return 0;
	}

	return listener.id;
}

void event_dispatcher_remove_listener(edge_event_dispatcher_t* dispatcher, uint64_t listener_id) {
	if (!dispatcher || listener_id == 0) {
		return;
	}

	size_t count = edge_vector_size(dispatcher->listeners);
	for (size_t i = 0; i < count; i++) {
		edge_event_listener_t* listener = (edge_event_listener_t*)edge_vector_at(dispatcher->listeners, i);
		if (listener && listener->id == listener_id) {
			edge_vector_remove(dispatcher->listeners, i, NULL);
			return;
		}
	}
}

void event_dispatcher_dispatch(edge_event_dispatcher_t* dispatcher, edge_event_header_t* event) {
	if (!dispatcher || !event) {
		return;
	}

	size_t count = edge_vector_size(dispatcher->listeners);
	for (size_t i = 0; i < count; i++) {
		edge_event_listener_t* listener = (edge_event_listener_t*)edge_vector_at(dispatcher->listeners, i);
		if (!listener) {
			continue;
		}

		/* Check if any of the event's category flags match the listener's categories */
		if ((event->categories & listener->listen_categories) != 0) {
			listener->listener_fn(event, listener->user_data);
		}
	}
}