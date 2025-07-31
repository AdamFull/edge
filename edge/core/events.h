#pragma once

#include "event_system.h"
#include "input_map.h"

namespace edge::events {
	enum class EventTag {
		eNone = 0,
		eWindow = 1 << 0,
		eInput = 1 << 1
	};

	EDGE_MAKE_ENUM_FLAGS(EventTags, EventTag);

	struct WindowShouldCloseEvent {
		static constexpr EventTags tag_flags = EventTag::eWindow;
		uint64_t window_id;
	};

	struct WindowSizeChangedEvent {
		static constexpr EventTags tag_flags = EventTag::eWindow;
		int width, height;
		uint64_t window_id;
	};

	struct WindowFocusChangedEvent {
		static constexpr EventTags tag_flags = EventTag::eWindow;
		bool focused;
		uint64_t window_id;
	};

	struct KeyEvent {
		static constexpr EventTags tag_flags = EventTag::eWindow;
		KeyboardKeyCode key_code;
		uint64_t window_id;
	};

	using Dispatcher = EventDispatcher<
		EventTags,
		WindowShouldCloseEvent,
		WindowSizeChangedEvent,
		WindowFocusChangedEvent,
		KeyEvent>;
}