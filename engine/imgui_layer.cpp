#include "imgui_layer.h"
#include "runtime/input_system.h"
#include "runtime/runtime.h"

#include <allocator.hpp>
#include <math.hpp>

#include <imgui.h>

#include <utility>

namespace edge {
	constexpr f32 IMGUI_STICK_DEADZONE = 0.15f;
	constexpr f32 IMGUI_TRIGGER_DEADZONE = 0.15f;
	constexpr f32 IMGUI_TRIGGER_THRESHOLD = 0.15f;

	static std::pair<f32, f32> radial_deadzone(f32 x, f32 y, f32 deadzone) {
		f32 magnitude = sqrt(x * x + y * y);
		if (magnitude < deadzone) {
			return { 0.0f, 0.0f };
		}

		f32 scale = (magnitude - deadzone) / (1.0f - deadzone);
		scale = min(scale, 1.0f);

		f32 normalized_x = x / magnitude;
		f32 normalized_y = y / magnitude;

		return { normalized_x * scale, normalized_y * scale };
	}

	static f32 simple_deadzone(f32 value, f32 deadzone) {
		if (value < deadzone) {
			return 0.0f;
		}

		return (value - deadzone) / (1.0f - deadzone);
	}

	static void handle_axis_direction(ImGuiIO& io, ImGuiKey negative_key, ImGuiKey positive_key, f32 value, float threshold) {
		if (value < -threshold) {
			io.AddKeyAnalogEvent(negative_key, true, -value);
		}
		else {
			io.AddKeyAnalogEvent(negative_key, false, 0.0f);
		}

		if (value > threshold) {
			io.AddKeyAnalogEvent(positive_key, true, value);
		}
		else {
			io.AddKeyAnalogEvent(positive_key, false, 0.0f);
		}
	}

