#include "event_dispatcher.h"

#include <array.hpp>

namespace edge {
	struct EventListener {
		u64 id;
		u64 listen_categories;
		EventListenerFn listener_fn;
	};

	struct EventDispatcher {
		const Allocator* allocator;
		Array<EventListener> listeners;
		u64 next_listener_id;
	};

	EventDispatcher* event_dispatcher_create(const Allocator* alloc) {
		if (!alloc) {
			return nullptr;
		}

		EventDispatcher* dispatcher = allocate<EventDispatcher>(alloc);
		if (!dispatcher) {
			return nullptr;
		}

		if (!dispatcher->listeners.reserve(alloc, 16)) {
			deallocate(alloc, dispatcher);
			return nullptr;
		}

		dispatcher->allocator = alloc;
		dispatcher->next_listener_id = 1;

		return dispatcher;
	}

	void event_dispatcher_destroy(EventDispatcher* dispatcher) {
		if (!dispatcher) {
			return;
		}

		dispatcher->listeners.destroy(dispatcher->allocator);
		deallocate(dispatcher->allocator, dispatcher);
	}

	u64 event_dispatcher_add_listener(EventDispatcher* dispatcher, u64 listen_categories, EventListenerFn listener_fn) {
		if (!dispatcher || !callable_is_valid(&listener_fn)) {
			return 0;
		}

		EventListener listener;
		listener.id = dispatcher->next_listener_id++;
		listener.listen_categories = listen_categories;
		listener.listener_fn = listener_fn;

		if (!dispatcher->listeners.push_back(dispatcher->allocator, listener)) {
			return 0;
		}

		return listener.id;
	}

	void event_dispatcher_remove_listener(EventDispatcher* dispatcher, u64 listener_id) {
		if (!dispatcher || listener_id == 0) {
			return;
		}

		for (usize i = 0; i < dispatcher->listeners.m_size; i++) {
			EventListener* listener = dispatcher->listeners.get(i);
			if (listener && listener->id == listener_id) {
				EventListener out_elem;
				dispatcher->listeners.remove(i, &out_elem);
				return;
			}
		}
	}

	void event_dispatcher_dispatch(EventDispatcher* dispatcher, EventHeader* event) {
		if (!dispatcher || !event) {
			return;
		}

		for (usize i = 0; i < dispatcher->listeners.m_size; i++) {
			EventListener* listener = dispatcher->listeners.get(i);
			if (!listener) {
				continue;
			}

			/* Check if any of the event's category flags match the listener's categories */
			if ((event->categories & listener->listen_categories) != 0) {
				callable_invoke(&listener->listener_fn, event);
			}
		}
	}
}