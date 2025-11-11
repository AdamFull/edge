#pragma once

#include "event_system.h"
#include "input_map.h"

namespace edge::events {
	enum class EventTag {
		eNone = 0,
		eWindow = 1 << 0,
		eRawInput = 1 << 1
	};

	using EventTags = foundation::Flags<EventTag>;

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
		static constexpr EventTags tag_flags = EventTag::eRawInput;
		KeyboardKeyCode key_code;
		bool state;
		uint64_t window_id;
	};

	struct MousePositionEvent {
		static constexpr EventTags tag_flags = EventTag::eRawInput;
		double x, y;
		uint64_t window_id;
	};

	struct MouseKeyEvent {
		static constexpr EventTags tag_flags = EventTag::eRawInput;
		MouseKeyCode key_code;
		bool state;
		uint64_t window_id;
	};

	struct MouseScrollEvent {
		static constexpr EventTags tag_flags = EventTag::eRawInput;
		double offset_x, offset_y;
		uint64_t window_id;
	};

	struct CharacterInputEvent {
		static constexpr EventTags tag_flags = EventTag::eRawInput;
		uint32_t charcode;
		uint64_t window_id;
	};

	struct GamepadConnectionEvent {
		static constexpr EventTags tag_flags = EventTag::eRawInput;
		int32_t gamepad_id;
        int32_t vendor_id;
        int32_t product_id;
        int32_t device_id;
		bool connected;
		const char* name;
	};

	struct GamepadButtonEvent {
		static constexpr EventTags tag_flags = EventTag::eRawInput;
		int32_t gamepad_id;
		GamepadKeyCode key_code;
		bool state;
	};

	struct GamepadAxisEvent {
		static constexpr EventTags tag_flags = EventTag::eRawInput;
		int32_t gamepad_id;
		float values[3];
		GamepadAxisCode axis_code;
	};

	using Dispatcher = EventDispatcher<
		EventTags,
		WindowShouldCloseEvent,
		WindowSizeChangedEvent,
		WindowFocusChangedEvent,
		KeyEvent,
		MousePositionEvent,
		MouseKeyEvent,
		MouseScrollEvent,
		CharacterInputEvent,
		GamepadConnectionEvent,
		GamepadButtonEvent,
		GamepadAxisEvent>;
}

EDGE_MAKE_ENUM_FLAGS(edge::events::EventTags, edge::events::EventTag);
