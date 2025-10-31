#include "imgui_layer.h"
#include "core/platform/platform.h"

#include "../assets/shaders/imgui.h"

#include <imgui.h>

#define EDGE_LOGGER_SCOPE "ImGuiLayer"

namespace edge {
	constexpr float IMGUI_STICK_DEADZONE = 0.15f;
	constexpr float IMGUI_TRIGGER_DEADZONE = 0.15f;
	constexpr float IMGUI_TRIGGER_THRESHOLD = 0.15f;

	// TODO: Move somewhere else
	inline auto radial_deadzone(float x, float y, float deadzone) -> std::pair<float, float> {
		float magnitude = std::sqrt(x * x + y * y);
		if (magnitude < deadzone) {
			return { 0.0f, 0.0f };
		}

		float scale = (magnitude - deadzone) / (1.0f - deadzone);
		scale = std::min(scale, 1.0f);

		float normalized_x = x / magnitude;
		float normalized_y = y / magnitude;

		return { normalized_x * scale, normalized_y * scale };
	}

	inline auto simple_deadzone(float value, float deadzone) -> float {
		if (value < deadzone) {
			return 0.0f;
		}

		return (value - deadzone) / (1.0f - deadzone);
	}

	inline constexpr auto translate_key_code(KeyboardKeyCode code) -> ImGuiKey {
		switch (code)
		{
		case KeyboardKeyCode::eUnknown: return ImGuiKey_None;
		case KeyboardKeyCode::eSpace: return ImGuiKey_Space;
		case KeyboardKeyCode::eApostrophe: return ImGuiKey_Apostrophe;
		case KeyboardKeyCode::eComma: return ImGuiKey_Comma;
		case KeyboardKeyCode::eMinus: return ImGuiKey_Minus;
		case KeyboardKeyCode::ePeriod: return ImGuiKey_Period;
		case KeyboardKeyCode::eSlash: return ImGuiKey_Slash;
		case KeyboardKeyCode::e0: return ImGuiKey_0;
		case KeyboardKeyCode::e1: return ImGuiKey_1;
		case KeyboardKeyCode::e2: return ImGuiKey_2;
		case KeyboardKeyCode::e3: return ImGuiKey_3;
		case KeyboardKeyCode::e4: return ImGuiKey_4;
		case KeyboardKeyCode::e5: return ImGuiKey_5;
		case KeyboardKeyCode::e6: return ImGuiKey_6;
		case KeyboardKeyCode::e7: return ImGuiKey_7;
		case KeyboardKeyCode::e8: return ImGuiKey_8;
		case KeyboardKeyCode::e9: return ImGuiKey_9;
		case KeyboardKeyCode::eSemicolon: return ImGuiKey_Semicolon;
		case KeyboardKeyCode::eEq: return ImGuiKey_Equal;
		case KeyboardKeyCode::eA: return ImGuiKey_A;
		case KeyboardKeyCode::eB: return ImGuiKey_B;
		case KeyboardKeyCode::eC: return ImGuiKey_C;
		case KeyboardKeyCode::eD: return ImGuiKey_D;
		case KeyboardKeyCode::eE: return ImGuiKey_E;
		case KeyboardKeyCode::eF: return ImGuiKey_F;
		case KeyboardKeyCode::eG: return ImGuiKey_G;
		case KeyboardKeyCode::eH: return ImGuiKey_H;
		case KeyboardKeyCode::eI: return ImGuiKey_I;
		case KeyboardKeyCode::eJ: return ImGuiKey_J;
		case KeyboardKeyCode::eK: return ImGuiKey_K;
		case KeyboardKeyCode::eL: return ImGuiKey_L;
		case KeyboardKeyCode::eM: return ImGuiKey_M;
		case KeyboardKeyCode::eN: return ImGuiKey_N;
		case KeyboardKeyCode::eO: return ImGuiKey_O;
		case KeyboardKeyCode::eP: return ImGuiKey_P;
		case KeyboardKeyCode::eQ: return ImGuiKey_Q;
		case KeyboardKeyCode::eR: return ImGuiKey_R;
		case KeyboardKeyCode::eS: return ImGuiKey_S;
		case KeyboardKeyCode::eT: return ImGuiKey_T;
		case KeyboardKeyCode::eU: return ImGuiKey_U;
		case KeyboardKeyCode::eV: return ImGuiKey_V;
		case KeyboardKeyCode::eW: return ImGuiKey_W;
		case KeyboardKeyCode::eX: return ImGuiKey_X;
		case KeyboardKeyCode::eY: return ImGuiKey_Y;
		case KeyboardKeyCode::eZ: return ImGuiKey_Z;
		case KeyboardKeyCode::eLeftBracket: return ImGuiKey_LeftBracket;
		case KeyboardKeyCode::eBackslash: return ImGuiKey_Backslash;
		case KeyboardKeyCode::eRightBracket: return ImGuiKey_RightBracket;
		case KeyboardKeyCode::eGraveAccent: return ImGuiKey_GraveAccent;
		case KeyboardKeyCode::eEsc: return ImGuiKey_Escape;
		case KeyboardKeyCode::eEnter: return ImGuiKey_Enter;
		case KeyboardKeyCode::eTab: return ImGuiKey_Tab;
		case KeyboardKeyCode::eBackspace: return ImGuiKey_Backspace;
		case KeyboardKeyCode::eInsert: return ImGuiKey_Insert;
		case KeyboardKeyCode::eDel: return ImGuiKey_Delete;
		case KeyboardKeyCode::eRight: return ImGuiKey_RightArrow;
		case KeyboardKeyCode::eLeft: return ImGuiKey_LeftArrow;
		case KeyboardKeyCode::eDown: return ImGuiKey_DownArrow;
		case KeyboardKeyCode::eUp: return ImGuiKey_UpArrow;
		case KeyboardKeyCode::ePageUp: return ImGuiKey_PageUp;
		case KeyboardKeyCode::ePageDown: return ImGuiKey_PageDown;
		case KeyboardKeyCode::eHome: return ImGuiKey_Home;
		case KeyboardKeyCode::eEnd: return ImGuiKey_End;
		case KeyboardKeyCode::eCapsLock: return ImGuiKey_CapsLock;
		case KeyboardKeyCode::eScrollLock: return ImGuiKey_ScrollLock;
		case KeyboardKeyCode::eNumLock: return ImGuiKey_NumLock;
		case KeyboardKeyCode::ePrintScreen: return ImGuiKey_PrintScreen;
		case KeyboardKeyCode::ePause: return ImGuiKey_Pause;
		case KeyboardKeyCode::eF1: return ImGuiKey_F1;
		case KeyboardKeyCode::eF2: return ImGuiKey_F2;
		case KeyboardKeyCode::eF3: return ImGuiKey_F3;
		case KeyboardKeyCode::eF4: return ImGuiKey_F4;
		case KeyboardKeyCode::eF5: return ImGuiKey_F5;
		case KeyboardKeyCode::eF6: return ImGuiKey_F6;
		case KeyboardKeyCode::eF7: return ImGuiKey_F7;
		case KeyboardKeyCode::eF8: return ImGuiKey_F8;
		case KeyboardKeyCode::eF9: return ImGuiKey_F9;
		case KeyboardKeyCode::eF10: return ImGuiKey_F10;
		case KeyboardKeyCode::eF11: return ImGuiKey_F11;
		case KeyboardKeyCode::eF12: return ImGuiKey_F12;
		case KeyboardKeyCode::eF13: return ImGuiKey_F13;
		case KeyboardKeyCode::eF14: return ImGuiKey_F14;
		case KeyboardKeyCode::eF15: return ImGuiKey_F15;
		case KeyboardKeyCode::eF16: return ImGuiKey_F16;
		case KeyboardKeyCode::eF17: return ImGuiKey_F17;
		case KeyboardKeyCode::eF18: return ImGuiKey_F18;
		case KeyboardKeyCode::eF19: return ImGuiKey_F19;
		case KeyboardKeyCode::eF20: return ImGuiKey_F20;
		case KeyboardKeyCode::eF21: return ImGuiKey_F21;
		case KeyboardKeyCode::eF22: return ImGuiKey_F22;
		case KeyboardKeyCode::eF23: return ImGuiKey_F23;
		case KeyboardKeyCode::eF24: return ImGuiKey_F24;
		case KeyboardKeyCode::eKp0: return ImGuiKey_Keypad0;
		case KeyboardKeyCode::eKp1: return ImGuiKey_Keypad1;
		case KeyboardKeyCode::eKp2: return ImGuiKey_Keypad2;
		case KeyboardKeyCode::eKp3: return ImGuiKey_Keypad3;
		case KeyboardKeyCode::eKp4: return ImGuiKey_Keypad4;
		case KeyboardKeyCode::eKp5: return ImGuiKey_Keypad5;
		case KeyboardKeyCode::eKp6: return ImGuiKey_Keypad6;
		case KeyboardKeyCode::eKp7: return ImGuiKey_Keypad7;
		case KeyboardKeyCode::eKp8: return ImGuiKey_Keypad8;
		case KeyboardKeyCode::eKp9: return ImGuiKey_Keypad9;
		case KeyboardKeyCode::eKpDec: return ImGuiKey_KeypadDecimal;
		case KeyboardKeyCode::eKpDiv: return ImGuiKey_KeypadDivide;
		case KeyboardKeyCode::eKpMul: return ImGuiKey_KeypadMultiply;
		case KeyboardKeyCode::eKpSub: return ImGuiKey_KeypadSubtract;
		case KeyboardKeyCode::eKpAdd: return ImGuiKey_KeypadAdd;
		case KeyboardKeyCode::eKpEnter: return ImGuiKey_KeypadEnter;
		case KeyboardKeyCode::eKpEq: return ImGuiKey_KeypadEqual;
		case KeyboardKeyCode::eLeftShift: return ImGuiKey_LeftShift;
		case KeyboardKeyCode::eLeftControl: return ImGuiKey_LeftCtrl;
		case KeyboardKeyCode::eLeftAlt: return ImGuiKey_LeftAlt;
		case KeyboardKeyCode::eLeftSuper: return ImGuiKey_LeftSuper;
		case KeyboardKeyCode::eRightShift: return ImGuiKey_RightShift;
		case KeyboardKeyCode::eRightControl: return ImGuiKey_RightCtrl;
		case KeyboardKeyCode::eRightAlt: return ImGuiKey_RightAlt;
		case KeyboardKeyCode::eRightSuper: return ImGuiKey_RightSuper;
		case KeyboardKeyCode::eMenu: return ImGuiKey_Menu;
		default: return ImGuiKey_None;
		}
	}

