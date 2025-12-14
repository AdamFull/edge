#ifndef EDGE_INPUT_EVENTS_H
#define EDGE_INPUT_EVENTS_H

#include "input.h"
#include "../event_dispatcher.h"

namespace edge {
	constexpr u64 INPUT_EVENT_MASK = 1 << 0;

	enum class InputEventType {
		Keyboard = 1,
		MouseMove,
		MouseBtn,
		MouseScroll,
		TextInput,
		PadConnection,
		PadButton,
		PadAxis
	};

	struct InputKeyboardEvent {
		EventHeader header;
		InputKeyboardKey key;
		InputKeyAction action;
	};

	struct InputMouseMoveEvent {
		EventHeader header;
		f32 x, y, dx, dy;
	};

	struct InputMouseBtnEvent {
		EventHeader header;
		InputMouseBtn btn;
		InputKeyAction action;
	};

	struct InputMouseScrollEvent {
		EventHeader header;
		f32 xoffset, yoffset;
	};

	struct InputTextInputEvent {
		EventHeader header;
		u32 codepoint;
	};

	struct InputPadConnectionEvent {
		EventHeader header;
		i32 pad_id;
		i32 vendor_id;
		i32 product_id;
		i32 device_id;
		bool connected;
		char name[256];
	};

	struct InputPadButtonEvent {
		EventHeader header;
		i32 pad_id;
		InputPadBtn btn;
		InputKeyAction state;
	};

	struct InputPadAxisEvent {
		EventHeader header;
		i32 pad_id;
		InputPadAxis axis;
		f32 x, y, z;
	};

	void input_update_keyboard_state(InputState* state, EventDispatcher* dispatcher, InputKeyboardKey key, InputKeyAction new_state);
	void input_update_mouse_move_state(InputState* state, EventDispatcher* dispatcher, f32 x, f32 y);
	void input_update_mouse_btn_state(InputState* state, EventDispatcher* dispatcher, InputMouseBtn key, InputKeyAction new_state);
	void input_update_pad_btn_state(InputState* state, EventDispatcher* dispatcher, i32 pad_id, InputPadBtn btn, InputKeyAction new_state);
	void input_update_pad_axis_state(InputState* state, EventDispatcher* dispatcher, i32 pad_id, InputPadAxis axis, f32 x, f32 y, f32 z);

	void input_mouse_scroll_event_init(InputMouseScrollEvent* evt, f32 xoffset, f32 yoffset);
	void input_text_input_event_init(InputTextInputEvent* evt, u32 codepoint);
	void input_pad_connection_event_init(InputPadConnectionEvent* evt, i32 pad_id, i32 vendor_id, i32 product_id, i32 device_id, bool connected, const char* name);
}

#endif // EDGE_INPUT_EVENTS_H