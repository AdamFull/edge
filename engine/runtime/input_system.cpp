#include "input.h"

namespace edge {
	bool InputSystem::create(NotNull<const Allocator*> alloc, usize initial_action_capacity) noexcept {
		return true;
	}

	void InputSystem::destroy(NotNull<const Allocator*> alloc) noexcept {
		
	}

	void InputSystem::update() noexcept {
		previous_state = current_state;
	}

	void InputSystem::set_state(InputKeyboardKey key, bool state) noexcept {
		usize key_idx = (usize)key;
		if (state) {
			current_state.keyboard.key_states.set(key_idx);
		}
		else {
			current_state.keyboard.key_states.clear(key_idx);
		}
	}

	bool InputSystem::get_state(InputKeyboardKey key) const noexcept {
		return current_state.keyboard.key_states.get(static_cast<usize>(key));
	}

	bool InputSystem::get_prev_state(InputKeyboardKey key) const noexcept {
		return previous_state.keyboard.key_states.get(static_cast<usize>(key));
	}

	void InputSystem::set_state(InputMouseBtn key, bool state) noexcept {
		usize btn_idx = static_cast<usize>(key);
		if (state) {
			current_state.mouse.btn_states.set(btn_idx);
		}
		else {
			current_state.mouse.btn_states.clear(btn_idx);
		}
	}

	bool InputSystem::get_state(InputMouseBtn btn) const noexcept {
		return current_state.mouse.btn_states.get(static_cast<usize>(btn));
	}

	bool InputSystem::get_prev_state(InputMouseBtn btn) const noexcept {
		return previous_state.mouse.btn_states.get(static_cast<usize>(btn));
	}

	f32 InputSystem::get_axis(InputMouseAxis axis) const noexcept {
		return current_state.mouse.get_axis(axis);
	}

	f32 InputSystem::get_prev_axis(InputMouseAxis axis) const noexcept {
		return previous_state.mouse.get_axis(axis);
	}

	bool InputSystem::get_state(usize index, InputPadBtn btn) const noexcept {
		assert(index < MAX_PAD_SLOTS && "Gamepad index out of range");
		return current_state.gamepads[index].btn_states.get(static_cast<usize>(btn));
	}

	bool InputSystem::get_prev_state(usize index, InputPadBtn btn) const noexcept {
		assert(index < MAX_PAD_SLOTS && "Gamepad index out of range");
		return previous_state.gamepads[index].btn_states.get(static_cast<usize>(btn));
	}

	const InputPadState* InputSystem::get_pad(usize index) const noexcept {
		assert(index < MAX_PAD_SLOTS && "Gamepad index out of range");
		return &current_state.gamepads[index];
	}

	void InputSystem::enable_text_input() noexcept {
		current_state.text_input_enabled = true;
		current_state.text_pos = 0;
	}

	void InputSystem::add_character(char32_t codepoint) noexcept {
		if (!current_state.text_input_enabled) {
			return;
		}

		if (current_state.text_pos >= 256) {
			return;
		}

		current_state.text_input[current_state.text_pos++] = codepoint;
	}

	void InputSystem::disable_text_input() noexcept {
		current_state.text_input_enabled = false;
	}
}