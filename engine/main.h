#ifndef MAIN_H
#define MAIN_H

#include <allocator.hpp>

#include "event_dispatcher.h"
#include "runtime/input_system.h"
#include "runtime/runtime.h"

#include "graphics/gfx_context.h"
#include "graphics/gfx_renderer.h"
#include "graphics/gfx_uploader.h"

#include "imgui_layer.h"
#include "graphics/renderers/imgui_renderer.h"

namespace edge {
	struct FrameTimeController {
		using duration_t = std::chrono::high_resolution_clock::duration;
		using timepoint_t = std::chrono::high_resolution_clock::time_point;

		duration_t target_frame_time = {};
		timepoint_t last_frame_time = {};
		timepoint_t prev_time = {};
		bool first_frame = true;

		f64 welford_estimate = 5e-3;
		f64 welford_mean = 5e-3;
		f64 welford_m2 = 0.0;
		i64 welford_count = 1;

		f32 frame_time_accumulator = 0.0f;
		u32 frame_counter = 0;
		u32 mean_fps = 0;
		f32 mean_frame_time = 0.0f;

		bool create() noexcept;
		void destroy();

		void set_limit(f64 target_frame_rate) noexcept;

		template<typename F>
		void process(F&& fn) noexcept {
			timepoint_t target_time = last_frame_time + target_frame_time;
			timepoint_t current_time = std::chrono::high_resolution_clock::now();

			f32 delta_time = std::chrono::duration<f32>(current_time - prev_time).count();

			if (frame_time_accumulator > 1.0f) {
				mean_fps = frame_counter;
				mean_frame_time = frame_time_accumulator / static_cast<f32>(frame_counter);

				frame_time_accumulator = 0.0f;
				frame_counter = 0;
			}
			else {
				frame_time_accumulator += delta_time;
				frame_counter += 1;
			}

			fn(delta_time);

			if (first_frame) {
				prev_time = last_frame_time = std::chrono::high_resolution_clock::now();
				first_frame = false;
				return;
			}

			if (current_time < target_time) {
				std::chrono::nanoseconds remaining_duration = target_time - current_time;
				f64 remaining_seconds = std::chrono::duration<f64>(remaining_duration).count();
				accurate_sleep(remaining_seconds);
			}

			last_frame_time = target_time;
			prev_time = current_time;
		}

	private:
		void accurate_sleep(f64 seconds) noexcept;

#if EDGE_PLATFORM_WINDOWS
		HANDLE waitable_timer = INVALID_HANDLE_VALUE;
#endif
	};

	struct EngineContext {
		EventDispatcher event_dispatcher = {};
		InputSystem input_system = {};

		IRuntime* runtime = nullptr;

		gfx::Queue main_queue = {};
		gfx::Queue copy_queue = {};
		gfx::Renderer renderer = {};
		
		ImGuiLayer imgui_layer = {};
		gfx::ImGuiRenderer imgui_renderer = {};

		FrameTimeController frame_time_controller = {};

		Handle test_tex = HANDLE_INVALID;

		bool create(NotNull<const Allocator*> alloc, NotNull<RuntimeLayout*> runtime_layout) noexcept;
		void destroy(NotNull<const Allocator*> alloc) noexcept;

		bool run() noexcept;
	private:
		void tick(f32 delta_time) noexcept;
	};
}

#endif