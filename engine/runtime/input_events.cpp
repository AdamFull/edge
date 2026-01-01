#include "input_events.h"

#include <math.h>

namespace edge {
	void input_update_keyboard_state(InputState* state, EventDispatcher* dispatcher, InputKeyboardKey key, InputKeyAction new_state) {
		if (!state || !dispatcher) {
			return;
		}
		
		InputKeyAction current_state = (InputKeyAction)state->btn_states.get((usize)key);
		if (current_state == new_state) {
			return;
		}

		InputKeyboardEvent evt;
		evt.header.categories = INPUT_EVENT_MASK;
		evt.header.type = (u64)InputEventType::Keyboard;
		evt.key = key;
		evt.action = new_state;

		dispatcher->dispatch((EventHeader*)&evt);

		state->btn_states.put((usize)key, (bool)new_state);
	}

	void input_update_mouse_move_state(InputState* state, EventDispatcher* dispatcher, f32 x, f32 y) {
		if (!state || !dispatcher) {
			return;
		}

		if (state->mouse.x == x && state->mouse.y == y) {
			return;
		}

		InputMouseMoveEvent evt = {
			.header = {
				.categories = INPUT_EVENT_MASK,
				.type = (u64)InputEventType::MouseMove
			},
			.x = x, 
			.y = y,
			.dx = x - state->mouse.x,
			.dy = y - state->mouse.y
		};

		dispatcher->dispatch((EventHeader*)&evt);

		state->mouse.x = x;
		state->mouse.y = y;
	}

	void input_update_mouse_btn_state(InputState* state, EventDispatcher* dispatcher, InputMouseBtn btn, InputKeyAction new_state) {
		if (!state || !dispatcher) {
			return;
		}

		InputKeyAction current_state = (InputKeyAction)state->mouse.btn_states.get((usize)btn);
		if (current_state == new_state) {
			return;
		}

		InputMouseBtnEvent evt;
		evt.header.categories = INPUT_EVENT_MASK;
		evt.header.type = (u64)InputEventType::MouseBtn;
		evt.btn = btn;
		evt.action = new_state;
		dispatcher->dispatch((EventHeader*)&evt);

		state->mouse.btn_states.put((usize)btn, (bool)new_state);
	}

	void input_update_pad_btn_state(InputState* state, EventDispatcher* dispatcher, i32 pad_id, InputPadBtn btn, InputKeyAction new_state) {
		if (!state || !dispatcher) {
			return;
		}

		InputKeyAction current_state = (InputKeyAction)state->pads[pad_id].btn_states.get((usize)btn);
		if (current_state == new_state) {
			return;
		}

		InputPadButtonEvent evt;
		evt.header.categories = INPUT_EVENT_MASK;
		evt.header.type = (u64)InputEventType::PadButton;
		evt.pad_id = pad_id;
		evt.btn = btn;
		evt.state = new_state;
		dispatcher->dispatch((EventHeader*)&evt);

		state->pads[pad_id].btn_states.put((usize)btn, (bool)new_state);
	}

