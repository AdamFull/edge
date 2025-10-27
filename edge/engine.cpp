#include "engine.h"
#include "core/platform/platform.h"
#include "core/filesystem/filesystem.h"
#include "imgui_layer.h"

#include "../assets/shaders/fullscreen.h"

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
		auto streamer_id = uploader_.load_image({ .path = u8"/assets/images/Poliigon_BrickWallReclaimed_8320_BaseColor.jpg" });
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

		renderer_->add_shader_pass({
			.name = u8"fit_screen",
			.pipeline_name = "fullscreen",
			.setup_cb = [this](gfx::Renderer& pass) -> void {
				auto backbuffer_id = pass.get_backbuffer_resource_id();
				auto& backbuffer = pass.get_render_resource(backbuffer_id);
				auto& backbuffer_image = backbuffer.get_handle<gfx::Image>();
				auto extent = backbuffer_image.get_extent();

				pass.add_color_attachment(backbuffer_id);
				pass.set_render_area({ {}, {extent.width, extent.height } });
			},
			.execute_cb = [this](gfx::Renderer& pass, gfx::CommandBuffer const& cmd, float delta_time) -> void {
				auto& backbuffer = pass.get_backbuffer_resource();
				auto& backbuffer_image = backbuffer.get_handle<gfx::Image>();
				auto extent = backbuffer_image.get_extent();

				vk::Viewport viewport{ 0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height), 0.0f, 1.0f };
				cmd->setViewport(0u, 1u, &viewport);
				vk::Rect2D scissor{ { 0u, 0u }, { extent.width , extent.height } };
				cmd->setScissor(0u, 1u, &scissor);

				gfx::fullscreen::PushConstant constants{};
				constants.width = extent.width;
				constants.height = extent.height;
				constants.image_id = 0u;

				auto constant_range_ptr = reinterpret_cast<const uint8_t*>(&constants);
				pass.push_constant_range(cmd, vk::ShaderStageFlagBits::eAllGraphics | vk::ShaderStageFlagBits::eCompute, { constant_range_ptr, sizeof(gfx::fullscreen::PushConstant) });

				cmd->draw(3u, 1u, 0u, 0u);
			}
			});

		renderer_->begin_frame(delta_time);


		for (auto it = pending_uploads_.begin(); it != pending_uploads_.end();) {
			if (uploader_.is_task_done(it->second)) {
				auto task_result = uploader_.get_task_result(it->second);
				if (task_result) {
					renderer_->setup_render_resource(it->first, std::move(std::get<gfx::Image>(task_result->data)), task_result->state);
				}

				it = pending_uploads_.erase(it);
			}
			else {
				++it;
			}
		}

		renderer_->execute_graph(delta_time);

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