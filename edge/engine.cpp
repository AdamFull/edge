#include "engine.h"
#include "core/platform/platform.h"

#include <imgui.h>

namespace edge {
	inline constexpr auto translate_key_code(KeyboardKeyCode code) -> ImGuiKey {
		switch (code)
		{
		case edge::KeyboardKeyCode::eUnknown: return ImGuiKey_None;
		case edge::KeyboardKeyCode::eSpace: return ImGuiKey_Space;
		case edge::KeyboardKeyCode::eApostrophe: return ImGuiKey_Apostrophe;
		case edge::KeyboardKeyCode::eComma: return ImGuiKey_Comma;
		case edge::KeyboardKeyCode::eMinus: return ImGuiKey_Minus;
		case edge::KeyboardKeyCode::ePeriod: return ImGuiKey_Period;
		case edge::KeyboardKeyCode::eSlash: return ImGuiKey_Slash;
		case edge::KeyboardKeyCode::e0: return ImGuiKey_0;
		case edge::KeyboardKeyCode::e1: return ImGuiKey_1;
		case edge::KeyboardKeyCode::e2: return ImGuiKey_2;
		case edge::KeyboardKeyCode::e3: return ImGuiKey_3;
		case edge::KeyboardKeyCode::e4: return ImGuiKey_4;
		case edge::KeyboardKeyCode::e5: return ImGuiKey_5;
		case edge::KeyboardKeyCode::e6: return ImGuiKey_6;
		case edge::KeyboardKeyCode::e7: return ImGuiKey_7;
		case edge::KeyboardKeyCode::e8: return ImGuiKey_8;
		case edge::KeyboardKeyCode::e9: return ImGuiKey_9;
		case edge::KeyboardKeyCode::eSemicolon: return ImGuiKey_Semicolon;
		case edge::KeyboardKeyCode::eEq: return ImGuiKey_Equal;
		case edge::KeyboardKeyCode::eA: return ImGuiKey_A;
		case edge::KeyboardKeyCode::eB: return ImGuiKey_B;
		case edge::KeyboardKeyCode::eC: return ImGuiKey_C;
		case edge::KeyboardKeyCode::eD: return ImGuiKey_D;
		case edge::KeyboardKeyCode::eE: return ImGuiKey_E;
		case edge::KeyboardKeyCode::eF: return ImGuiKey_F;
		case edge::KeyboardKeyCode::eG: return ImGuiKey_G;
		case edge::KeyboardKeyCode::eH: return ImGuiKey_H;
		case edge::KeyboardKeyCode::eI: return ImGuiKey_I;
		case edge::KeyboardKeyCode::eJ: return ImGuiKey_J;
		case edge::KeyboardKeyCode::eK: return ImGuiKey_K;
		case edge::KeyboardKeyCode::eL: return ImGuiKey_L;
		case edge::KeyboardKeyCode::eM: return ImGuiKey_M;
		case edge::KeyboardKeyCode::eN: return ImGuiKey_N;
		case edge::KeyboardKeyCode::eO: return ImGuiKey_O;
		case edge::KeyboardKeyCode::eP: return ImGuiKey_P;
		case edge::KeyboardKeyCode::eQ: return ImGuiKey_Q;
		case edge::KeyboardKeyCode::eR: return ImGuiKey_R;
		case edge::KeyboardKeyCode::eS: return ImGuiKey_S;
		case edge::KeyboardKeyCode::eT: return ImGuiKey_T;
		case edge::KeyboardKeyCode::eU: return ImGuiKey_U;
		case edge::KeyboardKeyCode::eV: return ImGuiKey_V;
		case edge::KeyboardKeyCode::eW: return ImGuiKey_W;
		case edge::KeyboardKeyCode::eX: return ImGuiKey_X;
		case edge::KeyboardKeyCode::eY: return ImGuiKey_Y;
		case edge::KeyboardKeyCode::eZ: return ImGuiKey_Z;
		case edge::KeyboardKeyCode::eLeftBracket: return ImGuiKey_LeftBracket;
		case edge::KeyboardKeyCode::eBackslash: return ImGuiKey_Backslash;
		case edge::KeyboardKeyCode::eRightBracket: return ImGuiKey_RightBracket;
		case edge::KeyboardKeyCode::eGraveAccent: return ImGuiKey_GraveAccent;
		case edge::KeyboardKeyCode::eEsc: return ImGuiKey_Escape;
		case edge::KeyboardKeyCode::eEnter: return ImGuiKey_Enter;
		case edge::KeyboardKeyCode::eTab: return ImGuiKey_Tab;
		case edge::KeyboardKeyCode::eBackspace: return ImGuiKey_Backspace;
		case edge::KeyboardKeyCode::eInsert: return ImGuiKey_Insert;
		case edge::KeyboardKeyCode::eDel: return ImGuiKey_Delete;
		case edge::KeyboardKeyCode::eRight: return ImGuiKey_RightArrow;
		case edge::KeyboardKeyCode::eLeft: return ImGuiKey_LeftArrow;
		case edge::KeyboardKeyCode::eDown: return ImGuiKey_DownArrow;
		case edge::KeyboardKeyCode::eUp: return ImGuiKey_UpArrow;
		case edge::KeyboardKeyCode::ePageUp: return ImGuiKey_PageUp;
		case edge::KeyboardKeyCode::ePageDown: return ImGuiKey_PageDown;
		case edge::KeyboardKeyCode::eHome: return ImGuiKey_Home;
		case edge::KeyboardKeyCode::eEnd: return ImGuiKey_End;
		case edge::KeyboardKeyCode::eCapsLock: return ImGuiKey_CapsLock;
		case edge::KeyboardKeyCode::eScrollLock: return ImGuiKey_ScrollLock;
		case edge::KeyboardKeyCode::eNumLock: return ImGuiKey_NumLock;
		case edge::KeyboardKeyCode::ePrintScreen: return ImGuiKey_PrintScreen;
		case edge::KeyboardKeyCode::ePause: return ImGuiKey_Pause;
		case edge::KeyboardKeyCode::eF1: return ImGuiKey_F1;
		case edge::KeyboardKeyCode::eF2: return ImGuiKey_F2;
		case edge::KeyboardKeyCode::eF3: return ImGuiKey_F3;
		case edge::KeyboardKeyCode::eF4: return ImGuiKey_F4;
		case edge::KeyboardKeyCode::eF5: return ImGuiKey_F5;
		case edge::KeyboardKeyCode::eF6: return ImGuiKey_F6;
		case edge::KeyboardKeyCode::eF7: return ImGuiKey_F7;
		case edge::KeyboardKeyCode::eF8: return ImGuiKey_F8;
		case edge::KeyboardKeyCode::eF9: return ImGuiKey_F9;
		case edge::KeyboardKeyCode::eF10: return ImGuiKey_F10;
		case edge::KeyboardKeyCode::eF11: return ImGuiKey_F11;
		case edge::KeyboardKeyCode::eF12: return ImGuiKey_F12;
		case edge::KeyboardKeyCode::eF13: return ImGuiKey_F13;
		case edge::KeyboardKeyCode::eF14: return ImGuiKey_F14;
		case edge::KeyboardKeyCode::eF15: return ImGuiKey_F15;
		case edge::KeyboardKeyCode::eF16: return ImGuiKey_F16;
		case edge::KeyboardKeyCode::eF17: return ImGuiKey_F17;
		case edge::KeyboardKeyCode::eF18: return ImGuiKey_F18;
		case edge::KeyboardKeyCode::eF19: return ImGuiKey_F19;
		case edge::KeyboardKeyCode::eF20: return ImGuiKey_F20;
		case edge::KeyboardKeyCode::eF21: return ImGuiKey_F21;
		case edge::KeyboardKeyCode::eF22: return ImGuiKey_F22;
		case edge::KeyboardKeyCode::eF23: return ImGuiKey_F23;
		case edge::KeyboardKeyCode::eF24: return ImGuiKey_F24;
		case edge::KeyboardKeyCode::eKp0: return ImGuiKey_Keypad0;
		case edge::KeyboardKeyCode::eKp1: return ImGuiKey_Keypad1;
		case edge::KeyboardKeyCode::eKp2: return ImGuiKey_Keypad2;
		case edge::KeyboardKeyCode::eKp3: return ImGuiKey_Keypad3;
		case edge::KeyboardKeyCode::eKp4: return ImGuiKey_Keypad4;
		case edge::KeyboardKeyCode::eKp5: return ImGuiKey_Keypad5;
		case edge::KeyboardKeyCode::eKp6: return ImGuiKey_Keypad6;
		case edge::KeyboardKeyCode::eKp7: return ImGuiKey_Keypad7;
		case edge::KeyboardKeyCode::eKp8: return ImGuiKey_Keypad8;
		case edge::KeyboardKeyCode::eKp9: return ImGuiKey_Keypad9;
		case edge::KeyboardKeyCode::eKpDec: return ImGuiKey_KeypadDecimal;
		case edge::KeyboardKeyCode::eKpDiv: return ImGuiKey_KeypadDivide;
		case edge::KeyboardKeyCode::eKpMul: return ImGuiKey_KeypadMultiply;
		case edge::KeyboardKeyCode::eKpSub: return ImGuiKey_KeypadSubtract;
		case edge::KeyboardKeyCode::eKpAdd: return ImGuiKey_KeypadAdd;
		case edge::KeyboardKeyCode::eKpEnter: return ImGuiKey_KeypadEnter;
		case edge::KeyboardKeyCode::eKpEq: return ImGuiKey_KeypadEqual;
		case edge::KeyboardKeyCode::eLeftShift: return ImGuiKey_LeftShift;
		case edge::KeyboardKeyCode::eLeftControl: return ImGuiKey_LeftCtrl;
		case edge::KeyboardKeyCode::eLeftAlt: return ImGuiKey_LeftAlt;
		case edge::KeyboardKeyCode::eLeftSuper: return ImGuiKey_LeftSuper;
		case edge::KeyboardKeyCode::eRightShift: return ImGuiKey_RightShift;
		case edge::KeyboardKeyCode::eRightControl: return ImGuiKey_RightCtrl;
		case edge::KeyboardKeyCode::eRightAlt: return ImGuiKey_RightAlt;
		case edge::KeyboardKeyCode::eRightSuper: return ImGuiKey_RightSuper;
		case edge::KeyboardKeyCode::eMenu: return ImGuiKey_Menu;
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

	auto Engine::initialize(platform::IPlatformContext& context) -> bool {
		gfx::RendererCreateInfo renderer_create_info{};
		renderer_create_info.enable_hdr = true;
		renderer_create_info.enable_vsync = false;

		auto gfx_renderer_result = gfx::Renderer::construct(renderer_create_info);
		if (!gfx_renderer_result) {
			EDGE_LOGE("Failed to create renderer. Reason: {}.", vk::to_string(gfx_renderer_result.error()));
			return false;
		}

		renderer_ = std::move(gfx_renderer_result.value());

		ImGui::CreateContext();

		ImGuiIO& io = ImGui::GetIO();
		IMGUI_CHECKVERSION();
		IM_ASSERT(io.BackendRendererUserData == nullptr && "Already initialized a renderer backend!");

		io.BackendRendererUserData = (void*)this;
		io.BackendRendererName = "edge";
		io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;  // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.
		//io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;  // We can Create multi-viewports on the Renderer side (optional)

		io.Fonts->Build();

		window_ = &context.get_window();

		auto& event_dispatcher = context.get_event_dispatcher();
		event_dispatcher.add_listener<events::EventTag::eWindow, events::EventTag::eRawInput>(
			[](const events::Dispatcher::event_variant_t& e, void* user_ptr) {
				auto* self = static_cast<Engine*>(user_ptr);
				
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
					else if constexpr (std::same_as<EventType, events::WindowFocusChangedEvent>) {
						io.AddFocusEvent(e.focused);
					}
					}, e);

			}, this);

		return true;
	}

	auto Engine::finish() -> void {
		ImGui::EndFrame();
		ImGui::DestroyContext();
	}

	auto Engine::update(float delta_time) -> void {
		ImGuiIO& io = ImGui::GetIO();
		io.DisplaySize = ImVec2(static_cast<float>(window_->get_width()), static_cast<float>(window_->get_height()));
		io.DeltaTime = delta_time;
		

		ImGui::NewFrame();

		ImGui::Render();

		renderer_->begin_frame(delta_time);

		renderer_->end_frame();
	}

	auto Engine::fixed_update(float delta_time) -> void {

	}
}