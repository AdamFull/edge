#include "platform.h"

#include <print>
#include <thread>

namespace edge::platform {
	PlatformContextInterface::~PlatformContextInterface() {
		event_dispatcher_->clear_events();
	}

	auto PlatformContextInterface::initialize(const PlatformCreateInfo& create_info) -> bool {
		if (!window_->create(create_info.window_props)) {
			window_.reset();
			return false;
		}

		event_dispatcher_ = std::make_unique<events::Dispatcher>();

		any_window_event_listener_ = event_dispatcher_->add_listener<events::EventTag::eWindow>(
			[](const events::Dispatcher::event_variant_t& e, void* user_ptr) {
				auto* self = static_cast<PlatformContextInterface*>(user_ptr);
				self->on_any_window_event(e);
			}, this);

		frame_handler_.set_limit(60);
		frame_handler_.setup_callback([](float delta_time, void* user_data) -> int32_t {
			auto* self = static_cast<PlatformContextInterface*>(user_data);
			return self->main_loop_tick(delta_time);
			}, this);

		return true;
	}

	auto PlatformContextInterface::setup_application(void(*app_setup_func)(std::unique_ptr<ApplicationInterface>&)) -> bool {
		app_setup_func(application_);

		if (!application_) {
			return false;
		}

		return application_->initialize();
	}

	auto PlatformContextInterface::terminate(int32_t code) -> void {
		if (application_) {
			application_->finish();
		}

		application_.reset();
		window_.reset();
		event_dispatcher_.reset();
	}

	auto PlatformContextInterface::main_loop() -> int32_t {
		int32_t exit_code = 0;
		while (!window_->requested_close()) {
			exit_code = frame_handler_.process();
			// TODO: Frame mark
		}
		return exit_code;
	}

	auto PlatformContextInterface::main_loop_tick(float delta_time) -> int32_t {
		std::println("Frame time: {:.2f}", delta_time * 1000.f);
		auto new_windows_name = std::format("{} [{} fps; {:.2f} ms]", "Application", frame_handler_.get_fps(), frame_handler_.get_mean_frame_time());
		window_->set_title(new_windows_name);

		window_->poll_events();

		if (window_focused_) {
			accumulated_delta_time_ += delta_time;
			while (accumulated_delta_time_ > fixed_delta_time_) {
				application_->fixed_update(fixed_delta_time_);
				accumulated_delta_time_ -= fixed_delta_time_;
			}
		
			application_->update(delta_time);
		}
		else {
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}

		event_dispatcher_->process_events();

		return 0;
	}

	auto PlatformContextInterface::on_any_window_event(const events::Dispatcher::event_variant_t& event) -> void {
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