	inline constexpr auto translate_gamepad_button(GamepadKeyCode code) -> ImGuiKey {
		switch (code)
		{
		case GamepadKeyCode::eButtonA: return ImGuiKey_GamepadFaceDown;
		case GamepadKeyCode::eButtonB: return ImGuiKey_GamepadFaceRight;
		case GamepadKeyCode::eButtonX: return ImGuiKey_GamepadFaceLeft;
		case GamepadKeyCode::eButtonY: return ImGuiKey_GamepadFaceUp;
		case GamepadKeyCode::eButtonLeftBumper: return ImGuiKey_GamepadL1;
		case GamepadKeyCode::eButtonRightBumper: return ImGuiKey_GamepadR1;
		case GamepadKeyCode::eButtonBack: return ImGuiKey_GamepadBack;
		case GamepadKeyCode::eButtonStart: return ImGuiKey_GamepadStart;
		case GamepadKeyCode::eButtonGuide: return ImGuiKey_None; // ImGui doesnt have a guide button
		case GamepadKeyCode::eButtonLeftThumb: return ImGuiKey_GamepadL3;
		case GamepadKeyCode::eButtonRightThumb: return ImGuiKey_GamepadR3;
		case GamepadKeyCode::eButtonDPadUp: return ImGuiKey_GamepadDpadUp;
		case GamepadKeyCode::eButtonDPadRight: return ImGuiKey_GamepadDpadRight;
		case GamepadKeyCode::eButtonDPadDown: return ImGuiKey_GamepadDpadDown;
		case GamepadKeyCode::eButtonDPadLeft: return ImGuiKey_GamepadDpadLeft;
		default: return ImGuiKey_None;
		}
	}

