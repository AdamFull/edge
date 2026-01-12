#ifndef EDGE_INPUT_SYSTEM_H
#define EDGE_INPUT_SYSTEM_H

#include <array.hpp>
#include <callable.hpp>
#include <bitarray.hpp>
#include <string.hpp>

#include <glm/glm.hpp>

namespace edge {
#pragma region Keyboard
	enum class Key : u16 {
		Unknown = 0,
		// Printable keys
		Space, Apostrophe, Comma, Minus, Period, Slash,
		_0, _1, _2, _3, _4, _5, _6, _7, _8, _9,
		Semicolon, Eq,
		A, B, C, D, E, F, G, H, I, J, K, L, M,
		N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
		LeftBracket, Backslash, RightBracket, GraveAccent,
		// Function keys
		Esc, Enter, Tab, Backspace, Insert, Del,
		Right, Left, Down, Up,
		PageUp, PageDown, Home, End,
		CapsLock, ScrollLock, NumLock, PrintScreen, Pause,
		F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
		// Keypad
		Kp0, Kp1, Kp2, Kp3, Kp4, Kp5, Kp6, Kp7, Kp8, Kp9,
		KpDec, KpDiv, KpMul, KpSub, KpAdd, KpEnter, KpEq,
		// Modifiers
		LeftShift, LeftControl, LeftAlt, LeftSuper,
		RightShift, RightControl, RightAlt, RightSuper,
		Menu,

		Count
	};

	struct KeyboardState {
		BitArray<static_cast<usize>(Key::Count)> cur = {};
		BitArray<static_cast<usize>(Key::Count)> prev = {};
	};

	struct KeyboardDevice {
		KeyboardState state = {};

		void set_key(Key key, bool pressed) noexcept {
			state.cur.put(static_cast<usize>(key), pressed);
		}

		[[nodiscard]] bool is_down(Key key) const noexcept {
			return state.cur.get(static_cast<usize>(key));
		}

		[[nodiscard]] bool is_up(Key key) const noexcept {
			return !state.cur.get(static_cast<usize>(key));
		}

		[[nodiscard]] bool was_pressed(Key key) const noexcept {
			auto index = static_cast<usize>(key);
			return state.cur.get(index) && !state.prev.get(index);
		}

		[[nodiscard]] bool was_released(Key key) const noexcept {
			auto index = static_cast<usize>(key);
			return !state.cur.get(index) && state.prev.get(index);
		}

		[[nodiscard]] bool is_shift_down() const {
			return state.cur.get(static_cast<usize>(Key::LeftShift)) || state.cur.get(static_cast<usize>(Key::RightShift));
		}

		[[nodiscard]] bool is_ctrl_down() const {
			return state.cur.get(static_cast<usize>(Key::LeftControl)) || state.cur.get(static_cast<usize>(Key::RightControl));
		}

		[[nodiscard]] bool is_alt_down() const {
			return state.cur.get(static_cast<usize>(Key::LeftAlt)) || state.cur.get(static_cast<usize>(Key::RightAlt));
		}

		[[nodiscard]] bool is_super_down() const {
			return state.cur.get(static_cast<usize>(Key::LeftSuper)) || state.cur.get(static_cast<usize>(Key::RightSuper));
		}

		void update() noexcept {
			state.prev = state.cur;
		}

		void clear() noexcept {
			state.cur.clear_all();
			state.prev.clear_all();
		}
	};

#pragma endregion Keyboard

#pragma region Mouse
	enum class MouseBtn : u8 {
		Left, Right, Middle,
		_4, _5, _6, _7, _8,
		Count
	};

	enum class MouseAxis : u8 {
		Pos = 0,
		Scroll,
		Count
	};

	struct MouseState {
		BitArray<static_cast<usize>(MouseBtn::Count)> cur_btn = {};
		BitArray<static_cast<usize>(MouseBtn::Count)> prev_btn = {};

