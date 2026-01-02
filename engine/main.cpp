#include "runtime/runtime.h"
#include "event_dispatcher.h"

#include "gfx_context.h"
#include "gfx_renderer.h"
#include "gfx_uploader.h"

#include "imgui/imgui_layer.h"
#include "imgui/imgui_renderer.h"

#include <allocator.hpp>
#include <logger.hpp>
#include <scheduler.hpp>
#include <platform_detect.hpp>

#include <assert.h>
#include <mimalloc.h>

using namespace edge;

static Allocator allocator = {};

static Logger logger = {};
static Scheduler* sched = nullptr;

static EventDispatcher event_dispatcher = {};

static IRuntime* runtime = nullptr;

static gfx::Queue main_queue = {};
static gfx::Queue copy_queue = {};

static gfx::Renderer renderer = {};

static ImGuiLayer imgui_layer = {};
static gfx::ImGuiRenderer imgui_renderer = {};

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
	allocator = Allocator::create(mi_malloc, mi_free, mi_realloc, mi_calloc, mi_strdup);
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

	RuntimeInitInfo runtime_info = {
		.alloc = &allocator,
		.layout = runtime_layout,

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

	//const WindowCreateInfo window_create_info = {
	//	.alloc = &allocator,
	//	.event_dispatcher = &event_dispatcher,
    //    .platform_context = platform_context,
	//
	//	.title = "Window",
	//	.mode = WindowMode::Default,
	//	.resizable = true,
	//	.vsync_mode = WindowVsyncMode::Off,
	//	.width = 1280,
	//	.height = 720
	//};
	//
	//window = window_create(window_create_info);
	//if (!window) {
	//	edge_cleanup_engine();
	//	return -1;
	//}

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
		.runtime = runtime
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

	// TODO: Remove requested_close and return bool in process_events
    while (!runtime->requested_close()) {
		runtime->process_events();

		imgui_layer.update(0.1f);

		if (renderer.frame_begin()) {
			imgui_renderer.execute(&allocator);
			
			renderer.frame_end();
		}
    }

	edge_cleanup_engine();
	return 0;
}