	constexpr ImGuiKey translate_key_code(Key code) {
		switch (code)
		{
		case Key::Unknown: return ImGuiKey_None;
		case Key::Space: return ImGuiKey_Space;
		case Key::Apostrophe: return ImGuiKey_Apostrophe;
		case Key::Comma: return ImGuiKey_Comma;
		case Key::Minus: return ImGuiKey_Minus;
		case Key::Period: return ImGuiKey_Period;
		case Key::Slash: return ImGuiKey_Slash;
		case Key::_0: return ImGuiKey_0;
		case Key::_1: return ImGuiKey_1;
		case Key::_2: return ImGuiKey_2;
		case Key::_3: return ImGuiKey_3;
		case Key::_4: return ImGuiKey_4;
		case Key::_5: return ImGuiKey_5;
		case Key::_6: return ImGuiKey_6;
		case Key::_7: return ImGuiKey_7;
		case Key::_8: return ImGuiKey_8;
		case Key::_9: return ImGuiKey_9;
		case Key::Semicolon: return ImGuiKey_Semicolon;
		case Key::Eq: return ImGuiKey_Equal;
		case Key::A: return ImGuiKey_A;
		case Key::B: return ImGuiKey_B;
		case Key::C: return ImGuiKey_C;
		case Key::D: return ImGuiKey_D;
		case Key::E: return ImGuiKey_E;
		case Key::F: return ImGuiKey_F;
		case Key::G: return ImGuiKey_G;
		case Key::H: return ImGuiKey_H;
		case Key::I: return ImGuiKey_I;
		case Key::J: return ImGuiKey_J;
		case Key::K: return ImGuiKey_K;
		case Key::L: return ImGuiKey_L;
		case Key::M: return ImGuiKey_M;
		case Key::N: return ImGuiKey_N;
		case Key::O: return ImGuiKey_O;
		case Key::P: return ImGuiKey_P;
		case Key::Q: return ImGuiKey_Q;
		case Key::R: return ImGuiKey_R;
		case Key::S: return ImGuiKey_S;
		case Key::T: return ImGuiKey_T;
		case Key::U: return ImGuiKey_U;
		case Key::V: return ImGuiKey_V;
		case Key::W: return ImGuiKey_W;
		case Key::X: return ImGuiKey_X;
		case Key::Y: return ImGuiKey_Y;
		case Key::Z: return ImGuiKey_Z;
		case Key::LeftBracket: return ImGuiKey_LeftBracket;
		case Key::Backslash: return ImGuiKey_Backslash;
		case Key::RightBracket: return ImGuiKey_RightBracket;
		case Key::GraveAccent: return ImGuiKey_GraveAccent;
		case Key::Esc: return ImGuiKey_Escape;
		case Key::Enter: return ImGuiKey_Enter;
		case Key::Tab: return ImGuiKey_Tab;
		case Key::Backspace: return ImGuiKey_Backspace;
		case Key::Insert: return ImGuiKey_Insert;
		case Key::Del: return ImGuiKey_Delete;
		case Key::Right: return ImGuiKey_RightArrow;
		case Key::Left: return ImGuiKey_LeftArrow;
		case Key::Down: return ImGuiKey_DownArrow;
		case Key::Up: return ImGuiKey_UpArrow;
		case Key::PageUp: return ImGuiKey_PageUp;
		case Key::PageDown: return ImGuiKey_PageDown;
		case Key::Home: return ImGuiKey_Home;
		case Key::End: return ImGuiKey_End;
		case Key::CapsLock: return ImGuiKey_CapsLock;
		case Key::ScrollLock: return ImGuiKey_ScrollLock;
		case Key::NumLock: return ImGuiKey_NumLock;
		case Key::PrintScreen: return ImGuiKey_PrintScreen;
		case Key::Pause: return ImGuiKey_Pause;
		case Key::F1: return ImGuiKey_F1;
		case Key::F2: return ImGuiKey_F2;
		case Key::F3: return ImGuiKey_F3;
		case Key::F4: return ImGuiKey_F4;
		case Key::F5: return ImGuiKey_F5;
		case Key::F6: return ImGuiKey_F6;
		case Key::F7: return ImGuiKey_F7;
		case Key::F8: return ImGuiKey_F8;
		case Key::F9: return ImGuiKey_F9;
		case Key::F10: return ImGuiKey_F10;
		case Key::F11: return ImGuiKey_F11;
		case Key::F12: return ImGuiKey_F12;
#if 0
		case Key::F13: return ImGuiKey_F13;
		case Key::F14: return ImGuiKey_F14;
		case Key::F15: return ImGuiKey_F15;
		case Key::F16: return ImGuiKey_F16;
		case Key::F17: return ImGuiKey_F17;
		case Key::F18: return ImGuiKey_F18;
		case Key::F19: return ImGuiKey_F19;
		case Key::F20: return ImGuiKey_F20;
		case Key::F21: return ImGuiKey_F21;
		case Key::F22: return ImGuiKey_F22;
		case Key::F23: return ImGuiKey_F23;
		case Key::F24: return ImGuiKey_F24;
#endif
		case Key::Kp0: return ImGuiKey_Keypad0;
		case Key::Kp1: return ImGuiKey_Keypad1;
		case Key::Kp2: return ImGuiKey_Keypad2;
		case Key::Kp3: return ImGuiKey_Keypad3;
		case Key::Kp4: return ImGuiKey_Keypad4;
		case Key::Kp5: return ImGuiKey_Keypad5;
		case Key::Kp6: return ImGuiKey_Keypad6;
		case Key::Kp7: return ImGuiKey_Keypad7;
		case Key::Kp8: return ImGuiKey_Keypad8;
		case Key::Kp9: return ImGuiKey_Keypad9;
		case Key::KpDec: return ImGuiKey_KeypadDecimal;
		case Key::KpDiv: return ImGuiKey_KeypadDivide;
		case Key::KpMul: return ImGuiKey_KeypadMultiply;
		case Key::KpSub: return ImGuiKey_KeypadSubtract;
		case Key::KpAdd: return ImGuiKey_KeypadAdd;
		case Key::KpEnter: return ImGuiKey_KeypadEnter;
		case Key::KpEq: return ImGuiKey_KeypadEqual;
		case Key::LeftShift: return ImGuiKey_LeftShift;
		case Key::LeftControl: return ImGuiKey_LeftCtrl;
		case Key::LeftAlt: return ImGuiKey_LeftAlt;
		case Key::LeftSuper: return ImGuiKey_LeftSuper;
		case Key::RightShift: return ImGuiKey_RightShift;
		case Key::RightControl: return ImGuiKey_RightCtrl;
		case Key::RightAlt: return ImGuiKey_RightAlt;
		case Key::RightSuper: return ImGuiKey_RightSuper;
		case Key::Menu: return ImGuiKey_Menu;
		default: return ImGuiKey_None;
		}
	}