		glm::vec3 cur_axes[static_cast<usize>(MouseAxis::Count)] = {};
		glm::vec3 prev_axes[static_cast<usize>(MouseAxis::Count)] = {};
	};

	struct MouseDevice {
		MouseState state = {};

		void set_btn(MouseBtn btn, bool pressed) noexcept {
			state.cur_btn.put(static_cast<usize>(btn), pressed);
		}

		void set_axis(MouseAxis axis, glm::vec3 value) noexcept {
			state.cur_axes[static_cast<usize>(axis)] = value;
		}

		[[nodiscard]] glm::vec3 get_axis(MouseAxis axis) const noexcept {
			return state.cur_axes[static_cast<usize>(axis)];
		}

		[[nodiscard]] bool is_down(MouseBtn btn) const noexcept {
			return state.cur_btn.get(static_cast<usize>(btn));
		}

		[[nodiscard]] bool is_up(MouseBtn btn) const noexcept {
			return !state.cur_btn.get(static_cast<usize>(btn));
		}

		[[nodiscard]] bool was_pressed(MouseBtn btn) const noexcept {
			auto index = static_cast<usize>(btn);
			return state.cur_btn.get(index) && !state.prev_btn.get(index);
		}

		[[nodiscard]] bool was_released(MouseBtn btn) const noexcept {
			auto index = static_cast<usize>(btn);
			return !state.cur_btn.get(index) && state.prev_btn.get(index);
		}

		void update() noexcept {
			state.prev_btn = state.cur_btn;
			memcpy(state.prev_axes, state.cur_axes, sizeof(state.cur_axes));
			set_axis(MouseAxis::Scroll, {});
		}

		void clear() noexcept {
			state.cur_btn.clear_all();
			state.prev_btn.clear_all();

			memset(state.cur_axes, 0, sizeof(state.cur_axes));
			memset(state.prev_axes, 0, sizeof(state.prev_axes));
		}
	};
#pragma endregion Mouse

#pragma region Gamepad
	constexpr usize MAX_GAMEPADS = 8;

	enum class PadBtn : u8 {
		A = 0, B, X, Y,
		BumperLeft, BumperRight,
		TriggerLeft, TriggerRight,
		Back, Start, Guide,
		ThumbLeft, ThumbRight,
		DpadUp, DpadRight, DpadDown, DpadLeft,
		Count
	};

	enum class PadAxis : u8 {
		StickLeft = 0,
		StickRight,
		TriggerLeft, 
		TriggerRight,
		Count
	};

	struct PadState {
		BitArray<static_cast<usize>(PadBtn::Count)> cur_btn = {};
		BitArray<static_cast<usize>(PadBtn::Count)> prev_btn = {};

		glm::vec3 cur_axes[static_cast<usize>(PadAxis::Count)] = {};
		glm::vec3 prev_axes[static_cast<usize>(PadAxis::Count)] = {};
	};

	struct PadDevice {
		PadState state = {};

		bool connected = false;
		char name[128] = {};
		i32 vendor_id = 0;
		i32 product_id = 0;

		f32 stick_deadzone = 0.15f;
		f32 trigger_deadzone = 0.0f;

		void set_btn(PadBtn btn, bool pressed) {
			state.cur_btn.put(static_cast<usize>(btn), pressed);
		}

		void set_axis(PadAxis axis, glm::vec3 value) {
			state.cur_axes[static_cast<usize>(axis)] = value;
		}

		[[nodiscard]] bool is_down(PadBtn btn) const noexcept {
			return state.cur_btn.get(static_cast<usize>(btn));
		}

		[[nodiscard]] bool is_up(PadBtn btn) const noexcept {
			return !state.cur_btn.get(static_cast<usize>(btn));
		}

		[[nodiscard]] bool was_pressed(PadBtn btn) const noexcept {
			auto index = static_cast<usize>(btn);
			return state.cur_btn.get(index) && !state.prev_btn.get(index);
		}

