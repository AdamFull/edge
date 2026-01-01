#include "imgui_layer.h"
#include "../runtime/input_events.h"
#include "../runtime/window_events.h"
#include "../runtime/platform.h"

#include <allocator.hpp>
#include <math.hpp>

#include <imgui.h>

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

	constexpr ImGuiKey translate_key_code(InputKeyboardKey code) {
		switch (code)
		{
		case InputKeyboardKey::Unknown: return ImGuiKey_None;
		case InputKeyboardKey::Space: return ImGuiKey_Space;
		case InputKeyboardKey::Apostrophe: return ImGuiKey_Apostrophe;
		case InputKeyboardKey::Comma: return ImGuiKey_Comma;
		case InputKeyboardKey::Minus: return ImGuiKey_Minus;
		case InputKeyboardKey::Period: return ImGuiKey_Period;
		case InputKeyboardKey::Slash: return ImGuiKey_Slash;
		case InputKeyboardKey::_0: return ImGuiKey_0;
		case InputKeyboardKey::_1: return ImGuiKey_1;
		case InputKeyboardKey::_2: return ImGuiKey_2;
		case InputKeyboardKey::_3: return ImGuiKey_3;
		case InputKeyboardKey::_4: return ImGuiKey_4;
		case InputKeyboardKey::_5: return ImGuiKey_5;
		case InputKeyboardKey::_6: return ImGuiKey_6;
		case InputKeyboardKey::_7: return ImGuiKey_7;
		case InputKeyboardKey::_8: return ImGuiKey_8;
		case InputKeyboardKey::_9: return ImGuiKey_9;
		case InputKeyboardKey::Semicolon: return ImGuiKey_Semicolon;
		case InputKeyboardKey::Eq: return ImGuiKey_Equal;
		case InputKeyboardKey::A: return ImGuiKey_A;
		case InputKeyboardKey::B: return ImGuiKey_B;
		case InputKeyboardKey::C: return ImGuiKey_C;
		case InputKeyboardKey::D: return ImGuiKey_D;
		case InputKeyboardKey::E: return ImGuiKey_E;
		case InputKeyboardKey::F: return ImGuiKey_F;
		case InputKeyboardKey::G: return ImGuiKey_G;
		case InputKeyboardKey::H: return ImGuiKey_H;
		case InputKeyboardKey::I: return ImGuiKey_I;
		case InputKeyboardKey::J: return ImGuiKey_J;
		case InputKeyboardKey::K: return ImGuiKey_K;
		case InputKeyboardKey::L: return ImGuiKey_L;
		case InputKeyboardKey::M: return ImGuiKey_M;
		case InputKeyboardKey::N: return ImGuiKey_N;
		case InputKeyboardKey::O: return ImGuiKey_O;
		case InputKeyboardKey::P: return ImGuiKey_P;
		case InputKeyboardKey::Q: return ImGuiKey_Q;
		case InputKeyboardKey::R: return ImGuiKey_R;
		case InputKeyboardKey::S: return ImGuiKey_S;
		case InputKeyboardKey::T: return ImGuiKey_T;
		case InputKeyboardKey::U: return ImGuiKey_U;
		case InputKeyboardKey::V: return ImGuiKey_V;
		case InputKeyboardKey::W: return ImGuiKey_W;
		case InputKeyboardKey::X: return ImGuiKey_X;
		case InputKeyboardKey::Y: return ImGuiKey_Y;
		case InputKeyboardKey::Z: return ImGuiKey_Z;
		case InputKeyboardKey::LeftBracket: return ImGuiKey_LeftBracket;
		case InputKeyboardKey::Backslash: return ImGuiKey_Backslash;
		case InputKeyboardKey::RightBracket: return ImGuiKey_RightBracket;
		case InputKeyboardKey::GraveAccent: return ImGuiKey_GraveAccent;
		case InputKeyboardKey::Esc: return ImGuiKey_Escape;
		case InputKeyboardKey::Enter: return ImGuiKey_Enter;
		case InputKeyboardKey::Tab: return ImGuiKey_Tab;
		case InputKeyboardKey::Backspace: return ImGuiKey_Backspace;
		case InputKeyboardKey::Insert: return ImGuiKey_Insert;
		case InputKeyboardKey::Del: return ImGuiKey_Delete;
		case InputKeyboardKey::Right: return ImGuiKey_RightArrow;
		case InputKeyboardKey::Left: return ImGuiKey_LeftArrow;
		case InputKeyboardKey::Down: return ImGuiKey_DownArrow;
		case InputKeyboardKey::Up: return ImGuiKey_UpArrow;
		case InputKeyboardKey::PageUp: return ImGuiKey_PageUp;
		case InputKeyboardKey::PageDown: return ImGuiKey_PageDown;
		case InputKeyboardKey::Home: return ImGuiKey_Home;
		case InputKeyboardKey::End: return ImGuiKey_End;
		case InputKeyboardKey::CapsLock: return ImGuiKey_CapsLock;
		case InputKeyboardKey::ScrollLock: return ImGuiKey_ScrollLock;
		case InputKeyboardKey::NumLock: return ImGuiKey_NumLock;
		case InputKeyboardKey::PrintScreen: return ImGuiKey_PrintScreen;
		case InputKeyboardKey::Pause: return ImGuiKey_Pause;
		case InputKeyboardKey::F1: return ImGuiKey_F1;
		case InputKeyboardKey::F2: return ImGuiKey_F2;
		case InputKeyboardKey::F3: return ImGuiKey_F3;
		case InputKeyboardKey::F4: return ImGuiKey_F4;
		case InputKeyboardKey::F5: return ImGuiKey_F5;
		case InputKeyboardKey::F6: return ImGuiKey_F6;
		case InputKeyboardKey::F7: return ImGuiKey_F7;
		case InputKeyboardKey::F8: return ImGuiKey_F8;
		case InputKeyboardKey::F9: return ImGuiKey_F9;
		case InputKeyboardKey::F10: return ImGuiKey_F10;
		case InputKeyboardKey::F11: return ImGuiKey_F11;
		case InputKeyboardKey::F12: return ImGuiKey_F12;
		case InputKeyboardKey::F13: return ImGuiKey_F13;
		case InputKeyboardKey::F14: return ImGuiKey_F14;
		case InputKeyboardKey::F15: return ImGuiKey_F15;
		case InputKeyboardKey::F16: return ImGuiKey_F16;
		case InputKeyboardKey::F17: return ImGuiKey_F17;
		case InputKeyboardKey::F18: return ImGuiKey_F18;
		case InputKeyboardKey::F19: return ImGuiKey_F19;
		case InputKeyboardKey::F20: return ImGuiKey_F20;
		case InputKeyboardKey::F21: return ImGuiKey_F21;
		case InputKeyboardKey::F22: return ImGuiKey_F22;
		case InputKeyboardKey::F23: return ImGuiKey_F23;
		case InputKeyboardKey::F24: return ImGuiKey_F24;
		case InputKeyboardKey::Kp0: return ImGuiKey_Keypad0;
		case InputKeyboardKey::Kp1: return ImGuiKey_Keypad1;
		case InputKeyboardKey::Kp2: return ImGuiKey_Keypad2;
		case InputKeyboardKey::Kp3: return ImGuiKey_Keypad3;
		case InputKeyboardKey::Kp4: return ImGuiKey_Keypad4;
		case InputKeyboardKey::Kp5: return ImGuiKey_Keypad5;
		case InputKeyboardKey::Kp6: return ImGuiKey_Keypad6;
		case InputKeyboardKey::Kp7: return ImGuiKey_Keypad7;
		case InputKeyboardKey::Kp8: return ImGuiKey_Keypad8;
		case InputKeyboardKey::Kp9: return ImGuiKey_Keypad9;
		case InputKeyboardKey::KpDec: return ImGuiKey_KeypadDecimal;
		case InputKeyboardKey::KpDiv: return ImGuiKey_KeypadDivide;
		case InputKeyboardKey::KpMul: return ImGuiKey_KeypadMultiply;
		case InputKeyboardKey::KpSub: return ImGuiKey_KeypadSubtract;
		case InputKeyboardKey::KpAdd: return ImGuiKey_KeypadAdd;
		case InputKeyboardKey::KpEnter: return ImGuiKey_KeypadEnter;
		case InputKeyboardKey::KpEq: return ImGuiKey_KeypadEqual;
		case InputKeyboardKey::LeftShift: return ImGuiKey_LeftShift;
		case InputKeyboardKey::LeftControl: return ImGuiKey_LeftCtrl;
		case InputKeyboardKey::LeftAlt: return ImGuiKey_LeftAlt;
		case InputKeyboardKey::LeftSuper: return ImGuiKey_LeftSuper;
		case InputKeyboardKey::RightShift: return ImGuiKey_RightShift;
		case InputKeyboardKey::RightControl: return ImGuiKey_RightCtrl;
		case InputKeyboardKey::RightAlt: return ImGuiKey_RightAlt;
		case InputKeyboardKey::RightSuper: return ImGuiKey_RightSuper;
		case InputKeyboardKey::Menu: return ImGuiKey_Menu;
		default: return ImGuiKey_None;
		}
	}

	constexpr ImGuiKey translate_gamepad_button(InputPadBtn code) {
		switch (code)
		{
		case InputPadBtn::A: return ImGuiKey_GamepadFaceDown;
		case InputPadBtn::B: return ImGuiKey_GamepadFaceRight;
		case InputPadBtn::X: return ImGuiKey_GamepadFaceLeft;
		case InputPadBtn::Y: return ImGuiKey_GamepadFaceUp;
		case InputPadBtn::BumperLeft: return ImGuiKey_GamepadL1;
		case InputPadBtn::BumperRight: return ImGuiKey_GamepadR1;
		case InputPadBtn::Back: return ImGuiKey_GamepadBack;
		case InputPadBtn::Start: return ImGuiKey_GamepadStart;
		case InputPadBtn::Guide: return ImGuiKey_None; // ImGui doesnt have a guide button
		case InputPadBtn::ThumbLeft: return ImGuiKey_GamepadL3;
		case InputPadBtn::ThumbRight: return ImGuiKey_GamepadR3;
		case InputPadBtn::DpadUp: return ImGuiKey_GamepadDpadUp;
		case InputPadBtn::DpadRight: return ImGuiKey_GamepadDpadRight;
		case InputPadBtn::DpadDown: return ImGuiKey_GamepadDpadDown;
		case InputPadBtn::DpadLeft: return ImGuiKey_GamepadDpadLeft;
		default: return ImGuiKey_None;
		}
	}

	constexpr ImGuiMouseButton translate_mouse_code(InputMouseBtn code) {
		switch (code)
		{
		case InputMouseBtn::Left: return ImGuiMouseButton_Left;
		case InputMouseBtn::Right: return ImGuiMouseButton_Right;
		case InputMouseBtn::Middle: return ImGuiMouseButton_Middle;
		default: return ImGuiMouseButton_COUNT;
		}
	}

	ImGuiLayer* imgui_layer_create(ImGuiLayerInitInfo init_info) {
		ImGuiLayer* layer = init_info.alocator->allocate<ImGuiLayer>();
		if (!layer) {
			return nullptr;
		}

		layer->alocator = init_info.alocator;
		layer->event_dispatcher = init_info.event_dispatcher;

		ImGui::SetAllocatorFunctions(
			[](size_t size, void* user_data) -> void* {
				const Allocator* allocator = (const Allocator*)user_data;
				return allocator->malloc(size);
			},
			[](void* ptr, void* user_data) -> void {
				const Allocator* allocator = (const Allocator*)user_data;
				allocator->free(ptr);
			}, (void*)init_info.alocator);

		ImGuiContext* ctx = ImGui::CreateContext();
		if (!ctx) {
			return nullptr;
		}

		ImGuiIO& io = ImGui::GetIO();
		IMGUI_CHECKVERSION();
		IM_ASSERT(io.BackendRendererUserData == nullptr && "Already initialized a renderer backend!");

		io.BackendRendererUserData = layer;
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

		f32 scale_factor = window_dpi_scale_factor(init_info.window);

		io.FontGlobalScale = scale_factor;

		ImGuiStyle& style = ImGui::GetStyle();
		style.ScaleAllSizes(scale_factor);

		i32 width, height;
		window_get_size(init_info.window, &width, &height);

		io.DisplaySize.x = (f32)width;
		io.DisplaySize.y = (f32)height;

		layer->listener_id = layer->event_dispatcher->add_listener(
			layer->alocator,
			INPUT_EVENT_MASK | WINDOW_EVENT_MASK,
			[](EventHeader* evt) -> void {
				ImGuiIO& io = ImGui::GetIO();

				if (evt->categories & INPUT_EVENT_MASK) {
					switch ((InputEventType)evt->type)
					{
					case InputEventType::Keyboard: {
						InputKeyboardEvent* e = (InputKeyboardEvent*)evt;
						io.AddKeyEvent(translate_key_code(e->key), e->action == InputKeyAction::Down);
						break;
					}
					case InputEventType::MouseMove: {
						InputMouseMoveEvent* e = (InputMouseMoveEvent*)evt;
						io.AddMousePosEvent(e->x, e->y);
						break;
					}
					case InputEventType::MouseBtn: {
						InputMouseBtnEvent* e = (InputMouseBtnEvent*)evt;
						io.AddMouseButtonEvent(translate_mouse_code(e->btn), e->action == InputKeyAction::Down);
						break;
					}
					case InputEventType::MouseScroll: {
						InputMouseScrollEvent* e = (InputMouseScrollEvent*)evt;
						io.AddMouseWheelEvent(e->xoffset, e->yoffset);
						break;
					}
					case InputEventType::TextInput: {
						InputTextInputEvent* e = (InputTextInputEvent*)evt;
						io.AddInputCharacter(e->codepoint);
						break;
					}
					case InputEventType::PadConnection: {
						InputPadConnectionEvent* e = (InputPadConnectionEvent*)evt;
						if (e->connected) {
							io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
						}
						else {
							io.BackendFlags &= ~ImGuiBackendFlags_HasGamepad;
						}
						break;
					}
					case InputEventType::PadButton: {
						InputPadButtonEvent* e = (InputPadButtonEvent*)evt;
						ImGuiKey key = translate_gamepad_button(e->btn);
						if (key != ImGuiKey_None) {
							io.AddKeyEvent(key, e->state == InputKeyAction::Down);
							io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
						}
						break;
					}
					case InputEventType::PadAxis: {
						InputPadAxisEvent* e = (InputPadAxisEvent*)evt;
						switch (e->axis) {
						case InputPadAxis::StickLeft: {
							auto [x, y] = radial_deadzone(e->x, e->y, IMGUI_STICK_DEADZONE);
							handle_axis_direction(io, ImGuiKey_GamepadLStickLeft, ImGuiKey_GamepadLStickRight, x, 0.0f);
							handle_axis_direction(io, ImGuiKey_GamepadLStickUp, ImGuiKey_GamepadLStickDown, y, 0.0f);
							break;
						}
						case InputPadAxis::StickRight: {
							auto [x, y] = radial_deadzone(e->x, e->y, IMGUI_STICK_DEADZONE);
							handle_axis_direction(io, ImGuiKey_GamepadRStickLeft, ImGuiKey_GamepadRStickRight, x, 0.0f);
							handle_axis_direction(io, ImGuiKey_GamepadRStickUp, ImGuiKey_GamepadRStickDown, y, 0.0f);
							break;
						}
						case InputPadAxis::TriggerLeft: {
							float value = simple_deadzone(e->x, IMGUI_TRIGGER_DEADZONE);
							io.AddKeyAnalogEvent(ImGuiKey_GamepadL2, value > IMGUI_TRIGGER_THRESHOLD, value);
							break;
						}
						case InputPadAxis::TriggerRight: {
							float value = simple_deadzone(e->x, IMGUI_TRIGGER_DEADZONE);
							io.AddKeyAnalogEvent(ImGuiKey_GamepadR2, value > IMGUI_TRIGGER_THRESHOLD, value);
							break;
						}
						default:
							break;
						}

						io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
						break;
					}
					default:
						break;
					}
				}
				else if (evt->categories & WINDOW_EVENT_MASK) {
					switch ((WindowEventType)evt->type)
					{
					case WindowEventType::Resize: {
						WindowResizeEvent* e = (WindowResizeEvent*)evt;
						io.DisplaySize.x = (f32)e->width;
						io.DisplaySize.y = (f32)e->height;
						break;
					}
					case WindowEventType::Focus: {
						WindowFocusEvent* e = (WindowFocusEvent*)evt;
						io.AddFocusEvent(e->focused);
						break;
					}
					default:
						break;
					}
				}
			}
		);

		return layer;
	}

	void imgui_layer_destroy(ImGuiLayer* layer) {
		if (!layer) {
			return;
		}

		ImGui::EndFrame();

		ImGuiIO& io = ImGui::GetIO();
		io.BackendRendererUserData = nullptr;

		ImGui::DestroyContext();

		layer->event_dispatcher->remove_listener(layer->alocator, layer->listener_id);

		const Allocator* allocator = layer->alocator;
		allocator->deallocate(layer);
	}

	void imgui_layer_update(NotNull<ImGuiLayer*> layer, f32 delta_time) {
		ImGuiIO& io = ImGui::GetIO();
		io.DeltaTime = delta_time;

		ImGui::NewFrame();
		ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

		if (ImGui::BeginMainMenuBar()) {
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("New")) {

				}
				if (ImGui::MenuItem("Open", "Ctrl+O")) {

				}
				if (ImGui::BeginMenu("Open Recent")) {
					ImGui::EndMenu();
				}
				if (ImGui::MenuItem("Save", "Ctrl+S")) {

				}
				if (ImGui::MenuItem("Save As..")) {

				}

				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Edit"))
			{
				if (ImGui::MenuItem("Undo", "CTRL+Z")) {

				}
				if (ImGui::MenuItem("Redo", "CTRL+Y", false, false)) {

				}
				ImGui::Separator();
				if (ImGui::MenuItem("Cut", "CTRL+X")) {

				}
				if (ImGui::MenuItem("Copy", "CTRL+C")) {

				}
				if (ImGui::MenuItem("Paste", "CTRL+V")) {

				}
				ImGui::EndMenu();
			}

			ImGui::EndMainMenuBar();
		}

		ImGui::ShowDemoWindow();

		ImGui::Render();
	}
}