#include "imgui_layer.h"
#include "core/platform/platform.h"

#include <imgui.h>

namespace edge {
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

	inline constexpr auto translate_mouse_code(MouseKeyCode code) -> ImGuiMouseButton {
		switch (code)
		{
		case edge::MouseKeyCode::eMouseLeft: return ImGuiMouseButton_Left;
		case edge::MouseKeyCode::eMouseRight: return ImGuiMouseButton_Right;
		case edge::MouseKeyCode::eMouseMiddle: return ImGuiMouseButton_Middle;
		default: return ImGuiMouseButton_COUNT;
		}
	}

	auto ImGuiLayer::create(platform::IPlatformContext& context) -> Owned<ImGuiLayer> {
		Owned<ImGuiLayer> self = std::make_unique<ImGuiLayer>();

		self->dispatcher_ = &context.get_event_dispatcher();

		return self;
	}

	auto ImGuiLayer::attach() -> void {
		ImGui::CreateContext();

		ImGuiIO& io = ImGui::GetIO();
		IMGUI_CHECKVERSION();
		IM_ASSERT(io.BackendRendererUserData == nullptr && "Already initialized a renderer backend!");

		io.BackendRendererUserData = (void*)this;
		io.BackendRendererName = "edge";
		io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;  // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.
		//io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;  // We can Create multi-viewports on the Renderer side (optional)

		io.Fonts->Build();

		uint8_t* font_pixels{ nullptr };
		int font_width, font_height;

		io.Fonts->GetTexDataAsRGBA32(&font_pixels, &font_width, &font_height);

		//gfx::ImageImportInfo font_inport_info{};
		//font_inport_info.raw.data.resize(font_width * font_height * 4);
		//font_inport_info.raw.width = font_width;
		//font_inport_info.raw.height = font_height;
		//uploader_.load_image(std::move(font_inport_info));

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
					else if constexpr (std::same_as<EventType, events::WindowFocusChangedEvent>) {
						io.AddFocusEvent(e.focused);
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
		//ImGuiIO& io = ImGui::GetIO();
		//io.DisplaySize = ImVec2(static_cast<float>(window_->get_width()), static_cast<float>(window_->get_height()));
		//io.DeltaTime = delta_time;
		//
		//
		//ImGui::NewFrame();
		//
		//ImGui::Render();
	}

	auto ImGuiLayer::fixed_update(float delta_time) -> void {

	}
}