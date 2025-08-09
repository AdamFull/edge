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

namespace edge::platform::input {
    struct InputDevice {
        int32_t vendor_id;
        int32_t product_id;
        int32_t device_id;
        bool connected;
        std::string name;
    };

    struct MouseDevice : InputDevice {
        std::array<float, 2> ss_pointer_pos;
        std::array<float, 2> pointer_delta;
        std::bitset<static_cast<size_t>(KeyboardKeyCode::eCount)> buttons;
    };

    struct KeyboardDevice : InputDevice {
        std::bitset<static_cast<size_t>(MouseKeyCode::eCount)> buttons;
    };

    struct GamepadDevice : InputDevice {
        int32_t gamepad_id;
        std::array<float, 2> left_stick;
        std::array<float, 2> right_stick;
        std::array<float, 3> accelerometer;
        float left_trigger;
        std::array<float, 3> gyroscope;
        float right_trigger;

        std::bitset<static_cast<size_t>(GamepadKeyCode::eButtonCount)> buttons;
    };

    struct TouchPointer {
        std::array<float, 2> position;
        KeyAction action;
    };

    struct TouchDevice : InputDevice {
        static constexpr size_t k_max_touch_pointers{ 16 };
        std::array<TouchPointer, k_max_touch_pointers> pointers;
    };

    enum InputDeviceAvaliabilityFlagBits {
        ENUM_INPUT_MOUSE_AVAILABLE_FLAG_BIT = 1 << 0,
        ENUM_INPUT_KEYBOARD_AVAILABLE_FLAG_BIT = 1 << 1,
        ENUM_INPUT_TOUCH_AVAILABLE_FLAG_BIT = 1 << 2,
        ENUM_INPUT_GAMEPAD_0_AVAILABLE_FLAG_BIT = 1 << 3,
        ENUM_INPUT_GAMEPAD_1_AVAILABLE_FLAG_BIT = 1 << 4,
        ENUM_INPUT_GAMEPAD_2_AVAILABLE_FLAG_BIT = 1 << 5,
        ENUM_INPUT_GAMEPAD_3_AVAILABLE_FLAG_BIT = 1 << 6,
        ENUM_INPUT_GAMEPAD_4_AVAILABLE_FLAG_BIT = 1 << 7,
        ENUM_INPUT_GAMEPAD_5_AVAILABLE_FLAG_BIT = 1 << 8,
        ENUM_INPUT_GAMEPAD_6_AVAILABLE_FLAG_BIT = 1 << 9,
        ENUM_INPUT_GAMEPAD_7_AVAILABLE_FLAG_BIT = 1 << 10,
    };
};

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

        virtual auto begin_text_input_capture(std::wstring_view initial_text = {}) -> bool = 0;
        virtual auto end_text_input_capture() -> void = 0;
    protected:
        input::MouseDevice mouse_device_;
        input::KeyboardDevice keyboard_device;
        input::TouchDevice touch_device;
        std::array<input::GamepadDevice, k_max_gamepad_supported> gamepad_devices;
        uint32_t devices_availability;
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

		virtual auto initialize(const PlatformCreateInfo& create_info) -> bool;
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
		std::unique_ptr<events::Dispatcher> event_dispatcher_;

		bool window_focused_{ true };

		const float fixed_delta_time_{ 0.02f };
		float accumulated_delta_time_{ 0.0f };

		events::Dispatcher::listener_id_t any_window_event_listener_{ ~0ull };
	};
}