	inline constexpr auto translate_mouse_code(MouseKeyCode code) -> ImGuiMouseButton {
		switch (code)
		{
		case edge::MouseKeyCode::eMouseLeft: return ImGuiMouseButton_Left;
		case edge::MouseKeyCode::eMouseRight: return ImGuiMouseButton_Right;
		case edge::MouseKeyCode::eMouseMiddle: return ImGuiMouseButton_Middle;
		default: return ImGuiMouseButton_COUNT;
		}
	}

	inline auto handle_axis_direction(ImGuiIO& io, ImGuiKey negative_key, ImGuiKey positive_key, float value, float threshold) -> void {
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

	auto ImGuiLayer::create(platform::IPlatformContext& context) -> Owned<ImGuiLayer> {
		Owned<ImGuiLayer> self = std::make_unique<ImGuiLayer>();
		self->dispatcher_ = &context.get_event_dispatcher();
		self->window_ = &context.get_window();

		ImGui::SetAllocatorFunctions(
			[](size_t size, void* user_data) -> void* {
				return mi_malloc(size);
			},
			[](void* ptr, void* user_data) -> void {
				mi_free(ptr);
			}, nullptr);

		return self;
	}

	auto ImGuiLayer::attach() -> void {
		ImGui::CreateContext();

		ImGuiIO& io = ImGui::GetIO();
		IMGUI_CHECKVERSION();
		IM_ASSERT(io.BackendRendererUserData == nullptr && "Already initialized a renderer backend!");

		io.BackendRendererUserData = (void*)this;
		io.BackendRendererName = "edge";
		//io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
		io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;  // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.
		io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;  // We can honor ImGuiPlatformIO::Textures[] requests during render.
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
#if EDGE_PLATFORM_ANDROID
		io.ConfigFlags |= ImGuiConfigFlags_IsTouchScreen;
#endif
		io.ConfigDpiScaleFonts = true;
		

		io.Fonts->Build();

		io.DisplaySize = ImVec2(static_cast<float>(window_->get_width()), static_cast<float>(window_->get_height()));

		// Register event listener
		listener_id_ = dispatcher_->add_listener<events::EventTag::eWindow, events::EventTag::eRawInput>(
			[](const events::Dispatcher::event_variant_t& e, void* user_ptr) {
				auto* self = static_cast<ImGuiLayer*>(user_ptr);

				std::visit([self](const auto& e) {
					ImGuiIO& io = ImGui::GetIO();
					using EventType = std::decay_t<decltype(e)>;
					if constexpr (std::same_as<EventType, events::KeyEvent>) {
						io.AddKeyEvent(translate_key_code(e.key_code), e.state);
					}
					else if constexpr (std::same_as<EventType, events::MousePositionEvent>) {
						io.AddMousePosEvent(static_cast<float>(e.x), static_cast<float>(e.y));
					}
					else if constexpr (std::same_as<EventType, events::MouseKeyEvent>) {
						io.AddMouseButtonEvent(translate_mouse_code(e.key_code), e.state);
					}
					else if constexpr (std::same_as<EventType, events::MouseScrollEvent>) {
						io.AddMouseWheelEvent(static_cast<float>(e.offset_x), static_cast<float>(e.offset_y));
					}
					else if constexpr (std::same_as<EventType, events::CharacterInputEvent>) {
						io.AddInputCharacter(e.charcode);
					}
					else if constexpr (std::same_as<EventType, events::GamepadConnectionEvent>) {
						if (e.connected) {
							io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
						}
						else {
							io.BackendFlags &= ~ImGuiBackendFlags_HasGamepad;
						}
					}
					else if constexpr (std::same_as<EventType, events::GamepadButtonEvent>) {
						ImGuiKey key = translate_gamepad_button(e.key_code);
						if (key != ImGuiKey_None) {
							io.AddKeyEvent(key, e.state);
							io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
						}
					}
					else if constexpr (std::same_as<EventType, events::GamepadAxisEvent>) {
						switch (e.axis_code) {
						case GamepadAxisCode::eLeftStick: {
							auto [x, y] = radial_deadzone(e.values[0], e.values[1], IMGUI_STICK_DEADZONE);
							handle_axis_direction(io, ImGuiKey_GamepadLStickLeft, ImGuiKey_GamepadLStickRight, x, 0.0f);
							handle_axis_direction(io, ImGuiKey_GamepadLStickUp, ImGuiKey_GamepadLStickDown, y, 0.0f);
							break;
						}
						case GamepadAxisCode::eRightStick: {
							auto [x, y] = radial_deadzone(e.values[0], e.values[1], IMGUI_STICK_DEADZONE);
							handle_axis_direction(io, ImGuiKey_GamepadRStickLeft, ImGuiKey_GamepadRStickRight, x, 0.0f);
							handle_axis_direction(io, ImGuiKey_GamepadRStickUp, ImGuiKey_GamepadRStickDown, y, 0.0f);
							break;
						}
						case GamepadAxisCode::eLeftTrigger: {
							float value = simple_deadzone(e.values[0], IMGUI_TRIGGER_DEADZONE);
							io.AddKeyAnalogEvent(ImGuiKey_GamepadL2, value > IMGUI_TRIGGER_THRESHOLD, value);
							break;
						}
						case GamepadAxisCode::eRightTrigger: {
							float value = simple_deadzone(e.values[0], IMGUI_TRIGGER_DEADZONE);
							io.AddKeyAnalogEvent(ImGuiKey_GamepadR2, value > IMGUI_TRIGGER_THRESHOLD, value);
							break;
						}
						default:
							break;
						}

						io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
					}
					else if constexpr (std::same_as<EventType, events::WindowFocusChangedEvent>) {
						io.AddFocusEvent(e.focused);
					}
					else if constexpr (std::same_as<EventType, events::WindowSizeChangedEvent>) {
						io.DisplaySize = ImVec2(static_cast<float>(e.width), static_cast<float>(e.height));
					}
					}, e);

			}, this);
	}

	auto ImGuiLayer::detach() -> void {
		ImGui::EndFrame();
		ImGui::DestroyContext();

		dispatcher_->remove_listener(listener_id_);
	}

	auto ImGuiLayer::update(float delta_time) -> void {
		ImGuiIO& io = ImGui::GetIO();
		io.DeltaTime = delta_time;
		
		ImGui::NewFrame();
		ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

		ImGui::ShowDemoWindow();

		static bool test_window{ true };
		if (ImGui::Begin("Test Window", &test_window)) {
			ImGui::Image(3u, ImVec2{ 512, 512 });
		}
		ImGui::End();

		ImGui::Render();
	}

	auto ImGuiLayer::fixed_update(float delta_time) -> void {

	}
}

#undef EDGE_LOGGER_SCOPE