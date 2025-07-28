#pragma once

#include "../platform.h"

struct android_app;

namespace edge::platform {
    class AndroidPlatformWindow final : public PlatformWindowInterface {
    public:
        ~AndroidPlatformWindow() override = default;
        static auto construct(const window::Properties& properties) -> std::unique_ptr<AndroidPlatformWindow>;

        auto create() -> bool override;
        auto destroy() -> void override;
        auto show() -> void override;
        auto hide() -> void override;
        auto is_visible() const -> bool override;
        auto poll_events() -> void override;

        auto set_title(std::string_view title) -> void override;
        auto get_title() const->std::string_view override;

    private:
        auto _construct(const window::Properties& properties) -> bool;
    };

    using Window = AndroidPlatformWindow;

	class AndroidPlatformContext final : public PlatformContextInterface {
	public:
		static auto construct(android_app* app) -> std::unique_ptr<AndroidPlatformContext>;

        auto initialize() -> bool override;
        auto create_window(const window::Properties& props) -> bool override;
        auto shutdown() -> void override;
        [[nodiscard]] auto get_platform_name() const->std::string_view override;

        auto get_window() -> Window& override;
        [[nodiscard]] auto get_window() const->Window const& override;

		auto get_android_app() -> android_app*;
		[[nodiscard]] auto get_android_app() const -> const android_app*;
	private:
		auto _construct(android_app* app) -> bool;

		android_app* android_app_{ nullptr };
        std::unique_ptr<Window> window_;
	};

	using PlatformContext = AndroidPlatformContext;
}