		[[nodiscard]] bool was_released(PadBtn btn) const noexcept {
			auto index = static_cast<usize>(btn);
			return !state.cur_btn.get(index) && state.prev_btn.get(index);
		}

		[[nodiscard]] glm::vec3 get_axis(PadAxis axis) const noexcept {
			return state.cur_axes[static_cast<usize>(axis)];
		}

		void update() noexcept {
			state.prev_btn = state.cur_btn;
			memcpy(state.prev_axes, state.cur_axes, sizeof(state.cur_axes));
		}

		void clear() noexcept {
			state.cur_btn.clear_all();
			state.prev_btn.clear_all();

			memset(state.cur_axes, 0, sizeof(state.cur_axes));
			memset(state.prev_axes, 0, sizeof(state.prev_axes));
		}
	};
#pragma endregion Gamepad

#pragma region Touch
	using TouchId = i32;
	constexpr TouchId INVALID_TOUCH_ID = -1;
	constexpr usize MAX_TOUCHES = 10;

	enum class TouchPhase : u8 {
		Began, Moved, Stationary, Ended, Cancelled
	};

	struct TouchPoint {
		TouchId id = INVALID_TOUCH_ID;
		TouchPhase phase = TouchPhase::Ended;

		f32 x = 0.0f, y = 0.0f;
		f32 x_prev = 0.0f, y_prev = 0.0f;
		f32 x_start = 0.0f, y_start = 0.0f;

		f32 pressure = 1.0f;
		f32 radius = 0.0f;

		f64 timestamp = 0.0;
		f64 last_update = 0.0;

		bool active = false;

		[[nodiscard]] f32 get_delta_x() const noexcept { return x - x_prev; }
		[[nodiscard]] f32 get_delta_y() const noexcept { return y - y_prev; }

		[[nodiscard]] f32 get_distance_from_start() const noexcept {
			f32 dx = x - x_start;
			f32 dy = y - y_start;
			return sqrt(dx * dx + dy * dy);
		}

		[[nodiscard]] f32 get_duration() const noexcept {
			return static_cast<f32>(last_update - timestamp);
		}
	};

	struct TouchState {
		TouchPoint touches[MAX_TOUCHES] = {};
		bool enabled = true;

		TouchPoint* find_or_create(TouchId id) noexcept {
			for (auto & touch : touches) {
				if (touch.active && touch.id == id) {
					return &touch;
				}
			}

			for (auto & touch : touches) {
				if (!touch.active) {
                    touch.id = id;
                    touch.active = true;
					return &touch;
				}
			}

			return nullptr;
		}

        [[nodiscard]] TouchPoint* find(TouchId id) noexcept {
			for (auto & touch : touches) {
				if (touch.active && touch.id == id) {
					return &touch;
				}
			}
			return nullptr;
		}

		[[nodiscard]] const TouchPoint* find(TouchId id) const noexcept {
			for (const auto & touch : touches) {
				if (touch.active && touch.id == id) {
					return &touch;
				}
			}
			return nullptr;
		}

		[[nodiscard]] usize get_count() const noexcept {
			usize count = 0;
			for (const auto & touch : touches) {
				if (touch.active) {
					count++;
				}
			}
			return count;
		}

		void update() noexcept {
			for (auto & touch : touches) {
					if (touch.active && touch.phase != TouchPhase::Ended &&
					touch.phase != TouchPhase::Cancelled) {
					if (touch.x == touch.x_prev && touch.y == touch.y_prev) {
						touch.phase = TouchPhase::Stationary;
					}
				}
			}

			for (auto & touch : touches) {
					if (touch.active && (touch.phase == TouchPhase::Ended ||
					touch.phase == TouchPhase::Cancelled)) {
					touch.active = false;
					touch.id = INVALID_TOUCH_ID;
				}
			}
		}

		void clear() noexcept {
			for (auto & touch : touches) {
                touch.active = false;
                touch.id = INVALID_TOUCH_ID;
			}
		}
	};

