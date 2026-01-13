#include "main.h"

#include <allocator.hpp>
#include <logger.hpp>
#include <scheduler.hpp>
#include <platform_detect.hpp>

#include <assert.h>

#include <mimalloc.h>
#include <mimalloc-stats.h>

#include <math.hpp>

static edge::Allocator allocator = {};

static edge::Logger logger = {};
static edge::Scheduler* sched = nullptr;

namespace edge {
	bool FrameTimeController::create() noexcept {
#if EDGE_PLATFORM_WINDOWS
		waitable_timer = CreateWaitableTimer(NULL, FALSE, NULL);
		if (waitable_timer) {
			return true;
		}
#endif
		return false;
	}

	void FrameTimeController::destroy() {
#if EDGE_PLATFORM_WINDOWS
		CloseHandle(waitable_timer);
#endif
	}

	void FrameTimeController::set_limit(f64 target_frame_rate) noexcept {
		target_frame_time = std::chrono::duration_cast<std::chrono::high_resolution_clock::duration>(std::chrono::duration<f64>(1.0 / target_frame_rate));
		last_frame_time = std::chrono::high_resolution_clock::now();
	}

	void FrameTimeController::accurate_sleep(f64 seconds) noexcept {
		// Adaptive algorithm (same across all platforms)
		while (seconds - welford_estimate > 1e-7) {
			f64 to_wait = seconds - welford_estimate;

			auto start = std::chrono::high_resolution_clock::now();

#if EDGE_PLATFORM_ANDROID
			// Android nanosleep implementation
			// Use nanosleep for better precision on mobile devices
			struct timespec req, rem;
			req.tv_sec = static_cast<time_t>(to_wait);
			req.tv_nsec = static_cast<long>((to_wait - req.tv_sec) * 1e9);

			// Handle potential interruptions
			while (nanosleep(&req, &rem) == -1) {
				req = rem;
			}
#elif EDGE_PLATFORM_WINDOWS
			LARGE_INTEGER due;
			due.QuadPart = -i64(to_wait * 1e7);  // Convert to 100ns units
			SetWaitableTimerEx(waitable_timer, &due, 0, NULL, NULL, NULL, 0);
			WaitForSingleObject(waitable_timer, INFINITE);
#else
			// TODO: Linux
			std::this_thread::sleep_for(std::chrono::duration<f64>(to_wait));
#endif
			auto end = std::chrono::high_resolution_clock::now();

			f64 observed = std::chrono::duration<f64>(end - start).count();
			seconds -= observed;

			// Update statistics using Welford's online algorithm
			++welford_count;
			f64 error = observed - to_wait;
			f64 delta = error - welford_mean;
			welford_mean += delta / welford_count;
			welford_m2 += delta * (error - welford_mean);
			f64 stddev = welford_count > 1 ? sqrt(welford_m2 / (welford_count - 1)) : 0.0;
			welford_estimate = welford_mean + stddev;
		}

		// Spin lock for remaining time (cross-platform)
		auto start = std::chrono::high_resolution_clock::now();
		auto spin_duration = std::chrono::duration<f64>(seconds);
		while (std::chrono::high_resolution_clock::now() - start < spin_duration) {
			// Tight spin loop for maximum precision
		}
	}


