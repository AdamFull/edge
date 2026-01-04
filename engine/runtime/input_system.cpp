#include "input_system.h"

namespace edge {
	bool InputSystem::create(NotNull<const Allocator*> alloc) noexcept {
		if (!listeners.empty()) {
			return true;
		}

		return listeners.reserve(alloc, 8);
	}

	void InputSystem::destroy(NotNull<const Allocator*> alloc) noexcept {
		for (auto& [id, listener] : listeners) {
			alloc->deallocate(listener);
		}
		listeners.destroy(alloc);
	}

	u64 InputSystem::add_listener(NotNull<const Allocator*> alloc, IListener* listener) noexcept {
		auto id = next_listener_id++;

		if (!listeners.push_back(alloc, std::make_pair(id, listener))) {
			next_listener_id--;
			return 0;
		}

		return id;
	}

	void InputSystem::remove_listener(NotNull<const Allocator*> alloc, u64 listener_id) noexcept {
		assert(listener_id != 0 && "Listener is invalid.");

		for (usize i = 0; i < listeners.size(); i++) {
			auto& listener_pair = listeners[i];
			if (listener_pair.first == listener_id) {
				std::pair<u64, IListener*> out_elem;
				listeners.remove(i, &out_elem);
				alloc->deallocate(out_elem.second);
				return;
			}
		}
	}

	void InputSystem::update(f64 current_time) noexcept {
		// Dispatch keyboard state changes
		{
			// TODO: Speedup bit checks with simd
			for (auto key : Range{ Key::Space, Key::Menu }) {
				auto index = static_cast<usize>(key);
				dispatch(DeviceType::Keyboard, index,
					keyboard.state.cur.get(index),
					keyboard.state.prev.get(index));
			}
			keyboard.update();
		}

		// Dispatch mouse state changes
		{
			// TODO: Speedup bit checks with simd
			for (auto btn : Range{ MouseBtn::Left, MouseBtn::_8 }) {
				auto index = static_cast<usize>(btn);
				dispatch(DeviceType::Mouse, index,
					mouse.state.cur_btn.get(index),
					mouse.state.prev_btn.get(index));
			}

			for (auto axis : Range{ MouseAxis::PosX, MouseAxis::ScrollY }) {
				auto index = static_cast<usize>(axis);
				dispatch(DeviceType::Mouse, index,
					mouse.state.cur_axes[index],
					mouse.state.prev_axes[index]);
			}

			mouse.update();
		}

		// Dispatch gamepad state changes
		for (usize i = 0; i < MAX_GAMEPADS; ++i) {
			auto pad_type = static_cast<DeviceType>(static_cast<usize>(DeviceType::Pad0) + i);
			PadDevice& pad = gamepads[i];

			// TODO: Speedup bit checks with simd
			for (auto btn : Range{ PadBtn::A, PadBtn::DpadLeft }) {
				auto index = static_cast<usize>(btn);
				dispatch(pad_type, index,
					pad.state.cur_btn.get(index),
					pad.state.prev_btn.get(index));
			}

			for (auto axis : Range{ PadAxis::LeftX, PadAxis::TriggerRight }) {
				auto index = static_cast<usize>(axis);
				dispatch(pad_type, index,
					pad.state.cur_axes[index],
					pad.state.prev_axes[index]);
			}

			pad.update();
		}

		// TODO: Dispatch touch state changes
		touch.update();
	}

	void InputSystem::clear() noexcept {
		keyboard.clear();
		mouse.clear();

		for (usize i = 0; i < MAX_GAMEPADS; ++i) {
			gamepads[i].clear();
		}

		touch.clear();
	}

	void InputSystem::dispatch(DeviceType type, usize button, bool cur, bool prev) const noexcept {
		if (cur != prev) {
			for (auto& [id, listener] : listeners) {
				listener->on_bool_change(type, button, cur, prev);
			}
		}
	}

	void InputSystem::dispatch(DeviceType type, usize button, f32 cur, f32 prev) const noexcept {
		if (cur != prev) {
			for (auto& [id, listener] : listeners) {
				listener->on_axis_change(type, button, cur, prev);
			}
		}
	}
}