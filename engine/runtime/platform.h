#ifndef PLATFORM_CONTEXT_H
#define PLATFORM_CONTEXT_H

#include <stddef.hpp>

namespace edge {
	struct Allocator;
	struct EventDispatcher;

	struct PlatformLayout;
	struct PlatformContext;

	struct PlatformContextCreateInfo {
		const Allocator* alloc;
		PlatformLayout* layout;
		EventDispatcher* event_dispatcher;
	};

	enum class WindowMode {
		Fullscreen,
		FullscreenBorderless,
		Default
	};

	enum class WindowVsyncMode {
		Off,
		On, 
		Default
	};

	struct WindowCreateInfo {
		const char* title;
		WindowMode mode;
		bool resizable;
		WindowVsyncMode vsync_mode;

		i32 width;
		i32 height;
	};

	PlatformContext* platform_context_create(PlatformContextCreateInfo create_info);
	void platform_context_destroy(PlatformContext* ctx);

	void platform_context_get_surface(NotNull<PlatformContext*> ctx, void* surface_info);

	bool platform_context_window_init(NotNull<PlatformContext*> ctx, WindowCreateInfo create_info);

	bool platform_context_window_should_close(NotNull<PlatformContext*> ctx);
	void platform_context_window_process_events(NotNull<PlatformContext*> ctx, f32 delta_time);

	void platform_context_window_show(NotNull<PlatformContext*> ctx);
	void platform_context_window_hide(NotNull<PlatformContext*> ctx);

	void platform_context_window_set_title(NotNull<PlatformContext*> ctx, const char* title);

	void platform_context_window_get_size(NotNull<PlatformContext*> ctx, i32* width, i32* height);

	f32 platform_context_window_dpi_scale_factor(NotNull<PlatformContext*> ctx);
	f32 platform_context_window_content_scale_factor(NotNull<PlatformContext*> ctx);
}

#endif //PLATFORM_CONTEXT_H