#pragma once

#include "../platform.h"

struct android_app;

namespace edge::platform {
    class AndroidPlatformWindow final : public PlatformWindowInterface {
    public:
        ~AndroidPlatformWindow() override = default;
        static auto construct(const window::Properties& properties) -> std::unique_ptr<AndroidPlatformWindow>;

        auto create(const window::Properties& props) -> bool override;
        auto destroy() -> void override;
        auto show() -> void override;
        auto hide() -> void override;
        auto is_visible() const -> bool override;
        auto poll_events() -> void override;

        auto get_dpi_factor() const noexcept -> float override;
        auto get_content_scale_factor() const noexcept -> float override;

        auto set_title(std::string_view title) -> void override;

    private:
        auto _construct(const window::Properties& properties) -> bool;
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