	bool EngineContext::create(NotNull<const Allocator*> alloc, NotNull<RuntimeLayout*> runtime_layout) noexcept {
		if (!event_dispatcher.create(alloc)) {
			EDGE_LOG_FATAL("Failed to initialize EventDispatcher.");
			return false;
		}
		EDGE_LOG_INFO("EventDispatcher initialized.");

		if (!input_system.create(&allocator)) {
			EDGE_LOG_FATAL("Failed to initialize InputSystem.");
			return false;
		}
		EDGE_LOG_INFO("InputSystem initialized.");

		RuntimeInitInfo runtime_info = {
		.alloc = &allocator,
		.layout = runtime_layout.m_ptr,
		.input_system = &input_system,

		.title = "Vulkan",
		.width = 1920,
		.height = 1080
		};

		runtime = create_runtime(&allocator);
		if (!runtime || !runtime->init(runtime_info)) {
			EDGE_LOG_FATAL("Failed to initialize engine runtime.");
			return false;
		}
		EDGE_LOG_INFO("Engine runtime initialized.");

		const gfx::ContextCreateInfo gfx_cteate_info = {
		.alloc = &allocator,
		.runtime = runtime
		};

		if (!gfx::context_init(&gfx_cteate_info)) {
			EDGE_LOG_FATAL("Failed to initialize graphics context.");
			return false;
		}
		EDGE_LOG_INFO("Graphics initialized.");

		const gfx::QueueRequest direct_queue_request = {
		.required_caps = gfx::QUEUE_CAPS_GRAPHICS | gfx::QUEUE_CAPS_COMPUTE | gfx::QUEUE_CAPS_TRANSFER | gfx::QUEUE_CAPS_PRESENT,
		.preferred_caps = gfx::QUEUE_CAPS_NONE,
		.strategy = gfx::QUEUE_SELECTION_STRATEGY_PREFER_DEDICATED,
		.prefer_separate_family = false
		};

		if (!main_queue.request(direct_queue_request)) {
			EDGE_LOG_FATAL("Failed to find direct queue.");
			return false;
		}
		EDGE_LOG_INFO("Direct queue found.");

		// NOTE: Optional, not required
		const gfx::QueueRequest copy_queue_request = {
			.required_caps = gfx::QUEUE_CAPS_TRANSFER,
			.strategy = gfx::QUEUE_SELECTION_STRATEGY_PREFER_DEDICATED,
			.prefer_separate_family = false
		};
		if (copy_queue.request(copy_queue_request)) {
			EDGE_LOG_INFO("Copy queue found.");
		}

		const gfx::RendererCreateInfo renderer_create_info = {
		.alloc = &allocator,
		.main_queue = main_queue
		};

		if (!renderer.create(renderer_create_info)) {
			EDGE_LOG_FATAL("Failed to initialize main renderer context.");
			return false;
		}

		const ImGuiLayerInitInfo imgui_init_info = {
		.alloc = &allocator,
		.runtime = runtime,
		.input_system = &input_system
		};

		// TODO: This should be optional in future
		if (!imgui_layer.create(imgui_init_info)) {
			EDGE_LOG_FATAL("Failed to initialize ImGuiLayer.");
			return false;
		}
		EDGE_LOG_INFO("ImGuiLayer initialized.");

		const gfx::ImGuiRendererCreateInfo imgui_renderer_create_info = {
			.alloc = &allocator,
			.renderer = &renderer
		};

		// TODO: This should be optional in future
		if (!imgui_renderer.create(imgui_renderer_create_info)) {
			EDGE_LOG_FATAL("Failed to initialize ImGuiRenderer.");
			return false;
		}
		EDGE_LOG_INFO("ImGuiRenderer initialized.");

		if (!frame_time_controller.create()) {
			EDGE_LOG_FATAL("Failed to initialize FrameTimeController.");
			return false;
		}

		return true;
	}

	void EngineContext::destroy(NotNull<const Allocator*> alloc) noexcept {
		// Wait for all work done
		if (main_queue) {
			main_queue.wait_idle();
		}
		
		if (copy_queue) {
			copy_queue.wait_idle();
		}

		frame_time_controller.destroy();

		imgui_layer.destroy(alloc);
		imgui_renderer.destroy(alloc);
		renderer.destroy(alloc);

		if (copy_queue) {
			copy_queue.release();
		}

		if (main_queue) {
			main_queue.release();
		}

		gfx::context_shutdown();

		if (runtime) {
			runtime->deinit(alloc);
			alloc->deallocate(runtime);
			runtime = nullptr;
		}

		input_system.destroy(&allocator);
		event_dispatcher.destroy(alloc);
	}

	bool EngineContext::run() noexcept {
		while (!runtime->requested_close()) {
			frame_time_controller.process([this](f32 delta_time) -> void { tick(delta_time); });
		}

		return true;
	}

