#ifndef EDGE_EVENT_DISPATCHER_H
#define EDGE_EVENT_DISPATCHER_H

#include <array.hpp>
#include <callable.hpp>

namespace edge {
	struct Allocator;

	struct EventHeader {
		u64 categories = 0;
		u64 type = 0;

		template<typename T>
		auto as() {
			using BaseType = std::remove_pointer_t<std::remove_reference_t<T>>;
			return reinterpret_cast<BaseType*>(this);
		}
	};

	using EventListenerFn = Callable<void(EventHeader* evt)>;

	struct EventListener {
		u64 id = 0;
		u64 categories = 0;
		EventListenerFn listener_fn = {};

		template<typename F>
		bool create(NotNull<const Allocator*> alloc, F&& fn) {
			listener_fn = callable_create_from_lambda(alloc, std::forward<F>(fn));
			return listener_fn.is_valid();
		}

		void destroy(NotNull<const Allocator*> alloc);
	};

	struct EventDispatcher {
		Array<EventListener> listeners = {};
		u64 next_listener_id = 1;

		bool create(NotNull<const Allocator*> alloc);
		void destroy(NotNull<const Allocator*> alloc);

		template<typename F>
		u64 add_listener(NotNull<const Allocator*> alloc, u64 categories, F&& fn) {
			EventListener listener = {};
			if (!listener.create(alloc, std::forward<F>(fn))) {
				return 0;
			}

			listener.id = next_listener_id++;
			listener.categories = categories;

			if (!listeners.push_back(alloc, listener)) {
				return 0;
			}

			return listener.id;
		}

		void remove_listener(NotNull<const Allocator*> alloc, u64 listener_id);
		void dispatch(EventHeader* event) const;
	};
}

#endif // EDGE_EVENT_DISPATCHER_H