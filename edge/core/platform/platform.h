#pragma once

#include <string>
#include <string_view>
#include <memory>

namespace edge::platform {
	namespace window {
		enum class Mode {
			eFullscreen,
			eFullscreenBorderless,
			eFullscreenStretch,
			eDefault
		};

		enum class Vsync {
			eOFF,
			eON,
			eDefault
		};

		struct Extent {
			uint32_t width;
			uint32_t height;
		};

		struct Properties {
			std::string title{ "Window" };
			Mode mode = Mode::eDefault;
			bool resizable = true;
			Vsync vsync = Vsync::eDefault;
			Extent extent = { 1280, 720 };
		};
	}

	class PlatformWindowInterface {
	public:
		virtual ~PlatformWindowInterface() = default;

		virtual auto create() -> bool = 0;
		virtual auto destroy() -> void = 0;
		virtual auto show() -> void = 0;
		virtual auto hide() -> void = 0;
		virtual auto is_visible() const -> bool = 0;

		virtual auto get_width() const -> uint32_t = 0;
		virtual auto get_height() const-> uint32_t = 0;
		virtual auto get_extent() const -> window::Extent = 0;

		virtual auto set_title(std::string_view title) -> void = 0;
		virtual auto get_title() const -> std::string_view = 0;
	protected:
		window::Properties properties_;
	};

	class PlatformContextInterface {
	public:
		virtual ~PlatformContextInterface() = default;

		virtual auto initialize() -> bool = 0;
		virtual auto shutdown() -> void = 0;
		virtual auto get_platform_name() const -> std::string_view = 0;

		virtual auto create_window(const window::Properties& props) -> bool = 0;
		virtual auto get_window() -> PlatformWindowInterface& = 0;
		virtual auto get_window() const -> PlatformWindowInterface const& = 0;
	};
}