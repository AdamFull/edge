#ifndef RUNTIME_H
#define RUNTIME_H

#include <allocator.hpp>

namespace edge {
	struct RuntimeLayout;
	struct InputSystem;

	enum class WindowMode {
		Windowed,
		Fullscreen,
		FullscreenBorderless
	};

	struct RuntimeInitInfo {
		const Allocator* alloc = nullptr;
		RuntimeLayout* layout = nullptr;
		InputSystem* input_system = nullptr;

		const char* title = nullptr;
		WindowMode mode = WindowMode::Windowed;
		bool vsync = false;
		i32 width = 1;
		i32 height = 1;
	};

	struct IRuntime {
		virtual bool init(const RuntimeInitInfo& init_info) noexcept = 0;
		virtual void deinit(NotNull<const Allocator*> alloc) noexcept = 0;

		virtual bool requested_close() const noexcept = 0;
		virtual void process_events() noexcept = 0;

		virtual void get_surface(void* surface_info) const noexcept = 0;
		virtual void get_surface_extent(i32& width, i32& height) const noexcept = 0;
		virtual f32 get_surface_scale_factor() const noexcept = 0;

		virtual bool is_focused() const noexcept = 0;

		virtual void set_title(const char* title) noexcept = 0;
	};

	IRuntime* create_runtime(NotNull<const Allocator*> alloc) noexcept;
}

#endif // RUNTIME_H