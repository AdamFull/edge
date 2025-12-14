#include "window_events.h"

namespace edge {
	void window_resize_event_init(WindowResizeEvent* evt, i32 width, i32 height) {
		if (!evt) {
			return;
		}

		evt->header.categories = WINDOW_EVENT_MASK;
		evt->header.type = (u64)WindowEventType::Resize;
		evt->width = width;
		evt->height = height;
	}

	void window_focus_event_init(WindowFocusEvent* evt, bool focused) {
		if (!evt) {
			return;
		}

		evt->header.categories = WINDOW_EVENT_MASK;
		evt->header.type = (u64)WindowEventType::Focus;
		evt->focused = focused;
	}

	void window_close_event_init(WindowCloseEvent* evt) {
		if (!evt) {
			return;
		}

		evt->header.categories = WINDOW_EVENT_MASK;
		evt->header.type = (u64)WindowEventType::Close;
	}
}