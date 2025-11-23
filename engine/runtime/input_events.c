#include "input_events.h"

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

void input_update_mouse_scroll_state(input_state_t* state, event_dispatcher_t* dispatcher, float dx, float dy) {
	if (!state || !dispatcher) {
		return;
	}

	// TODO: May be need to handle how much scrolled

	input_mouse_scroll_event_t evt;
	evt.header.categories = EDGE_INPUT_EVENT_MASK;
	evt.header.type = INPUT_EVENT_TYPE_MOUSE_SCROLL;
	evt.dx = dx;
	evt.dy = dy;

	event_dispatcher_dispatch(dispatcher, (event_header_t*)&evt);
}