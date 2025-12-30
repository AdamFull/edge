#include "engine.h"
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

static EngineContext engine_context = {};

static void edge_cleanup_engine(void) {
	engine_context.main_queue.wait_idle();

	if (engine_context.imgui_layer) {
		imgui_layer_destroy(engine_context.imgui_layer);
	}

	if (engine_context.imgui_renderer) {
		gfx::imgui_renderer_destroy(engine_context.imgui_renderer);
	}

	if (engine_context.uploader) {
		gfx::uploader_destroy(engine_context.allocator, engine_context.uploader);
	}

	if (engine_context.renderer) {
		gfx::renderer_destroy(engine_context.renderer);
	}

	engine_context.main_queue.release();
	
	gfx::context_shutdown();

	if (engine_context.event_dispatcher) {
		event_dispatcher_destroy(engine_context.event_dispatcher);
	}

	if (engine_context.window) {
		window_destroy(engine_context.allocator, engine_context.window);
	}

	if (engine_context.platform_context) {
		platform_context_destroy(engine_context.platform_context);
	}

	if (engine_context.sched) {
		sched_destroy(engine_context.sched);
	}

	if (engine_context.logger) {
		logger_destroy(engine_context.logger);
	}

	size_t net_allocated = engine_context.allocator->get_net();
	assert(net_allocated == 0 && "Memory leaks detected.");
}

int edge_main(PlatformLayout* platform_layout) {
#if EDGE_DEBUG
	Allocator allocator = Allocator::create_tracking();
#else
	Allocator allocator = Allocator::create(mi_malloc, mi_free, mi_realloc, mi_calloc, mi_strdup);
#endif

	engine_context.allocator = &allocator;
	engine_context.platform_layout = platform_layout;

	engine_context.logger = logger_create(&allocator, LogLevel::Trace);
	if (!engine_context.logger) {
		edge_cleanup_engine();
		return -1;
	}

	logger_set_global(engine_context.logger);

	LoggerOutput* stdout_output = logger_create_stdout_output(&allocator, LogFormat_Default | LogFormat_Color);
	logger_add_output(engine_context.logger, stdout_output);

	LoggerOutput* file_output = logger_create_file_output(&allocator, LogFormat_Default, "log.log", false);
	logger_add_output(engine_context.logger, file_output);

	engine_context.sched = sched_create(&allocator);
	if (!engine_context.sched) {
		edge_cleanup_engine();
		return -1;
	}

	engine_context.event_dispatcher = event_dispatcher_create(&allocator);
	if (!engine_context.event_dispatcher) {
		edge_cleanup_engine();
		return -1;
	}

	const PlatformContextCreateInfo platform_context_create_info = {
		.alloc = &allocator,
		.layout = platform_layout,
		.event_dispatcher = engine_context.event_dispatcher
	};

	engine_context.platform_context = platform_context_create(platform_context_create_info);
	if (!engine_context.platform_context) {
		edge_cleanup_engine();
		return -1;
	}

	EDGE_LOG_INFO("Context initialization finished.");
	logger_flush(logger_get_global());

	const WindowCreateInfo window_create_info = {
		.alloc = &allocator,
		.event_dispatcher = engine_context.event_dispatcher,

		.title = "Window",
		.mode = WindowMode::Default,
		.resizable = true,
		.vsync_mode = WindowVsyncMode::Off,
		.width = 1280,
		.height = 720
	};

	engine_context.window = window_create(window_create_info);
	if (!engine_context.window) {
		edge_cleanup_engine();
		return -1;
	}

	const gfx::ContextCreateInfo gfx_cteate_info = {
		.alloc = &allocator,
		.platform_context = engine_context.platform_context,
		.window = engine_context.window
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

	if (!engine_context.main_queue.request(direct_queue_request)) {
		edge_cleanup_engine();
		return -1;
	}

	// NOTE: Optional, not required
	const gfx::QueueRequest copy_queue_request = {
		.required_caps = gfx::QUEUE_CAPS_TRANSFER,
		.strategy = gfx::QUEUE_SELECTION_STRATEGY_PREFER_DEDICATED,
		.prefer_separate_family = false
	};
	engine_context.copy_queue.request(copy_queue_request);

	const gfx::RendererCreateInfo renderer_create_info = {
		.alloc = &allocator,
		.main_queue = engine_context.main_queue
	};

	engine_context.renderer = gfx::renderer_create(renderer_create_info);
	if (!engine_context.renderer) {
		edge_cleanup_engine();
		return -1;
	}

	const gfx::UploaderCreateInfo uploader_create_info = {
		.alloc = &allocator,
		.sched = engine_context.sched,
		.queue = engine_context.copy_queue ? engine_context.copy_queue : engine_context.main_queue
	};

	engine_context.uploader = gfx::uploader_create(uploader_create_info);
	if (!engine_context.uploader) {
		edge_cleanup_engine();
		return -1;
	}

	const ImGuiLayerInitInfo imgui_init_info = {
		.alocator = engine_context.allocator,
		.event_dispatcher = engine_context.event_dispatcher,
		.platform_context = engine_context.platform_context,
		.window = engine_context.window
	};

	engine_context.imgui_layer = imgui_layer_create(imgui_init_info);
	if (!engine_context.imgui_layer) {
		edge_cleanup_engine();
		return -1;
	}

	const gfx::ImGuiRendererCreateInfo imgui_renderer_create_info = {
		.renderer = engine_context.renderer
	};

	engine_context.imgui_renderer = gfx::imgui_renderer_create(imgui_renderer_create_info);
	if (!engine_context.imgui_renderer) {
		edge_cleanup_engine();
		return -1;
	}

    while (!window_should_close(engine_context.window)) {
		window_process_events(engine_context.window, 0.1f);

		imgui_layer_update(engine_context.imgui_layer, 0.1f);

		if (engine_context.renderer->frame_begin()) {
			engine_context.imgui_renderer->execute();
			
			engine_context.renderer->frame_end();
		}
    }

	edge_cleanup_engine();
	return 0;
}