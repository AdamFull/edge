#include "engine.h"
#include "event_dispatcher.h"

#include "gfx_context.h"
#include "gfx_renderer.h"

#include <edge_allocator.h>
#include <edge_testing.h>
#include <edge_logger.h>
#include <edge_scheduler.h>
#include <edge_platform_detect.h>

#include <assert.h>
#include <mimalloc.h>

static edge_engine_context_t engine_context = { 0 };

static void edge_cleanup_engine(void) {
	if (engine_context.gfx_renderer) {
		gfx_renderer_destroy(engine_context.gfx_renderer);
	}

	gfx_release_queue(&engine_context.gfx_main_queue);
	
	gfx_context_shutdown();

	if (engine_context.event_dispatcher) {
		event_dispatcher_destroy(engine_context.event_dispatcher);
	}

	if (engine_context.platform_context) {
		platform_context_destroy(engine_context.platform_context);
	}

	if (engine_context.sched) {
		edge_sched_destroy(engine_context.sched);
	}

	if (engine_context.logger) {
		edge_logger_destroy(engine_context.logger);
	}

	size_t net_allocated = edge_testing_net_allocated();
	assert(net_allocated == 0 && "Memory leaks detected.");
}

int edge_main(platform_layout_t* platform_layout) {
#if EDGE_DEBUG
	edge_allocator_t allocator = edge_testing_allocator_create();
#else
	edge_allocator_t allocator = edge_allocator_create(mi_malloc, mi_free, mi_realloc, mi_calloc, mi_strdup);
#endif

	engine_context.allocator = &allocator;
	engine_context.platform_layout = platform_layout;

	engine_context.logger = edge_logger_create(&allocator, EDGE_LOG_LEVEL_TRACE);
	if (!engine_context.logger) {
		goto fatal_error;
	}

	edge_logger_set_global(engine_context.logger);

	edge_logger_output_t* stdout_output = edge_logger_create_stdout_output(&allocator, EDGE_LOG_FORMAT_DEFAULT | EDGE_LOG_FORMAT_COLOR);
	edge_logger_add_output(engine_context.logger, stdout_output);

	edge_logger_output_t* file_output = edge_logger_create_file_output(&allocator, EDGE_LOG_FORMAT_DEFAULT, "log.log", false);
	edge_logger_add_output(engine_context.logger, file_output);

	engine_context.sched = edge_sched_create(&allocator);
	if (!engine_context.sched) {
		goto fatal_error;
	}

	engine_context.event_dispatcher = event_dispatcher_create(&allocator);
	if (!engine_context.event_dispatcher) {
		goto fatal_error;
	}

	platform_context_create_info_t platform_context_create_info = { 0 };
	platform_context_create_info.alloc = &allocator;
	platform_context_create_info.layout = platform_layout;
	platform_context_create_info.event_dispatcher = engine_context.event_dispatcher;

	engine_context.platform_context = platform_context_create(&platform_context_create_info);
	if (!engine_context.platform_context) {
		goto fatal_error;
	}

	EDGE_LOG_INFO("Context initialization finished.");
	edge_logger_flush(edge_logger_get_global());

	window_create_info_t window_create_info = { 0 };
	window_create_info.title = "Window";
	window_create_info.mode = WINDOW_MODE_DEFAULT;
	window_create_info.resizable = true;
	window_create_info.vsync_mode = WINDOW_VSYNC_MODE_OFF;
	window_create_info.width = 1280;
	window_create_info.height = 720;

	if (!platform_context_window_init(engine_context.platform_context, &window_create_info)) {
		goto fatal_error;
	}

	gfx_context_create_info_t gfx_cteate_info = { 0 };
	gfx_cteate_info.alloc = &allocator;
	gfx_cteate_info.platform_context = engine_context.platform_context;

	if (!gfx_context_init(&gfx_cteate_info)) {
		goto fatal_error;
	}

	gfx_queue_request_t queue_request;
	queue_request.required_caps = GFX_QUEUE_CAPS_GRAPHICS | GFX_QUEUE_CAPS_COMPUTE | GFX_QUEUE_CAPS_TRANSFER | GFX_QUEUE_CAPS_PRESENT;
	queue_request.preferred_caps = GFX_QUEUE_CAPS_NONE;
	queue_request.strategy = GFX_QUEUE_SELECTION_STRATEGY_PREFER_DEDICATED;
	queue_request.prefer_separate_family = false;

	if (!gfx_get_queue(&queue_request, &engine_context.gfx_main_queue)) {
		goto fatal_error;
	}

	gfx_renderer_create_info_t renderer_create_info;
	renderer_create_info.alloc = &allocator;
	renderer_create_info.main_queue = &engine_context.gfx_main_queue;

	engine_context.gfx_renderer = gfx_renderer_create(&renderer_create_info);
	if (!engine_context.gfx_renderer) {
		goto fatal_error;
	}

    while (!platform_context_window_should_close(engine_context.platform_context)) {
		platform_context_window_process_events(engine_context.platform_context, 0.1f);

		if (gfx_renderer_frame_begin(engine_context.gfx_renderer)) {

			gfx_renderer_frame_end(engine_context.gfx_renderer);
		}
    }

	edge_cleanup_engine();
	return 0;

fatal_error:
	edge_cleanup_engine();
	return -1;
}