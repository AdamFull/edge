#ifndef EDGE_INPUT_EVENTS_H
#define EDGE_INPUT_EVENTS_H

#include "input.h"
#include "../event_dispatcher.h"

#define EDGE_INPUT_EVENT_MASK (1 << 0)
#define EDGE_INPUT_EVENT_START_INDEX 1

#ifdef __cplusplus
extern "C" {
#endif

	typedef enum input_event_type {
		INPUT_EVENT_TYPE_KEYBOARD = EDGE_INPUT_EVENT_START_INDEX,
		INPUT_EVENT_TYPE_MOUSE_MOVE
	} input_event_type_t;

	typedef struct input_keyboard_event {
		edge_event_header_t header;
		input_keyboard_key_t key;
		input_key_action_t action;
	} input_keyboard_event_t;

	typedef struct input_mouse_move_event {
		edge_event_header_t header;
		float x, y, dx, dy;
	} input_mouse_move_event_t;

	inline void input_keyboard_event_init(input_keyboard_event_t* evt, input_keyboard_key_t key, input_key_action_t action) {
		if (!evt) {
			return;
		}

		evt->header.categories = EDGE_INPUT_EVENT_MASK;
		evt->header.type = INPUT_EVENT_TYPE_KEYBOARD;
		evt->key = key;
		evt->action = action;
	}

	inline void input_mouse_move_event_init(input_mouse_move_event_t* evt, float x, float y, float dx, float dy) {
		if (!evt) {
			return;
		}

		evt->header.categories = EDGE_INPUT_EVENT_MASK;
		evt->header.type = INPUT_EVENT_TYPE_MOUSE_MOVE;
		evt->x = x;
		evt->y = y;
		evt->dx = dx;
		evt->dy = dy;
	}

#ifdef __cplusplus
}
#endif

#endif // EDGE_INPUT_EVENTS_H