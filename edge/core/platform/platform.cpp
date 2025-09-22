#include "platform.h"

#include <spdlog/spdlog.h>
#include <thread>

namespace edge::platform {
	IPlatformContext::~IPlatformContext() {
		event_dispatcher_->clear_events();
	}

	auto IPlatformContext::initialize(const PlatformCreateInfo& create_info) -> bool {
        event_dispatcher_ = std::make_unique<events::Dispatcher>();

        any_window_event_listener_ = event_dispatcher_->add_listener<events::EventTag::eWindow>(
                [](const events::Dispatcher::event_variant_t& e, void* user_ptr) {
                    auto* self = static_cast<IPlatformContext*>(user_ptr);
                    self->on_any_window_event(e);
                }, this);

        //frame_handler_.set_limit(60);
        frame_handler_.setup_callback([](float delta_time, void* user_data) -> int32_t {
            auto* self = static_cast<IPlatformContext*>(user_data);
            return self->main_loop_tick(delta_time);
        }, this);

		if (!window_->create(create_info.window_props)) {
			window_.reset();
			spdlog::error("[Platform Context]: Failed to create window context.");
			return false;
		}

        do {
            window_->poll_events();
        } while (!window_->is_visible());

        if(!input_->create()) {
			input_.reset();
			spdlog::error("[Platform Context]: Failed to create input context.");
            return false;
        }

		auto gfx_creation_result = gfx::Context::construct(gfx::ContextInfo{
			.preferred_device_type = vk::PhysicalDeviceType::eDiscreteGpu,
			.window = window_.get()
			});

		if (!gfx_creation_result) {
			EDGE_LOGE("Failed to create graphics context. Reason: {}.", vk::to_string(gfx_creation_result.error()));
			return false;
		}

		gfx::RendererCreateInfo renderer_create_info{};
		renderer_create_info.enable_hdr = true;
		renderer_create_info.enable_vsync = false;

		auto gfx_renderer_result = gfx::Renderer::construct(std::move(gfx_creation_result.value()), renderer_create_info);
		if (!gfx_renderer_result) {
			EDGE_LOGE("Failed to create renderer. Reason: {}.", vk::to_string(gfx_renderer_result.error()));
			return false;
		}

		renderer_ = std::move(gfx_renderer_result.value());

		return true;
	}

	auto IPlatformContext::setup_application(void(*app_setup_func)(std::unique_ptr<ApplicationInterface>&)) -> bool {
		app_setup_func(application_);

		if (!application_) {
			return false;
		}

		return application_->initialize();
	}

	auto IPlatformContext::terminate(int32_t code) -> void {
		if (application_) {
			application_->finish();
		}

		application_.reset();
		window_.reset();
		event_dispatcher_.reset();
	}

	auto IPlatformContext::main_loop() -> int32_t {
		int32_t exit_code = 0;
		while (!window_->requested_close()) {
			exit_code = frame_handler_.process();
			// TODO: Frame mark
		}
		return exit_code;
	}

	auto IPlatformContext::main_loop_tick(float delta_time) -> int32_t {
		//spdlog::trace("[Android Platform] Frame time: {:.2f}", delta_time * 1000.f);
		auto new_windows_name = std::format("{} [cpu {} fps; {:.2f} ms] [gpu {:.2f} ms]", "Application", frame_handler_.get_fps(), frame_handler_.get_mean_frame_time(), renderer_->get_gpu_delta_time());
		window_->set_title(new_windows_name);

		window_->poll_events();

		if (window_focused_) {
			//accumulated_delta_time_ += delta_time;
			//while (accumulated_delta_time_ > fixed_delta_time_) {
			//	application_->fixed_update(fixed_delta_time_);
			//	accumulated_delta_time_ -= fixed_delta_time_;
			//}

			renderer_->begin_frame(delta_time);
		
			application_->update(delta_time);

			renderer_->end_frame();
		}
		else {
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}

		event_dispatcher_->process_events();

		return 0;
	}

	auto IPlatformContext::on_any_window_event(const events::Dispatcher::event_variant_t& event) -> void {
		std::visit([this](const auto& e) {
			using EventType = std::decay_t<decltype(e)>;
			if constexpr (std::same_as<EventType, events::WindowShouldCloseEvent>) {
				
			}
			else if constexpr (std::same_as<EventType, events::WindowSizeChangedEvent>) {

			}
			else if constexpr (std::same_as<EventType, events::WindowFocusChangedEvent>) {
				window_focused_ = e.focused;
			}
			}, event);
	}
}