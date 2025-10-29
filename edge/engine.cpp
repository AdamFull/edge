#include "engine.h"
#include "core/platform/platform.h"
#include "core/filesystem/filesystem.h"
#include "imgui_layer.h"

#include "core/gfx/gfx_imgui_pass.h"
#include "core/gfx/gfx_test_pass.h"

#include "../assets/shaders/fullscreen.h"

#define EDGE_LOGGER_SCOPE "Engine"

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

		auto gfx_updater_result = gfx::ResourceUpdater::create(renderer_->get_queue(), 32ull * 1024ull * 1024ull, 2u);
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

		auto& pipeline_layout = renderer_->get_pipeline_layout();
		auto& swapchain = renderer_->get_swapchain();

		gfx::ShaderLibraryInfo shader_library_info{};
		shader_library_info.pipeline_layout = &pipeline_layout;
		shader_library_info.pipeline_cache_path = u8"/shader_cache.cache";
		shader_library_info.library_path = u8"/assets/shaders";
		shader_library_info.backbuffer_format = swapchain.get_format();

		auto shader_library_result = gfx::ShaderLibrary::construct(shader_library_info);
		if (!shader_library_result) {
			GFX_ASSERT_MSG(false, "Failed to create shader library. Reason: {}.", vk::to_string(shader_library_result.error()));
			return false;
		}
		shader_library_ = std::move(shader_library_result.value());

		window_ = &context.get_window();

		// Resource uploader test
		auto resource_id = renderer_->create_render_resource();
		auto streamer_id = uploader_.load_image({ .path = u8"/assets/images/Poliigon_BrickWallReclaimed_8320_BaseColor.jpg" });
		pending_uploads_.push_back(std::make_pair(resource_id, streamer_id));

		layers_.push_back(ImGuiLayer::create(context));
		for (auto& layer : layers_) {
			layer->attach();
		}

		auto* fullscreen_pipeline = shader_library_.get_pipeline("fullscreen");
		renderer_->add_shader_pass(gfx::TestPass::create(*renderer_, 2u, fullscreen_pipeline));

		auto* imgui_pipeline = shader_library_.get_pipeline("imgui");
		renderer_->add_shader_pass(gfx::ImGuiPass::create(*renderer_, updater_, uploader_, imgui_pipeline));

		return true;
	}

	auto Engine::finish() -> void {
		for (auto& layer : layers_) {
			layer->detach();
		}

		gfx::shutdown_graphics();
		fs::shutdown_filesystem();
	}

	auto Engine::update(float delta_time) -> void {
		renderer_->begin_frame(delta_time);

		// Collect uploaded resource updates
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

		for (auto& layer : layers_) {
			layer->update(delta_time);
		}

		// Execute collected render commands
		renderer_->execute_graph(delta_time);

		// Synchronize main queue with uploader and updater queues
		mi::Vector<vk::SemaphoreSubmitInfoKHR> uploader_submitted_semaphores{};
		auto uploader_semaphore = uploader_.get_last_submitted_semaphore();
		if (uploader_semaphore.semaphore) {
			uploader_submitted_semaphores.push_back(uploader_semaphore);
		}

		auto updater_semaphore = updater_.flush(uploader_submitted_semaphores);
		if (updater_semaphore.semaphore) {
			uploader_submitted_semaphores.push_back(updater_semaphore);
		}

		renderer_->end_frame(uploader_submitted_semaphores);
	}

	auto Engine::fixed_update(float delta_time) -> void {

	}
}

#undef EDGE_LOGGER_SCOPE 