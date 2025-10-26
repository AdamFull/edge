#include "engine.h"
#include "core/platform/platform.h"
#include "core/filesystem/filesystem.h"
#include "imgui_layer.h"

#include <imgui.h>

namespace edge {
	auto Engine::initialize(platform::IPlatformContext& context) -> bool {
		fs::initialize_filesystem();

		gfx::initialize_graphics(
			edge::gfx::ContextInfo{
				.preferred_device_type = vk::PhysicalDeviceType::eDiscreteGpu,
				.window = &context.get_window()
			});

		gfx::RendererCreateInfo renderer_create_info{};
		renderer_create_info.enable_hdr = true;
		renderer_create_info.enable_vsync = false;

		auto gfx_renderer_result = gfx::Renderer::construct(renderer_create_info);
		if (!gfx_renderer_result) {
			EDGE_LOGE("Failed to create renderer. Reason: {}.", vk::to_string(gfx_renderer_result.error()));
			return false;
		}
		renderer_ = std::move(gfx_renderer_result.value());

		auto gfx_updater_result = gfx::ResourceUpdater::create(32ull * 1024ull * 1024ull, 2u);
		if (!gfx_updater_result) {
			EDGE_LOGE("Failed to create resource updater. Reason: {}.", vk::to_string(gfx_updater_result.error()));
			return false;
		}
		updater_ = std::move(gfx_updater_result.value());

		auto gfx_uploader_result = gfx::ResourceUploader::create(128ull * 1024ull * 1024ull, 2u);
		if (!gfx_uploader_result) {
			EDGE_LOGE("Failed to create resource uploader. Reason: {}.", vk::to_string(gfx_uploader_result.error()));
			return false;
		}
		uploader_ = std::move(gfx_uploader_result.value());
		uploader_.start_streamer();

		window_ = &context.get_window();

		// Resource uploader test
		auto resource_id = renderer_->create_render_resource();
		auto streamer_id = uploader_.load_image({ .path = u8"/assets/images/CasualDay4K.ktx2" });
		pending_uploads_.push_back(std::make_pair(resource_id, streamer_id));
		//panding_tokens_.push_back(uploader_.load_image({ .path = u8"/assets/images/Poliigon_BrickWallReclaimed_8320_BaseColor.jpg" }));
		//panding_tokens_.push_back(uploader_.load_image({
		//	.path = u8"/assets/images/Poliigon_BrickWallReclaimed_8320_Normal.png",
		//	.import_type = gfx::ImageImportType::eNormalMap
		//	}));

		return true;
	}

	auto Engine::finish() -> void {
		gfx::shutdown_graphics();
		fs::shutdown_filesystem();
	}

	auto Engine::update(float delta_time) -> void {
		// Process pending resources
		// TODO: Add new resource to renderer
		for (auto it = pending_uploads_.begin(); it != pending_uploads_.end();) {
			if (uploader_.is_task_done(it->second)) {
				auto task_result = uploader_.get_task_result(it->second);
				if (task_result) {
					auto& render_resource = renderer_->get_render_resource(it->first);
					render_resource.setup(std::move(std::get<gfx::Image>(task_result->data)), gfx::ResourceStateFlag::eShaderResource);
				}

				it = pending_uploads_.erase(it);
			}
			else {
				++it;
			}
		}

		renderer_->begin_frame(delta_time);

		mi::Vector<vk::SemaphoreSubmitInfoKHR> uploader_submitted_semaphores{};
		auto updater_semaphore = updater_.flush({});
		if (updater_semaphore.semaphore) {
			uploader_submitted_semaphores.push_back(updater_semaphore);
		}

		auto uploader_semaphore = uploader_.get_last_submitted_semaphore();
		if (uploader_semaphore.semaphore) {
			uploader_submitted_semaphores.push_back(uploader_semaphore);
		}

		renderer_->end_frame(uploader_submitted_semaphores);
	}

	auto Engine::fixed_update(float delta_time) -> void {

	}
}