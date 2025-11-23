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
		INPUT_EVENT_TYPE_MOUSE_MOVE,
		INPUT_EVENT_TYPE_MOUSE_BTN,
		INPUT_EVENT_TYPE_MOUSE_SCROLL,
		INPUT_EVENT_TYPE_TEXT_INPUT,
	} input_event_type_t;

	typedef struct input_keyboard_event {
		event_header_t header;
		input_keyboard_key_t key;
		input_key_action_t action;
	} input_keyboard_event_t;

	typedef struct input_mouse_move_event {
		event_header_t header;
		float x, y, dx, dy;
	} input_mouse_move_event_t;

	typedef struct input_mouse_btn_event {
		event_header_t header;
		input_mouse_btn_t btn;
		input_key_action_t action;
	} input_mouse_btn_event_t;

	typedef struct input_mouse_scroll_event {
		event_header_t header;
		float dx, dy;
	} input_mouse_scroll_event_t;

	typedef struct input_text_input_event {
		event_header_t header;
		uint32_t codepoint;
	} input_text_input_event_t;

	void input_update_keyboard_state(input_state_t* state, event_dispatcher_t* dispatcher, input_keyboard_key_t key, input_key_action_t new_state);
	void input_update_mouse_move_state(input_state_t* state, event_dispatcher_t* dispatcher, float x, float y);
	void input_update_mouse_btn_state(input_state_t* state, event_dispatcher_t* dispatcher, input_mouse_btn_t key, input_key_action_t new_state);
	void input_update_mouse_scroll_state(input_state_t* state, event_dispatcher_t* dispatcher, float dx, float dy);

#ifdef __cplusplus
}
#endif

#endif // EDGE_INPUT_EVENTS_H