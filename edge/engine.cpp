#include "engine.h"
#include "core/platform/platform.h"

#include <imgui.h>

namespace edge {
	auto Engine::initialize(platform::IPlatformContext const& context) -> bool {
		ImGui::CreateContext();

		ImGuiIO& io = ImGui::GetIO();
		IMGUI_CHECKVERSION();
		IM_ASSERT(io.BackendRendererUserData == nullptr && "Already initialized a renderer backend!");

		io.BackendRendererUserData = (void*)this;
		io.BackendRendererName = "e2d";
		io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;  // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.
		//io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;  // We can Create multi-viewports on the Renderer side (optional)

		return true;
	}

	auto Engine::finish() -> void {
		ImGui::EndFrame();
		ImGui::DestroyContext();
	}

	auto Engine::update(float delta_time) -> void {

	}

	auto Engine::fixed_update(float delta_time) -> void {

	}
}