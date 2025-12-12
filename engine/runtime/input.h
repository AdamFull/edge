#ifndef EDGE_INPUT_H
#define EDGE_INPUT_H

#include <bitarray.hpp>
#include <platform_detect.hpp>

namespace edge {
	constexpr usize MAX_PAD_SLOTS = 8;

	enum class InputKeyAction {
		Unknown = -1,
		Up,
		Down
	};

	enum class InputKeyboardKey {
		Unknown = -1,
		Space,
		Apostrophe, /* ' */
		Comma, /* , */
		Minus, /* - */
		Period, /* . */
		Slash, /* / */
		_0,
		_1,
		_2,
		_3,
		_4,
		_5,
		_6,
		_7,
		_8,
		_9,
		Semicolon, /* ; */
		Eq, /* = */
		A,
		B,
		C,
		D,
		E,
		F,
		G,
		H,
		I,
		J,
		K,
		L,
		M,
		N,
		O,
		P,
		Q,
		R,
		S,
		T,
		U,
		V,
		W,
		X,
		Y,
		Z,
		LeftBracket, /* [ */
		Backslash, /* \ */
		RightBracket, /* ] */
		GraveAccent, /* ` */

		/* Function keys */
		Esc,
		Enter,
		Tab,
		Backspace,
		Insert,
		Del,
		Right,
		Left,
		Down,
		Up,
		PageUp,
		PageDown,
		Home,
		End,
		CapsLock,
		ScrollLock,
		NumLock,
		PrintScreen,
		Pause,
		F1,
		F2,
		F3,
		F4,
		F5,
		F6,
		F7,
		F8,
		F9,
		F10,
		F11,
		F12,
		F13,
		F14,
		F15,
		F16,
		F17,
		F18,
		F19,
		F20,
		F21,
		F22,
		F23,
		F24,
		F25,
		Kp0,
		Kp1,
		Kp2,
		Kp3,
		Kp4,
		Kp5,
		Kp6,
		Kp7,
		Kp8,
		Kp9,
		KpDec,
		KpDiv,
		KpMul,
		KpSub,
		KpAdd,
		KpEnter,
		KpEq,
		LeftShift,
		LeftControl,
		LeftAlt,
		LeftSuper,
		RightShift,
		RightControl,
		RightAlt,
		RightSuper,
		Menu,

		Count

	};

	enum class InputMouseBtn {
		Unknown = -1,
		Left,
		Right,
		Middle,
		_4,
		_5,
		_6,
		_7,
		_8,
		Count
	};

	enum class InputPadBtn {
		Unknown = -1,
		A,
		B,
		X,
		Y,
		Cross = A,
		Circle = B,
		Square = X,
		Triangle = Y,
		BumperLeft,
		TriggerLeft,
		BumperRight,
		TriggerRight,
		Back,
		Start,
		Guide,
		ThumbLeft,
		ThumbRight,
		DpadUp,
		DpadRight,
		DpadDown,
		DpadLeft,
		Count
	};

	enum class InputPadAxis {
		Unknown = -1,
		StickLeft = 0,
		StickRight,
		TriggerLeft,
		TriggerRight,
		Accel,
		Gyro,
		Count
	};

	EDGE_ALIGN(16) struct InputPadState {
		BitArray<(usize)InputPadBtn::Count> btn_states;
		f32 stick_left_x, stick_left_y;
		f32 stick_right_x, stick_right_y;
		f32 trigger_left, trigger_right;
		f32 accel_x, accel_y, accel_z;
		f32 gyro_x, gyro_y, gyro_z;
	};

	EDGE_ALIGN(16) struct InputMouseState {
		BitArray<(usize)InputMouseBtn::Count> btn_states;
		f32 x, y;
	};

	EDGE_ALIGN(16) struct InputState {
		BitArray<(usize)InputKeyboardKey::Count> btn_states;
		InputMouseState mouse;
		InputPadState pads[MAX_PAD_SLOTS];
	};
}

#endif // EDGE_INPUT_H