#include "event_dispatcher.h"

#include <array.hpp>

namespace edge {
	void EventListener::destroy(NotNull<const Allocator*> alloc) noexcept {
		listener_fn.destroy(alloc);
	}

	bool EventDispatcher::create(NotNull<const Allocator*> alloc) noexcept {
		if (!listeners.reserve(alloc, 16)) {
			return false;
		}
		return true;
	}

	void EventDispatcher::destroy(NotNull<const Allocator*> alloc) noexcept {
		for (EventListener& listener : listeners) {
			listener.destroy(alloc);
		}
		listeners.destroy(alloc);
	}

	void EventDispatcher::remove_listener(NotNull<const Allocator*> alloc, u64 listener_id) noexcept {
		assert(listener_id != 0 && "Listener is invalid.");

		for (usize i = 0; i < listeners.size(); i++) {
			EventListener& listener = listeners[i];
			if (listener.id == listener_id) {
				EventListener out_elem;
				listeners.remove(i, &out_elem);
				out_elem.listener_fn.destroy(alloc);
				return;
			}
		}
	}

	void EventDispatcher::dispatch(EventHeader* event) const noexcept {
		assert(event && "Event should be valid pointer.");

		for (auto& listener : listeners) {
			/* Check if any of the event's category flags match the listener's categories */
			if ((event->categories & listener.categories) != 0) {
				listener.listener_fn.invoke(event);
			}
		}
	}
}