	void EngineContext::tick(f32 delta_time) noexcept {
		runtime->process_events();
		input_system.update();

		imgui_layer.update(delta_time);

		if (renderer.frame_begin()) {
#if 0
			[&]() {
				if (!texture_uploaded) {
					gfx::ImageCreateInfo create_info = {
					.extent = {
							.width = tex_source.base_width,
							.height = tex_source.base_height,
							.depth = tex_source.base_depth
						},
						.level_count = tex_source.mip_levels,
						.layer_count = tex_source.array_layers,
						.face_count = tex_source.face_count,
					.usage_flags = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
					.format = static_cast<VkFormat>(tex_source.format_info->vk_format)
					};

					gfx::Image image = {};
					if (!image.create(create_info)) {
						EDGE_LOG_ERROR("Failed to create font image.");
						texture_uploaded = true;
						tex_source.destroy(&allocator);
						return;
					}

					gfx::CmdBuf cmd = renderer.active_frame->cmd;

					gfx::PipelineBarrierBuilder barrier_builder = {};

					barrier_builder.add_image(image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, {
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.baseMipLevel = 0,
						.levelCount = tex_source.mip_levels,
						.baseArrayLayer = 0,
						.layerCount = tex_source.array_layers * tex_source.face_count
						});
					cmd.pipeline_barrier(barrier_builder);
					image.layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

					gfx::ImageUpdateInfo update_info = {
						.dst_image = image,
						.buffer_view = renderer.active_frame->try_allocate_staging_memory(&allocator, tex_source.data_size, 1)
					};

					for (usize level = 0; level < tex_source.mip_levels; ++level) {
						u32 mip_width = max(tex_source.base_width >> level, 1u);
						u32 mip_height = max(tex_source.base_height >> level, 1u);
						u32 mip_depth = max(tex_source.base_depth >> level, 1u);

						auto subresource_info = tex_source.get_mip(level);

						update_info.write(&allocator, {
									.data = { subresource_info.data, subresource_info.size },
									.extent = {
									.width = mip_width,
									.height = mip_height,
									.depth = mip_depth
									},
									.mip_level = (u32)level,
									.array_layer = 0,
									.layer_count = tex_source.array_layers * tex_source.face_count
							});
					}

					renderer.image_update_end(&allocator, update_info);
					renderer.setup_resource(&allocator, tex_handle, image);

					tex_source.destroy(&allocator);
					texture_uploaded = true;
				}
				}();
#endif

			imgui_renderer.execute(&allocator);

			renderer.frame_end();
		}
	}
}

using namespace edge;

int edge_main(RuntimeLayout* runtime_layout) {
	int return_value = 0;

#if EDGE_DEBUG
	allocator = Allocator::create_tracking();
#else
	allocator = Allocator::create(
		[](usize size, usize alignment, void*) { return mi_aligned_alloc(alignment, size); },
		[](void* ptr, void*) { mi_free(ptr); },
		[](void* ptr, usize size, usize alignment, void*) { return mi_aligned_recalloc(ptr, 1, size, alignment); },
		nullptr);
#endif

	if (!logger.create(&allocator, LogLevel::Trace)) {
		return_value = -1;
		goto cleanup;
	}

	logger_set_global(&logger);

	ILoggerOutput* stdout_output = logger_create_stdout_output(&allocator, LogFormat_Default | LogFormat_Color);
	logger.add_output(&allocator, stdout_output);

	ILoggerOutput* file_output = logger_create_file_output(&allocator, LogFormat_Default, "log.log", false);
	logger.add_output(&allocator, file_output);

	sched = sched_create(&allocator);
	if (!sched) {
		EDGE_LOG_FATAL("Failed to initialize Scheduler.");
		return_value = -1;
		goto cleanup;
	}

	edge::EngineContext engine = {};
	if (!engine.create(&allocator, runtime_layout)) {
		return_value = -1;
		goto cleanup;
	}

	engine.run();

cleanup:
	if (sched) {
		sched_destroy(sched);
	}

	logger.destroy(&allocator);

	{
		size_t net_allocated = allocator.get_net();
		assert(net_allocated == 0 && "Memory leaks detected.");
	}

	engine.destroy(&allocator);
	return return_value;
}