#ifndef EDGE_INPUT_H
#define EDGE_INPUT_H

#include <array.hpp>
#include <bitarray.hpp>
#include <string.hpp>
#include <hashmap.hpp>

namespace edge {
	constexpr usize MAX_PAD_SLOTS = 8;
	constexpr usize MAX_INPUT_ACTIONS = 256;
	constexpr usize MAX_INPUT_BINDINGS_PER_ACTION = 8;

	enum class InputKeyAction {
		Unknown = -1,
		Up,
		Down,
		Repeat
	};

	enum class InputKeyboardKey {
		Unknown = -1,
		Space,
		Apostrophe, /* ' */
		Comma, /* , */
		Minus, /* - */
		Period, /* . */
		Slash, /* / */
		_0, _1, _2, _3, _4, _5, _6, _7, _8, _9,
		Semicolon, /* ; */
		Eq, /* = */
		A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
		LeftBracket, /* [ */
		Backslash, /* \ */
		RightBracket, /* ] */
		GraveAccent, /* ` */

		/* Function keys */
		Esc, Enter, Tab, Backspace, Insert, Del,
		Right, Left, Down, Up,
		PageUp, PageDown, Home, End,
		CapsLock, ScrollLock, NumLock, PrintScreen, Pause,
		F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
		F13, F14, F15, F16, F17, F18, F19, F20, F21, F22, F23, F24, F25,
		Kp0, Kp1, Kp2, Kp3, Kp4, Kp5, Kp6, Kp7, Kp8, Kp9,
		KpDec, KpDiv, KpMul, KpSub, KpAdd, KpEnter, KpEq,
		LeftShift, LeftControl, LeftAlt, LeftSuper,
		RightShift, RightControl, RightAlt, RightSuper,
		Menu,
		Count
	};

	enum class InputMouseBtn {
		Unknown = -1,
		Left, Right, Middle,
		_4, _5, _6, _7, _8,
		Count
	};

	enum class InputMouseAxis {
		Unknown = -1,
		PosX, PosY,
		ScrollX, ScrollY,
		Count
	};

	enum class InputPadBtn {
		Unknown = -1,
		A, B, X, Y,
		Cross = A, Circle = B, Square = X, Triangle = Y,
		BumperLeft, TriggerLeft,
		BumperRight, TriggerRight,
		Back, Start, Guide,
		ThumbLeft, ThumbRight,
		DpadUp, DpadRight, DpadDown, DpadLeft,
		Count
	};

	enum class InputPadAxis {
		Unknown = -1,
		StickLeftX, StickLeftY,
		StickRightX, StickRightY,
		TriggerLeft, TriggerRight,
		AccelX, AccelY, AccelZ,
		GyroX, GyroY, GyroZ,
		Count
	};

	enum class InputDeviceType {
		Keyboard,
		Mouse,
		Gamepad
	};

	EDGE_ALIGN(16) struct InputPadState {
		BitArray<(usize)InputPadBtn::Count> btn_states = {};
		f32 axes[(usize)InputPadAxis::Count] = {};
		
		bool connected = false;
		u16 vendor_id = 0;
		u16 product_id = 0;
		char name[128] = {};

		f32 stick_deadzone = 0.15f;
		f32 trigger_deadzone = 0.1f;

		f32 get_axis(InputPadAxis axis) const noexcept {
			usize idx = static_cast<usize>(axis);
			if (idx >= (usize)InputPadAxis::Count) return 0.0f;
			return axes[idx];
		}

		void set_axis(InputPadAxis axis, f32 value) noexcept {
			usize idx = static_cast<usize>(axis);
			if (idx < (usize)InputPadAxis::Count) {
				axes[idx] = value;
			}
		}
	};

	EDGE_ALIGN(16) struct InputMouseState {
		BitArray<(usize)InputMouseBtn::Count> btn_states = {};
		f32 axes[(usize)InputMouseAxis::Count] = {};

		f32 get_axis(InputMouseAxis axis) const noexcept {
			usize idx = static_cast<usize>(axis);
			if (idx >= (usize)InputMouseAxis::Count) {
				return 0.0f;
			}
			return axes[idx];
		}

		void set_axis(InputMouseAxis axis, f32 value) noexcept {
			usize idx = static_cast<usize>(axis);
			if (idx < (usize)InputMouseAxis::Count) {
				axes[idx] = value;
			}
		}
	};

	EDGE_ALIGN(16) struct InputKeyboardState {
		BitArray<(usize)InputKeyboardKey::Count> key_states = {};
	};

	EDGE_ALIGN(16) struct InputState {
		InputKeyboardState keyboard = {};
		InputMouseState mouse = {};
		InputPadState gamepads[MAX_PAD_SLOTS] = {};

		char32_t text_input[256] = {};
		usize text_pos = 0;
		bool text_input_enabled = false;
	};

	struct InputSystem {
		InputState current_state = {};
		InputState previous_state = {};

		bool create(NotNull<const Allocator*> alloc, usize initial_action_capacity = MAX_INPUT_ACTIONS) noexcept;
		void destroy(NotNull<const Allocator*> alloc) noexcept;

		void update() noexcept;

		void set_state(InputKeyboardKey key, bool state) noexcept;
		bool get_state(InputKeyboardKey key) const noexcept;
		bool get_prev_state(InputKeyboardKey key) const noexcept;

		void set_state(InputMouseBtn key, bool state) noexcept;
		bool get_state(InputMouseBtn btn) const noexcept;
		bool get_prev_state(InputMouseBtn btn) const noexcept;

		f32 get_axis(InputMouseAxis axis) const noexcept;
		f32 get_prev_axis(InputMouseAxis axis) const noexcept;

		bool get_state(usize index, InputPadBtn btn) const noexcept;
		bool get_prev_state(usize index, InputPadBtn btn) const noexcept;

		const InputPadState* get_pad(usize index) const noexcept;

		void enable_text_input() noexcept;
		void add_character(char32_t codepoint) noexcept;
		void disable_text_input() noexcept;
	};
}

#endif // EDGE_INPUT_H