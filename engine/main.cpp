#include "runtime/runtime.h"
#include "runtime/input_system.h"
#include "event_dispatcher.h"

#include "graphics/gfx_context.h"
#include "graphics/gfx_renderer.h"
#include "graphics/gfx_uploader.h"

#include "imgui_layer.h"
#include "graphics/renderers/imgui_renderer.h"

#include <allocator.hpp>
#include <logger.hpp>
#include <scheduler.hpp>
#include <platform_detect.hpp>

#include <assert.h>

#include <mimalloc.h>
#include <mimalloc-stats.h>

#include <math.hpp>

using duration_t = std::chrono::high_resolution_clock::duration;
using timepoint_t = std::chrono::high_resolution_clock::time_point;

static duration_t target_frame_time = {};
timepoint_t last_frame_time = {};
timepoint_t prev_time = {};
bool first_frame = true;

static f64 welford_estimate = 5e-3;
static f64 welford_mean = 5e-3;
static f64 welford_m2 = 0.0;
static i64 welford_count = 1;

static f32 frame_time_accumulator = 0.0f;
static u32 frame_counter = 0;
static u32 mean_fps = 0;
static f32 mean_frame_time = 0.0f;

using namespace edge;

static Allocator allocator = {};

static Logger logger = {};
static Scheduler* sched = nullptr;

static EventDispatcher event_dispatcher = {};
static InputSystem input_system = {};

static IRuntime* runtime = nullptr;

static gfx::Queue main_queue = {};
static gfx::Queue copy_queue = {};

static gfx::Renderer renderer = {};

static ImGuiLayer imgui_layer = {};
static gfx::ImGuiRenderer imgui_renderer = {};

static void accurate_sleep(f64 seconds) {
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
		HANDLE waitable_timer = CreateWaitableTimer(NULL, FALSE, NULL);

		LARGE_INTEGER due;
		due.QuadPart = -i64(to_wait * 1e7);  // Convert to 100ns units
		SetWaitableTimerEx(waitable_timer, &due, 0, NULL, NULL, NULL, 0);
		WaitForSingleObject(waitable_timer, INFINITE);

		CloseHandle(waitable_timer);
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

static void edge_cleanup_engine(void) {
	main_queue.wait_idle();

	imgui_layer.destroy(&allocator);
	imgui_renderer.destroy(&allocator);
	renderer.destroy(&allocator);
	main_queue.release();

	gfx::context_shutdown();

	event_dispatcher.destroy(&allocator);

	if (runtime) {
		runtime->deinit(&allocator);
		allocator.deallocate(runtime);
	}

	input_system.destroy(&allocator);

	if (sched) {
		sched_destroy(sched);
	}

	logger.destroy(&allocator);

	size_t net_allocated = allocator.get_net();
	assert(net_allocated == 0 && "Memory leaks detected.");
}

int edge_main(RuntimeLayout* runtime_layout) {
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
		edge_cleanup_engine();
		return -1;
	}

	logger_set_global(&logger);

	ILoggerOutput* stdout_output = logger_create_stdout_output(&allocator, LogFormat_Default | LogFormat_Color);
	logger.add_output(&allocator, stdout_output);

	ILoggerOutput* file_output = logger_create_file_output(&allocator, LogFormat_Default, "log.log", false);
	logger.add_output(&allocator, file_output);

	sched = sched_create(&allocator);
	if (!sched) {
		edge_cleanup_engine();
		return -1;
	}

	if (!event_dispatcher.create(&allocator)) {
		edge_cleanup_engine();
		return -1;
	}

	if (!input_system.create(&allocator)) {
		edge_cleanup_engine();
		return -1;
	}

	RuntimeInitInfo runtime_info = {
		.alloc = &allocator,
		.layout = runtime_layout,
		.input_system = &input_system,

		.title = "Vulkan",
		.width = 1920,
		.height = 1080
	};

	runtime = create_runtime(&allocator);
	if (!runtime || !runtime->init(runtime_info)) {
		edge_cleanup_engine();
		return -1;
	}

	EDGE_LOG_INFO("Context initialization finished.");
	logger.flush();

	const gfx::ContextCreateInfo gfx_cteate_info = {
		.alloc = &allocator,
		.runtime = runtime
	};

	if (!gfx::context_init(&gfx_cteate_info)) {
		edge_cleanup_engine();
		return -1;
	}

	const gfx::QueueRequest direct_queue_request = {
		.required_caps = gfx::QUEUE_CAPS_GRAPHICS | gfx::QUEUE_CAPS_COMPUTE | gfx::QUEUE_CAPS_TRANSFER | gfx::QUEUE_CAPS_PRESENT,
		.preferred_caps = gfx::QUEUE_CAPS_NONE,
		.strategy = gfx::QUEUE_SELECTION_STRATEGY_PREFER_DEDICATED,
		.prefer_separate_family = false
	};

	if (!main_queue.request(direct_queue_request)) {
		edge_cleanup_engine();
		return -1;
	}

	// NOTE: Optional, not required
	const gfx::QueueRequest copy_queue_request = {
		.required_caps = gfx::QUEUE_CAPS_TRANSFER,
		.strategy = gfx::QUEUE_SELECTION_STRATEGY_PREFER_DEDICATED,
		.prefer_separate_family = false
	};
	copy_queue.request(copy_queue_request);

	const gfx::RendererCreateInfo renderer_create_info = {
		.alloc = &allocator,
		.main_queue = main_queue
	};

	if (!renderer.create(renderer_create_info)) {
		edge_cleanup_engine();
		return -1;
	}

	const gfx::UploaderCreateInfo uploader_create_info = {
		.alloc = &allocator,
		.sched = sched,
		.queue = copy_queue ? copy_queue : main_queue
	};

	//engine_context.uploader = gfx::uploader_create(uploader_create_info);
	//if (!engine_context.uploader) {
	//	edge_cleanup_engine();
	//	return -1;
	//}

	const ImGuiLayerInitInfo imgui_init_info = {
		.alloc = &allocator,
		.runtime = runtime,
		.input_system = &input_system
	};

	if (!imgui_layer.create(imgui_init_info)) {
		edge_cleanup_engine();
		return -1;
	}

	const gfx::ImGuiRendererCreateInfo imgui_renderer_create_info = {
		.alloc = &allocator,
		.renderer = &renderer
	};

	if (!imgui_renderer.create(imgui_renderer_create_info)) {
		edge_cleanup_engine();
		return -1;
	}

	// TODO: Move frame limiter somewhere 
	target_frame_time = std::chrono::duration_cast<std::chrono::high_resolution_clock::duration>(std::chrono::duration<f64>(1.0 / static_cast<f64>(60)));
	last_frame_time = std::chrono::high_resolution_clock::now();

	// TODO: Remove requested_close and return bool in process_events
	while (!runtime->requested_close()) {
		auto target_time = last_frame_time + target_frame_time;
		auto current_time = std::chrono::high_resolution_clock::now();

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

			if (first_frame) {
				prev_time = last_frame_time = std::chrono::high_resolution_clock::now();
				first_frame = false;
				continue;
			}

			if (current_time < target_time) {
				auto remaining_duration = target_time - current_time;
				f64 remaining_seconds = std::chrono::duration<f64>(remaining_duration).count();
				accurate_sleep(remaining_seconds);
			}

			last_frame_time = target_time;
			prev_time = current_time;
		}
	}

	edge_cleanup_engine();
	return 0;
}