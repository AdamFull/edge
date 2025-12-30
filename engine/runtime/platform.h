#ifndef PLATFORM_CONTEXT_H
#define PLATFORM_CONTEXT_H

#include <stddef.hpp>

namespace edge {
	struct Allocator;
	struct EventDispatcher;

	struct PlatformLayout;
	struct PlatformContext;
	struct Window;

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
		const Allocator* alloc;
		EventDispatcher* event_dispatcher;

		const char* title;
		WindowMode mode;
		bool resizable;
		WindowVsyncMode vsync_mode;

		i32 width;
		i32 height;
	};

	PlatformContext* platform_context_create(PlatformContextCreateInfo create_info);
	void platform_context_destroy(PlatformContext* ctx);

	// Window interface
	Window* window_create(WindowCreateInfo create_info);
	void window_destroy(NotNull<const Allocator*> alloc, Window* wnd);

	bool window_should_close(NotNull<Window*> wnd);
	void window_process_events(NotNull<Window*> wnd, f32 delta_time);

	void window_show(NotNull<Window*> wnd);
	void window_hide(NotNull<Window*> wnd);

	void window_get_surface(NotNull<Window*> wnd, void* surface_info);

	void window_set_title(NotNull<Window*> wnd, const char* title);

	void window_get_size(NotNull<Window*> wnd, i32* width, i32* height);
	f32 window_dpi_scale_factor(NotNull<Window*> wnd);
	f32 window_content_scale_factor(NotNull<Window*> wnd);
}

#endif //PLATFORM_CONTEXT_H