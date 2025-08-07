#pragma once

#include "../platform.h"
#include "android_input_layer.h"

struct android_app;

namespace edge::platform {
    class AndroidPlatformWindow final : public PlatformWindowInterface {
    public:
        ~AndroidPlatformWindow() override = default;
        static auto construct(PlatformContextInterface* platform_context) -> std::unique_ptr<AndroidPlatformWindow>;

        auto create(const window::Properties& props) -> bool override;
        auto destroy() -> void override;
        [[maybe_unused]] auto show() -> void override {}
        [[maybe_unused]] auto hide() -> void override {}
        [[maybe_unused]] auto is_visible() const -> bool override { return surface_ready_; }
        auto poll_events() -> void override;

        auto get_dpi_factor() const noexcept -> float override;
        auto get_content_scale_factor() const noexcept -> float override;

        auto set_title(std::string_view title) -> void override;
    private:
        static auto on_app_cmd(android_app* app, int32_t cmd) -> void;
        auto _process_commands(android_app* app, int32_t cmd) -> void;

        android_app* android_app_{ nullptr };
        PlatformContextInterface* platform_context_{ nullptr };
        std::unique_ptr<InputLayer> input_layer_;
        bool surface_ready_{ false };
    };

    using Window = AndroidPlatformWindow;

	class AndroidPlatformContext final : public PlatformContextInterface {
	public:
		static auto construct(android_app* app) -> std::unique_ptr<AndroidPlatformContext>;

        auto initialize(const PlatformCreateInfo& create_info) -> bool override;
        auto shutdown() -> void override;
        [[nodiscard]] auto get_platform_name() const->std::string_view override;

		auto get_android_app() -> android_app*;
		[[nodiscard]] auto get_android_app() const -> const android_app*;
    private:
		auto _construct(android_app* app) -> bool;

		android_app* android_app_{ nullptr };
        std::unique_ptr<Window> window_;
	};

	using PlatformContext = AndroidPlatformContext;
}