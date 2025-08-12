#pragma once

#include <array>
#include <bitset>
#include <memory>
#include <string>
#include <string_view>

#include "frame_handler.h"

#include "../application.h"
#include "../events.h"

namespace edge::platform {
	class PlatformWindowInterface;
	class PlatformContextInterface;
}

namespace edge::platform::window {
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

namespace edge::platform {
	struct PlatformCreateInfo {
		window::Properties window_props;
	};

    class PlatformInputInterface {
    public:
        static constexpr size_t k_max_gamepad_supported{ 8 };
        virtual ~PlatformInputInterface() = default;

        virtual auto create() -> bool = 0;
        virtual auto destroy() -> void = 0;

        virtual auto update(float delta_time) -> void = 0;

        virtual auto begin_text_input_capture(std::string_view initial_text = {}) -> bool = 0;
        virtual auto end_text_input_capture() -> void = 0;

        virtual auto set_gamepad_color(int32_t gamepad_id, uint32_t color) -> bool = 0;
    };

	class PlatformWindowInterface {
	public:
		virtual ~PlatformWindowInterface() = default;

		/**
		 * @brief Creates window and return result of creation
		 */
		virtual auto create(const window::Properties& props) -> bool = 0;

		/**
		 * @brief Requests to close and destroy the window
		 */
		virtual auto destroy() -> void = 0;

		/**
		 * @brief Requests to show the window
		 */
		virtual auto show() -> void = 0;

		/**
		 * @brief Requests to hide the window
		 */
		virtual auto hide() -> void = 0;

		/**
		 * @brief Test that window is not hidden or not under another one
		 */
		[[nodiscard]] virtual auto is_visible() const -> bool = 0;

		/**
		 * @brief Handles the processing of all underlying window events
		 */
		virtual auto poll_events() -> void = 0;

		/**
		 * @return The dot-per-inch scale factor
		 */
		[[nodiscard]]  virtual auto get_dpi_factor() const noexcept -> float = 0;

		/**
		 * @return The scale factor for systems with heterogeneous window and pixel coordinates
		 */
		[[nodiscard]]  virtual auto get_content_scale_factor() const noexcept -> float = 0;

		/**
		 * @brief Checks if the window should be closed
		 */
		[[nodiscard]] auto requested_close() const noexcept -> bool {
			return requested_close_;
		}

		[[nodiscard]] auto get_width() const noexcept -> uint32_t {
			return properties_.extent.width;
		}

		[[nodiscard]] auto get_height() const noexcept -> uint32_t {
			return properties_.extent.height;
		}

		[[nodiscard]] auto get_extent() const noexcept -> window::Extent {
			return properties_.extent;
		}

		/**
		 * @brief Windows title setter
		 * @param title New window title
		 */
		virtual auto set_title(std::string_view title) -> void = 0;

		[[nodiscard]] auto get_title() const -> std::string_view {
			return properties_.title;
		}
	protected:
		window::Properties properties_;
		bool requested_close_{ false };
	};

	class PlatformContextInterface {
	public:
		virtual ~PlatformContextInterface();

		auto initialize(const PlatformCreateInfo& create_info) -> bool;
		virtual auto shutdown() -> void = 0;

		auto terminate(int32_t code) -> void;

		auto setup_application(void(*app_setup_func)(std::unique_ptr<ApplicationInterface>&)) -> bool;

		auto main_loop() -> int32_t;
		auto main_loop_tick(float delta_time) -> int32_t;

		[[nodiscard]] virtual auto get_platform_name() const -> std::string_view = 0;

		auto on_any_window_event(const events::Dispatcher::event_variant_t& e) -> void;

		auto get_window() -> PlatformWindowInterface& {
			return *window_;
		}

		auto get_window() const -> PlatformWindowInterface const& {
			return *window_;
		}

        auto get_input() -> PlatformInputInterface& {
            return *input_;
        }

        auto get_input() const -> PlatformInputInterface const& {
            return *input_;
        }

		auto get_event_dispatcher() -> events::Dispatcher& {
			return *event_dispatcher_;
		}
		
		auto get_event_dispatcher() const -> events::Dispatcher const& {
			return *event_dispatcher_;
		}

	protected:
		FrameHandler frame_handler_;

		std::unique_ptr<ApplicationInterface> application_;

		std::unique_ptr<PlatformWindowInterface> window_;
        std::unique_ptr<PlatformInputInterface> input_;
		std::unique_ptr<events::Dispatcher> event_dispatcher_;

		bool window_focused_{ true };

		const float fixed_delta_time_{ 0.02f };
		float accumulated_delta_time_{ 0.0f };

		events::Dispatcher::listener_id_t any_window_event_listener_{ ~0ull };
	};
}