	constexpr ImGuiKey translate_gamepad_button(PadBtn code) {
		switch (code)
		{
		case PadBtn::A: return ImGuiKey_GamepadFaceDown;
		case PadBtn::B: return ImGuiKey_GamepadFaceRight;
		case PadBtn::X: return ImGuiKey_GamepadFaceLeft;
		case PadBtn::Y: return ImGuiKey_GamepadFaceUp;
		case PadBtn::BumperLeft: return ImGuiKey_GamepadL1;
		case PadBtn::BumperRight: return ImGuiKey_GamepadR1;
		case PadBtn::Back: return ImGuiKey_GamepadBack;
		case PadBtn::Start: return ImGuiKey_GamepadStart;
		case PadBtn::Guide: return ImGuiKey_None; // ImGui doesnt have a guide button
		case PadBtn::ThumbLeft: return ImGuiKey_GamepadL3;
		case PadBtn::ThumbRight: return ImGuiKey_GamepadR3;
		case PadBtn::DpadUp: return ImGuiKey_GamepadDpadUp;
		case PadBtn::DpadRight: return ImGuiKey_GamepadDpadRight;
		case PadBtn::DpadDown: return ImGuiKey_GamepadDpadDown;
		case PadBtn::DpadLeft: return ImGuiKey_GamepadDpadLeft;
		default: return ImGuiKey_None;
		}
	}

	constexpr ImGuiMouseButton translate_mouse_code(MouseBtn code) {
		switch (code)
		{
		case MouseBtn::Left: return ImGuiMouseButton_Left;
		case MouseBtn::Right: return ImGuiMouseButton_Right;
		case MouseBtn::Middle: return ImGuiMouseButton_Middle;
		default: return ImGuiMouseButton_COUNT;
		}
	}

	static void SetupImGuiStyle()
	{
		ImGui::StyleColorsDark();
		// TODO: Make style
	}

	struct ImGuiInputListener final : InputSystem::IListener {
		void on_bool_change(NotNull<const InputSystem*> input_system, DeviceType device, usize button, bool cur, bool prev) override {
			ImGuiIO& io = ImGui::GetIO();

			switch (device)
			{
			case edge::DeviceType::Keyboard: {
				auto key = static_cast<Key>(button);
				ImGuiKey imgui_key = translate_key_code(key);
				if (imgui_key != ImGuiKey_None) {
					io.AddKeyEvent(imgui_key, cur);
				}
				break;
			}
			case edge::DeviceType::Mouse: {
				auto btn = static_cast<MouseBtn>(button);
				ImGuiMouseButton imgui_btn = translate_mouse_code(btn);
				if (imgui_btn != ImGuiMouseButton_COUNT) {
					io.AddMouseButtonEvent(imgui_btn, cur);
				}
				break;
			} 
			case edge::DeviceType::Pad0: {
				//io.BackendFlags &= ~ImGuiBackendFlags_HasGamepad;

				auto btn = static_cast<PadBtn>(button);
				ImGuiKey key = translate_gamepad_button(btn);
				if (key != ImGuiKey_None) {
					io.AddKeyEvent(key, cur);
				}

				//io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
				break;
			}
			case edge::DeviceType::Touch: {
				break;
			}
			default:
				break;
			}
		}

