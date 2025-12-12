#ifndef EDGE_EVENT_DISPATCHER_H
#define EDGE_EVENT_DISPATCHER_H

#include <stddef.hpp>

namespace edge {
	struct Allocator;

	struct EventHeader {
		u64 categories;
		u64 type;
	};

	using EventListenerFn = void (*)(EventHeader* evt, void* user_data);

	struct EventDispatcher;

	EventDispatcher* event_dispatcher_create(const Allocator* alloc);
	void event_dispatcher_destroy(EventDispatcher* dispatcher);

	uint64_t event_dispatcher_add_listener(EventDispatcher* dispatcher, u64 listen_categories, EventListenerFn listener_fn, void* user_data);
	void event_dispatcher_remove_listener(EventDispatcher* dispatcher, u64 listener_id);

	void event_dispatcher_dispatch(EventDispatcher* dispatcher, EventHeader* event);
}

#endif // EDGE_EVENT_DISPATCHER_H