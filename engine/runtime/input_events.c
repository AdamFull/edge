#include "input_events.h"

#include <math.h>

void input_update_keyboard_state(input_state_t* state, event_dispatcher_t* dispatcher, input_keyboard_key_t key, input_key_action_t new_state) {
	if (!state || !dispatcher) {
		return;
	}

	input_key_action_t current_state = (input_key_action_t)edge_bitarray_get(state->key_states, key);
	if (current_state == new_state) {
		return;
	}

	input_keyboard_event_t evt;
	evt.header.categories = EDGE_INPUT_EVENT_MASK;
	evt.header.type = INPUT_EVENT_TYPE_KEYBOARD;
	evt.key = key;
	evt.action = new_state;

	event_dispatcher_dispatch(dispatcher, (event_header_t*)&evt);

	edge_bitarray_put(state->key_states, key, new_state);
}

void input_update_mouse_move_state(input_state_t* state, event_dispatcher_t* dispatcher, float x, float y) {
	if (!state || !dispatcher) {
		return;
	}

	if (state->mouse.x == x && state->mouse.y == y) {
		return;
	}

	input_mouse_move_event_t evt;
	evt.header.categories = EDGE_INPUT_EVENT_MASK;
	evt.header.type = INPUT_EVENT_TYPE_MOUSE_MOVE;
	evt.x = x;
	evt.y = y;
	evt.dx = evt.x - x;
	evt.dy = evt.y - y;

	event_dispatcher_dispatch(dispatcher, (event_header_t*)&evt);

	state->mouse.x = x;
	state->mouse.y = y;
}

void input_update_mouse_btn_state(input_state_t* state, event_dispatcher_t* dispatcher, input_mouse_btn_t btn, input_key_action_t new_state) {
	if (!state || !dispatcher) {
		return;
	}

	input_key_action_t current_state = (input_key_action_t)edge_bitarray_get(state->mouse.btn_states, btn);
	if (current_state == new_state) {
		return;
	}

	input_mouse_btn_event_t evt;
	evt.header.categories = EDGE_INPUT_EVENT_MASK;
	evt.header.type = INPUT_EVENT_TYPE_MOUSE_BTN;
	evt.btn = btn;
	evt.action = new_state;
	event_dispatcher_dispatch(dispatcher, (event_header_t*)&evt);

	edge_bitarray_put(state->mouse.btn_states, btn, new_state);
}

void input_update_pad_btn_state(input_state_t* state, event_dispatcher_t* dispatcher, int32_t pad_id, input_pad_btn_t btn, input_key_action_t new_state) {
	if (!state || !dispatcher) {
		return;
	}

	input_key_action_t current_state = (input_key_action_t)edge_bitarray_get(state->pads->btn_states, btn);
	if (current_state == new_state) {
		return;
	}

	input_pad_button_event_t evt;
	evt.header.categories = EDGE_INPUT_EVENT_MASK;
	evt.header.type = INPUT_EVENT_TYPE_PAD_BUTTON;
	evt.pad_id = pad_id;
	evt.btn = btn;
	evt.state = new_state;
	event_dispatcher_dispatch(dispatcher, (event_header_t*)&evt);

	edge_bitarray_put(state->pads->btn_states, btn, new_state);
}

