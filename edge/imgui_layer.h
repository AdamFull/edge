#pragma once

#include "core/events.h"
#include "core/foundation/foundation.h"

#include "layer.h"

struct ImFont;

namespace edge {
	namespace platform {
		class IPlatformContext;
		class IPlatformWindow;
	}

	class ImGuiLayer final : public ILayer, public NonCopyable {
	public:
		ImGuiLayer() = default;
		~ImGuiLayer() override = default;

		ImGuiLayer(ImGuiLayer&&) = default;
		auto operator=(ImGuiLayer&&) -> ImGuiLayer & = default;

		static auto create(platform::IPlatformContext& context) -> Owned<ImGuiLayer>;

		//auto add_font(std::u8string_view path, ImFontConfig config) -> void;

		auto attach() -> void override;
		auto detach() -> void override;

		auto update(float delta_time) -> void override;
		auto fixed_update(float delta_time) -> void override;
	private:
		ImFont* icon_font_{ nullptr };
		events::Dispatcher* dispatcher_{ nullptr };
		platform::IPlatformWindow* window_{ nullptr };
		uint64_t listener_id_{ ~0ull };
	};
}