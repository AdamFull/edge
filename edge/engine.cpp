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

		auto queue_result = gfx::device_.get_queue({
				.required_caps = gfx::QueuePresets::kPresentGraphics,
				.strategy = gfx::QueueSelectionStrategy::ePreferDedicated
			});
		EDGE_FATAL_ERROR(queue_result.has_value(), "Failed to request graphics queue for renderer.");
		main_queue_ = std::move(queue_result.value());

		gfx::RendererCreateInfo renderer_create_info{};
		renderer_create_info.enable_hdr = true;
		renderer_create_info.enable_vsync = false;
		renderer_create_info.queue = &main_queue_;
		renderer_ = gfx::Renderer::construct(renderer_create_info);

		updater_ = gfx::ResourceUpdater::create(main_queue_, 32ull * 1024ull * 1024ull, 2u);

		uploader_ = gfx::ResourceUploader::create(main_queue_, 128ull * 1024ull * 1024ull, 2u);
		uploader_.start_streamer();

		auto& pipeline_layout = renderer_->get_pipeline_layout();
		auto& swapchain = renderer_->get_swapchain();

		gfx::ShaderLibraryInfo shader_library_info{};
		shader_library_info.pipeline_layout = &pipeline_layout;
		shader_library_info.pipeline_cache_path = u8"/shader_cache.cache";
		shader_library_info.library_path = u8"/assets/shaders";
		shader_library_info.backbuffer_format = swapchain.get_format();
		shader_library_ = gfx::ShaderLibrary::construct(shader_library_info);

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