void input_update_pad_axis_state(input_state_t* state, event_dispatcher_t* dispatcher, int32_t pad_id, input_pad_axis_t axis, float x, float y, float z) {
	if (!state || !dispatcher) {
		return;
	}

	input_pad_axis_event_t evt;
	evt.header.categories = EDGE_INPUT_EVENT_MASK;
	evt.header.type = INPUT_EVENT_TYPE_PAD_AXIS;
	evt.pad_id = pad_id;
	evt.axis = axis;

	const float axis_threshold = 0.01f;

	switch (axis)
	{
	case INPUT_PAD_AXIS_STICK_LEFT: {
		float x_diff = fabs(x - state->pads[pad_id].stick_left_x);
		float y_diff = fabs(y - state->pads[pad_id].stick_left_y);

		if (x_diff > axis_threshold || y_diff > axis_threshold) {
			evt.x = x;
			evt.y = y;
			event_dispatcher_dispatch(dispatcher, (event_header_t*)&evt);

			state->pads[pad_id].stick_left_x = x;
			state->pads[pad_id].stick_left_y = y;
		}
		break;
	}
	case INPUT_PAD_AXIS_STICK_RIGHT: {
		float x_diff = fabs(x - state->pads[pad_id].stick_right_x);
		float y_diff = fabs(y - state->pads[pad_id].stick_right_y);

		if (x_diff > axis_threshold || y_diff > axis_threshold) {
			evt.x = x;
			evt.y = y;
			event_dispatcher_dispatch(dispatcher, (event_header_t*)&evt);

			state->pads[pad_id].stick_right_x = x;
			state->pads[pad_id].stick_right_y = y;
		}

		break;
	}
	case INPUT_PAD_AXIS_TRIGGER_LEFT: {
		float diff = fabs(x - state->pads[pad_id].trigger_left);
		if (diff > axis_threshold) {
			evt.x = x;
			event_dispatcher_dispatch(dispatcher, (event_header_t*)&evt);

			state->pads[pad_id].trigger_left = x;
		}
		break;
	}
	case INPUT_PAD_AXIS_TRIGGER_RIGHT: {
		float diff = fabs(x - state->pads[pad_id].trigger_right);
		if (diff > axis_threshold) {
			evt.x = x;
			event_dispatcher_dispatch(dispatcher, (event_header_t*)&evt);

			state->pads[pad_id].trigger_right = x;
		}
		break;
	}
	case INPUT_PAD_AXIS_ACCEL: {
		float x_diff = fabs(x - state->pads[pad_id].accel_x);
		float y_diff = fabs(y - state->pads[pad_id].accel_y);
		float z_diff = fabs(z - state->pads[pad_id].accel_z);

		if (x_diff > axis_threshold || y_diff > axis_threshold || z_diff > axis_threshold) {
			evt.x = x;
			evt.y = y;
			evt.z = z;
			event_dispatcher_dispatch(dispatcher, (event_header_t*)&evt);

			state->pads[pad_id].accel_x = x;
			state->pads[pad_id].accel_y = y;
			state->pads[pad_id].accel_z = z;
		}
		break;
	}
	case INPUT_PAD_AXIS_GYRO: {
		float x_diff = fabs(x - state->pads[pad_id].gyro_x);
		float y_diff = fabs(y - state->pads[pad_id].gyro_y);
		float z_diff = fabs(z - state->pads[pad_id].gyro_z);

		if (x_diff > axis_threshold || y_diff > axis_threshold || z_diff > axis_threshold) {
			evt.x = x;
			evt.y = y;
			evt.z = z;
			event_dispatcher_dispatch(dispatcher, (event_header_t*)&evt);

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

void input_mouse_scroll_event_init(input_mouse_scroll_event_t* evt, float xoffset, float yoffset) {
	if (!evt) {
		return;
	}

	evt->header.categories = EDGE_INPUT_EVENT_MASK;
	evt->header.type = INPUT_EVENT_TYPE_MOUSE_SCROLL;
	evt->xoffset = xoffset;
	evt->yoffset = yoffset;
}

void input_text_input_event_init(input_text_input_event_t* evt, uint32_t codepoint) {
	if (!evt) {
		return;
	}

	evt->header.categories = EDGE_INPUT_EVENT_MASK;
	evt->header.type = INPUT_EVENT_TYPE_TEXT_INPUT;
	evt->codepoint = codepoint;
}

void input_pad_connection_event_init(input_pad_connection_event_t* evt, int32_t pad_id, int32_t vendor_id, int32_t product_id, int32_t device_id, bool connected, const char* name) {
	if (!evt) {
		return;
	}

	evt->header.categories = EDGE_INPUT_EVENT_MASK;
	evt->header.type = INPUT_EVENT_TYPE_PAD_CONNECTION;
	evt->pad_id = pad_id;
	evt->vendor_id = vendor_id;
	evt->product_id = product_id;
	evt->device_id = device_id;
	evt->connected = connected;
	strncpy(evt->name, name, strlen(name));
}