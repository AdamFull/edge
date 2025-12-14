#ifndef WINDOW_EVENTS_H
#define WINDOW_EVENTS_H

#include "../event_dispatcher.h"

namespace edge {
	constexpr u64 WINDOW_EVENT_MASK = 1 << 1;

	enum class WindowEventType {
		Resize,
		Focus,
		Close
	};

	struct WindowResizeEvent {
		EventHeader header;
		i32 width;
		i32 height;
	};

	struct WindowFocusEvent {
		EventHeader header;
		bool focused;
	};

	struct WindowCloseEvent {
		EventHeader header;
	};

	void window_resize_event_init(WindowResizeEvent* evt, i32 width, i32 height);
	void window_focus_event_init(WindowFocusEvent* evt, bool focused);
	void window_close_event_init(WindowCloseEvent* evt);
}

#endif