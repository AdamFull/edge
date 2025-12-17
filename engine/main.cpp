#include "engine.h"
#include "event_dispatcher.h"

#include "gfx_context.h"
#include "gfx_renderer.h"

#include "imgui/imgui_layer.h"
#include "imgui/imgui_renderer.h"

#include <allocator.hpp>
#include <logger.hpp>
#include <scheduler.hpp>
#include <platform_detect.hpp>

#include <assert.h>
#include <mimalloc.h>

static edge::EngineContext engine_context = {};

static void edge_cleanup_engine(void) {
	if (engine_context.imgui_layer) {
		edge::imgui_layer_destroy(engine_context.imgui_layer);
	}

	if (engine_context.imgui_renderer) {
		edge::gfx::imgui_renderer_destroy(engine_context.imgui_renderer);
	}

	if (engine_context.renderer) {
		edge::gfx::renderer_destroy(engine_context.renderer);
	}

	edge::gfx::release_queue(&engine_context.main_queue);
	
	edge::gfx::context_shutdown();

	if (engine_context.event_dispatcher) {
		event_dispatcher_destroy(engine_context.event_dispatcher);
	}

	if (engine_context.platform_context) {
		platform_context_destroy(engine_context.platform_context);
	}

	if (engine_context.sched) {
		edge::sched_destroy(engine_context.sched);
	}

	if (engine_context.logger) {
		edge::logger_destroy(engine_context.logger);
	}

	size_t net_allocated = engine_context.allocator->get_net();
	assert(net_allocated == 0 && "Memory leaks detected.");
}

int edge_main(edge::PlatformLayout* platform_layout) {
#if EDGE_DEBUG
	edge::Allocator allocator = edge::Allocator::create_tracking();
#else
	edge::Allocator allocator = edge::Allocator::create(mi_malloc, mi_free, mi_realloc, mi_calloc, mi_strdup);
#endif

	engine_context.allocator = &allocator;
	engine_context.platform_layout = platform_layout;

	engine_context.logger = edge::logger_create(&allocator, edge::LogLevel::Trace);
	if (!engine_context.logger) {
		edge_cleanup_engine();
		return -1;
	}

	edge::logger_set_global(engine_context.logger);

	edge::LoggerOutput* stdout_output = edge::logger_create_stdout_output(&allocator, edge::LogFormat_Default | edge::LogFormat_Color);
	edge::logger_add_output(engine_context.logger, stdout_output);

	edge::LoggerOutput* file_output = edge::logger_create_file_output(&allocator, edge::LogFormat_Default, "log.log", false);
	edge::logger_add_output(engine_context.logger, file_output);

	engine_context.sched = edge::sched_create(&allocator);
	if (!engine_context.sched) {
		edge_cleanup_engine();
		return -1;
	}

	engine_context.event_dispatcher = event_dispatcher_create(&allocator);
	if (!engine_context.event_dispatcher) {
		edge_cleanup_engine();
		return -1;
	}

	edge::PlatformContextCreateInfo platform_context_create_info = {};
	platform_context_create_info.alloc = &allocator;
	platform_context_create_info.layout = platform_layout;
	platform_context_create_info.event_dispatcher = engine_context.event_dispatcher;

	engine_context.platform_context = platform_context_create(&platform_context_create_info);
	if (!engine_context.platform_context) {
		edge_cleanup_engine();
		return -1;
	}

	EDGE_LOG_INFO("Context initialization finished.");
	edge::logger_flush(edge::logger_get_global());

	edge::WindowCreateInfo window_create_info = {};
	window_create_info.title = "Window";
	window_create_info.mode = edge::WindowMode::Default;
	window_create_info.resizable = true;
	window_create_info.vsync_mode = edge::WindowVsyncMode::Off;
	window_create_info.width = 1280;
	window_create_info.height = 720;

	if (!platform_context_window_init(engine_context.platform_context, &window_create_info)) {
		edge_cleanup_engine();
		return -1;
	}

	edge::gfx::ContextCreateInfo gfx_cteate_info = {};
	gfx_cteate_info.alloc = &allocator;
	gfx_cteate_info.platform_context = engine_context.platform_context;

	if (!edge::gfx::context_init(&gfx_cteate_info)) {
		edge_cleanup_engine();
		return -1;
	}

	edge::gfx::QueueRequest queue_request;
	queue_request.required_caps = edge::gfx::QUEUE_CAPS_GRAPHICS | edge::gfx::QUEUE_CAPS_COMPUTE | edge::gfx::QUEUE_CAPS_TRANSFER | edge::gfx::QUEUE_CAPS_PRESENT;
	queue_request.preferred_caps = edge::gfx::QUEUE_CAPS_NONE;
	queue_request.strategy = edge::gfx::QUEUE_SELECTION_STRATEGY_PREFER_DEDICATED;
	queue_request.prefer_separate_family = false;

	if (!edge::gfx::get_queue(&queue_request, &engine_context.main_queue)) {
		edge_cleanup_engine();
		return -1;
	}

	edge::gfx::RendererCreateInfo renderer_create_info;
	renderer_create_info.alloc = &allocator;
	renderer_create_info.main_queue = &engine_context.main_queue;

	engine_context.renderer = edge::gfx::renderer_create(&renderer_create_info);
	if (!engine_context.renderer) {
		edge_cleanup_engine();
		return -1;
	}

	edge::ImGuiLayerInitInfo imgui_init_info = {
		.alocator = engine_context.allocator,
		.event_dispatcher = engine_context.event_dispatcher,
		.platform_context = engine_context.platform_context
	};

	engine_context.imgui_layer = edge::imgui_layer_create(&imgui_init_info);
	if (!engine_context.imgui_layer) {
		edge_cleanup_engine();
		return -1;
	}

	edge::gfx::ImGuiRendererCreateInfo imgui_renderer_create_info = {
		.allocator = engine_context.allocator,
		.renderer = engine_context.renderer
	};

	engine_context.imgui_renderer = edge::gfx::imgui_renderer_create(&imgui_renderer_create_info);
	if (!engine_context.imgui_renderer) {
		edge_cleanup_engine();
		return -1;
	}

    while (!platform_context_window_should_close(engine_context.platform_context)) {
		platform_context_window_process_events(engine_context.platform_context, 0.1f);

		edge::imgui_layer_update(engine_context.imgui_layer, 0.1f);

		if (edge::gfx::renderer_frame_begin(engine_context.renderer)) {

			edge::gfx::renderer_frame_end(engine_context.renderer);
		}
    }

	edge_cleanup_engine();
	return 0;
}