#pragma once

#include "../desktop/platform.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#undef NOMINMAX
#undef WIN32_LEAN_AND_MEAN

namespace edge::platform {
	class WindowsPlatformContext final : public PlatformContextInterface {
	public:
		~WindowsPlatformContext() override = default;
		static auto construct(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow) -> std::unique_ptr<WindowsPlatformContext>;

		auto initialize() -> bool override;
		auto create_window(const window::Properties& props) -> bool override;
		auto shutdown() -> void override;
		auto get_platform_name() const->std::string_view override;

		auto get_window() -> Window& override;
		auto get_window() const->Window const& override;
	private:
		auto _construct(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow) -> bool;

		HINSTANCE hInstance_{ nullptr };
		HINSTANCE hPrevInstance_{ nullptr };
		PSTR lpCmdLine_{ nullptr };
		INT nCmdShow_{ 0 };

		std::unique_ptr<Window> window_;
	};

	using PlatformContext = WindowsPlatformContext;
}