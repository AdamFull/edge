#include "platform.h"

#include <allocator.hpp>
#include <logger.hpp>

#include <math.h>
#include <stdio.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

extern i32 edge_main(edge::PlatformLayout* platform_layout);

namespace edge {
	struct PlatformLayout {
		HINSTANCE hinst;
		HINSTANCE prev_hinst;
		PSTR cmd_line;
		INT cmd_show;
	};

	struct PlatformContext {
		const Allocator* alloc;
		PlatformLayout* layout;
		EventDispatcher* event_dispatcher;
	};

	PlatformContext* platform_context_create(PlatformContextCreateInfo create_info) {
		if (!create_info.alloc || !create_info.layout) {
			return nullptr;
		}

		PlatformContext* ctx = create_info.alloc->allocate<PlatformContext>();
		if (!ctx) {
			return nullptr;
		}

		ctx->alloc = create_info.alloc;
		ctx->layout = create_info.layout;
		ctx->event_dispatcher = create_info.event_dispatcher;

#if EDGE_DEBUG
		if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
			if (!AllocConsole()) {
				EDGE_LOG_DEBUG("Failed to allocate console.");
			}
		}

		HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
		DWORD dwMode = 0;

		GetConsoleMode(hOut, &dwMode);
		dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
		SetConsoleMode(hOut, dwMode);

		FILE* fp;
		freopen_s(&fp, "conin$", "r", stdin);
		freopen_s(&fp, "conout$", "w", stdout);
		freopen_s(&fp, "conout$", "w", stderr);
#endif
		
		Logger* logger = logger_get_global();
		LoggerOutput* debug_output = logger_create_debug_console_output(create_info.alloc, LogFormat::LogFormat_Default);
		logger_add_output(logger, debug_output);

		return ctx;
	}

	void platform_context_destroy(PlatformContext* ctx) {
		if (!ctx) {
			return;
		}

		const Allocator* alloc = ctx->alloc;
		alloc->deallocate(ctx);
	}
}

i32 APIENTRY WinMain(HINSTANCE hinst, HINSTANCE prev_hinst, PSTR cmd_line, INT cmd_show) {
	edge::PlatformLayout platform_layout = {
		.hinst = hinst,
		.prev_hinst = prev_hinst,
		.cmd_line = cmd_line,
		.cmd_show = cmd_show
	};

	return edge_main(&platform_layout);
}