	struct TouchDevice {
		TouchState state;

		void update_touch(TouchId id, TouchPhase phase, f32 x, f32 y,
			f32 pressure = 1.0f, f32 radius = 0.0f, f64 timestamp = 0.0) noexcept {

			TouchPoint* touch = state.find_or_create(id);
			if (!touch) {
				// TODO: Handle this case
				return;
			}

			touch->phase = phase;
			touch->x_prev = touch->x;
			touch->y_prev = touch->y;
			touch->x = x;
			touch->y = y;
			touch->pressure = pressure;
			touch->radius = radius;
			touch->last_update = timestamp;

			if (phase == TouchPhase::Began) {
				touch->x_start = x;
				touch->y_start = y;
				touch->timestamp = timestamp;
			}
		}

		[[nodiscard]] const TouchPoint* get_touch(TouchId id) const noexcept {
			return state.find(id);
		}

		[[nodiscard]] const TouchPoint* get_touch_at(usize index) const noexcept {
			if (index >= MAX_TOUCHES) return nullptr;
			return state.touches[index].active ? &state.touches[index] : nullptr;
		}

		[[nodiscard]] usize get_touch_count() const noexcept {
			return state.get_count();
		}

		void update() noexcept {
			if (state.enabled) {
				state.update();
			}
		}

		void clear() {
			state.clear();
		}
	};
#pragma endregion Touch

	enum class DeviceType {
		Keyboard,
		Mouse,
		Pad0,
		Pad1,
		Pad2,
		Pad3,
		Pad4,
		Pad5,
		Pad6,
		Pad7,
		Touch
	};

	struct InputSystem {
		struct IListener {
            virtual ~IListener() = default;

			virtual void on_bool_change(NotNull<const InputSystem*> input_system, DeviceType device, usize button, bool cur, bool prev) noexcept = 0;
			virtual void on_axis_change(NotNull<const InputSystem*> input_system, DeviceType device, usize button, glm::vec3 cur, glm::vec3 prev) noexcept = 0;
			virtual void on_character(NotNull<const InputSystem*> input_system, char32_t codepoint) noexcept = 0;
		};

		Array<std::pair<u64, IListener*>> listeners = {};
		u64 next_listener_id = 1;

		KeyboardDevice keyboard = {};
		MouseDevice mouse = {};
		PadDevice gamepads[MAX_GAMEPADS] = {};
		TouchDevice touch = {};

		bool create(NotNull<const Allocator*> alloc) noexcept;
		void destroy(NotNull<const Allocator*> alloc) noexcept;

		u64 add_listener(NotNull<const Allocator*> alloc, IListener* listener) noexcept;
		void remove_listener(NotNull<const Allocator*> alloc, u64 listener_id) noexcept;

		void update(f64 current_time = 0.0) noexcept;
		void clear() noexcept;

		void dispatch(DeviceType type, usize button, bool cur, bool prev) const noexcept;
		void dispatch(DeviceType type, usize button, glm::vec3 cur, glm::vec3 prev) const noexcept;

		KeyboardDevice* get_keyboard() { return &keyboard; }
		[[nodiscard]] const KeyboardDevice* get_keyboard() const { return &keyboard; }

		MouseDevice* get_mouse() { return &mouse; }
		[[nodiscard]] const MouseDevice* get_mouse() const { return &mouse; }

		PadDevice* get_gamepad(usize index) {
			if (index >= MAX_GAMEPADS) return nullptr;
			return &gamepads[index];
		}

		[[nodiscard]] const PadDevice* get_gamepad(usize index) const {
			if (index >= MAX_GAMEPADS) return nullptr;
			return &gamepads[index];
		}

		TouchDevice* get_touch() { return &touch; }
		[[nodiscard]] const TouchDevice* get_touch() const { return &touch; }
	};
}

#endif // EDGE_INPUT_SYSTEM_H