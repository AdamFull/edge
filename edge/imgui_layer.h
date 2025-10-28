#pragma once

#include "core/events.h"
#include "core/foundation/foundation.h"

#include "layer.h"

namespace edge {
	namespace platform {
		class IPlatformContext;
	}

	namespace gfx {
		class Renderer;
		class ResourceUploader;
		class ResourceUpdater;
	}

	class ImGuiLayer final : public ILayer, public NonCopyable {
	public:
		ImGuiLayer() = default;
		~ImGuiLayer() override = default;

		ImGuiLayer(ImGuiLayer&&) = default;
		auto operator=(ImGuiLayer&&) -> ImGuiLayer & = default;

		static auto create(platform::IPlatformContext& context, gfx::Renderer& renderer, gfx::ResourceUploader& uploader, gfx::ResourceUpdater& updater) -> Owned<ImGuiLayer>;

		auto attach() -> void override;
		auto detach() -> void override;

		auto update(float delta_time) -> void override;
		auto fixed_update(float delta_time) -> void override;
	private:
		events::Dispatcher* dispatcher_{ nullptr };
		gfx::Renderer* renderer_{ nullptr };
		gfx::ResourceUploader* resource_uploader_{ nullptr };
		gfx::ResourceUpdater* resource_updater_{ nullptr };
		uint32_t font_upload_task_{ ~0u };
		uint32_t font_image_id_{ ~0u };
		uint32_t vertex_buffer_id_{ ~0u };
		uint32_t index_buffer_id_{ ~0u };
		uint64_t listener_id_{ ~0ull };
	};
}