		void on_axis_change(NotNull<const InputSystem*> input_system, DeviceType device, usize button, glm::vec3 cur, glm::vec3 prev) override {
			ImGuiIO& io = ImGui::GetIO();

			switch (device)
			{
			case edge::DeviceType::Mouse: {
				switch (static_cast<MouseAxis>(button))
				{
				case edge::MouseAxis::Pos: {
					io.AddMousePosEvent(cur.x, cur.y);
					break;
				}
				case edge::MouseAxis::Scroll: {
					io.AddMouseWheelEvent(cur.x, cur.y);
					break;
				}
				default: break;
				}
				break;
			}
			case edge::DeviceType::Pad0: {
				// TODO: io.AddKeyAnalogEvent(ImGuiKey_GamepadL2, );
				// TODO: io.AddKeyAnalogEvent(ImGuiKey_GamepadL2, );
				break;
			}
			case edge::DeviceType::Touch: {
				break;
			}
			default: break;
			}
		}

		void on_character(NotNull<const InputSystem*> input_system, char32_t codepoint) override {
			ImGuiIO& io = ImGui::GetIO();
			io.AddInputCharacter(codepoint);
		}
	};

	bool ImGuiLayer::create(NotNull<const Allocator*> alloc, ImGuiLayerInitInfo init_info) {
		runtime = init_info.runtime;
		input_system = init_info.input_system;

		ImGui::SetAllocatorFunctions(
			[](size_t size, void* user_data) -> void* {
				const Allocator* allocator = (const Allocator*)user_data;
				return allocator->malloc(size);
			},
			[](void* ptr, void* user_data) -> void {
				const Allocator* allocator = (const Allocator*)user_data;
				allocator->free(ptr);
			}, (void*)alloc.m_ptr);

		if (!ImGui::CreateContext()) {
			return false;
		}

		ImGuiIO& io = ImGui::GetIO();
		IMGUI_CHECKVERSION();
		IM_ASSERT(io.BackendRendererUserData == nullptr && "Already initialized a renderer backend!");

		io.BackendRendererUserData = this;
		io.BackendRendererName = "edge";
		//io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
		io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
		io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
#if EDGE_PLATFORM_ANDROID
		io.ConfigFlags |= ImGuiConfigFlags_IsTouchScreen;
#endif
		io.ConfigDpiScaleFonts = true;

		SetupImGuiStyle();

		f32 scale_factor = init_info.runtime->get_surface_scale_factor();

		io.FontGlobalScale = scale_factor;

		ImGuiStyle& style = ImGui::GetStyle();
		style.ScaleAllSizes(scale_factor);

		i32 width, height;
		init_info.runtime->get_surface_extent(width, height);

		io.DisplaySize.x = (f32)width;
		io.DisplaySize.y = (f32)height;

		ImGuiInputListener* listener = alloc->allocate<ImGuiInputListener>();
		if (!listener) {
			return false;
		}
		input_listener_id = init_info.input_system->add_listener(alloc, listener);

		return true;
	}

	void ImGuiLayer::destroy(NotNull<const Allocator*> alloc) {
		ImGui::EndFrame();
		ImGuiIO& io = ImGui::GetIO();
		io.BackendRendererUserData = nullptr;
		ImGui::DestroyContext();
	}

	void ImGuiLayer::on_frame_begin(f32 dt) {
		ImGuiIO& io = ImGui::GetIO();
		io.DeltaTime = dt;

		// Update extent
		i32 width, height;
		runtime->get_surface_extent(width, height);

		io.DisplaySize.x = (f32)width;
		io.DisplaySize.y = (f32)height;

		io.AddFocusEvent(runtime->is_focused());

		ImGui::NewFrame();
	}

	void ImGuiLayer::on_frame_end() {
		ImGui::Render();
	}
}