	void input_update_pad_axis_state(InputState* state, EventDispatcher* dispatcher, i32 pad_id, InputPadAxis axis, f32 x, f32 y, f32 z) {
		if (!state || !dispatcher) {
			return;
		}

		InputPadAxisEvent evt;
		evt.header.categories = INPUT_EVENT_MASK;
		evt.header.type = (u64)InputEventType::PadAxis;
		evt.pad_id = pad_id;
		evt.axis = axis;

		constexpr f32 axis_threshold = 0.01f;
		
		switch (axis)
		{
		case InputPadAxis::StickLeft: {
			f32 x_diff = fabs(x - state->pads[pad_id].stick_left_x);
			f32 y_diff = fabs(y - state->pads[pad_id].stick_left_y);

			if (x_diff > axis_threshold || y_diff > axis_threshold) {
				evt.x = x;
				evt.y = y;
				dispatcher->dispatch((EventHeader*)&evt);

				state->pads[pad_id].stick_left_x = x;
				state->pads[pad_id].stick_left_y = y;
			}
			break;
		}
		case InputPadAxis::StickRight: {
			f32 x_diff = fabs(x - state->pads[pad_id].stick_right_x);
			f32 y_diff = fabs(y - state->pads[pad_id].stick_right_y);

			if (x_diff > axis_threshold || y_diff > axis_threshold) {
				evt.x = x;
				evt.y = y;
				dispatcher->dispatch((EventHeader*)&evt);

				state->pads[pad_id].stick_right_x = x;
				state->pads[pad_id].stick_right_y = y;
			}

			break;
		}
		case InputPadAxis::TriggerLeft: {
			f32 diff = fabs(x - state->pads[pad_id].trigger_left);
			if (diff > axis_threshold) {
				evt.x = x;
				dispatcher->dispatch((EventHeader*)&evt);

				state->pads[pad_id].trigger_left = x;
			}
			break;
		}
		case InputPadAxis::TriggerRight: {
			f32 diff = fabs(x - state->pads[pad_id].trigger_right);
			if (diff > axis_threshold) {
				evt.x = x;
				dispatcher->dispatch((EventHeader*)&evt);

				state->pads[pad_id].trigger_right = x;
			}
			break;
		}
		case InputPadAxis::Accel: {
			f32 x_diff = fabs(x - state->pads[pad_id].accel_x);
			f32 y_diff = fabs(y - state->pads[pad_id].accel_y);
			f32 z_diff = fabs(z - state->pads[pad_id].accel_z);

			if (x_diff > axis_threshold || y_diff > axis_threshold || z_diff > axis_threshold) {
				evt.x = x;
				evt.y = y;
				evt.z = z;
				dispatcher->dispatch((EventHeader*)&evt);

				state->pads[pad_id].accel_x = x;
				state->pads[pad_id].accel_y = y;
				state->pads[pad_id].accel_z = z;
			}
			break;
		}
		case InputPadAxis::Gyro: {
			f32 x_diff = fabs(x - state->pads[pad_id].gyro_x);
			f32 y_diff = fabs(y - state->pads[pad_id].gyro_y);
			f32 z_diff = fabs(z - state->pads[pad_id].gyro_z);

			if (x_diff > axis_threshold || y_diff > axis_threshold || z_diff > axis_threshold) {
				evt.x = x;
				evt.y = y;
				evt.z = z;
				dispatcher->dispatch((EventHeader*)&evt);

				state->pads[pad_id].gyro_x = x;
				state->pads[pad_id].gyro_y = y;
				state->pads[pad_id].gyro_z = z;
			}
			break;
		}
		default:
			break;
		}
	}

	void input_mouse_scroll_event_init(InputMouseScrollEvent* evt, f32 xoffset, f32 yoffset) {
		if (!evt) {
			return;
		}

		evt->header.categories = INPUT_EVENT_MASK;
		evt->header.type = (u64)InputEventType::MouseScroll;
		evt->xoffset = xoffset;
		evt->yoffset = yoffset;
	}

	void input_text_input_event_init(InputTextInputEvent* evt, u32 codepoint) {
		if (!evt) {
			return;
		}

		evt->header.categories = INPUT_EVENT_MASK;
		evt->header.type = (u64)InputEventType::TextInput;
		evt->codepoint = codepoint;
	}

	void input_pad_connection_event_init(InputPadConnectionEvent* evt, i32 pad_id, i32 vendor_id, i32 product_id, i32 device_id, bool connected, const char* name) {
		if (!evt) {
			return;
		}

		evt->header.categories = INPUT_EVENT_MASK;
		evt->header.type = (u64)InputEventType::PadConnection;
		evt->pad_id = pad_id;
		evt->vendor_id = vendor_id;
		evt->product_id = product_id;
		evt->device_id = device_id;
		evt->connected = connected;
		strncpy(evt->name, name, strlen(name));
	}
}