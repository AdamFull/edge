#pragma once

#include "../platform.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#undef NOMINMAX
#undef WIN32_LEAN_AND_MEAN

namespace edge::platform {
	class WindowsPlatformWindow final : public PlatformWindowInterface {
	public:
		~WindowsPlatformWindow() override = default;
		static auto construct(const window::Properties& properties) -> std::unique_ptr<WindowsPlatformWindow>;

		auto get_hwnd() -> HWND;
		auto get_hwnd() const -> const HWND;

		auto create() -> bool override;
		auto destroy() -> void override;
		auto show() -> void override;
		auto hide() -> void override;
		auto is_visible() const -> bool override;

		auto get_width() const->uint32_t override;
		auto get_height() const->uint32_t override;
		auto get_extent() const->window::Extent override;

		auto set_title(std::string_view title) -> void override;
		auto get_title() const->std::string_view override;

	private:
		auto _construct(const window::Properties& properties) -> bool;
		static auto CALLBACK _window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)->LRESULT;

		HWND hwnd_{ nullptr };
		bool is_visible_{ false };

		static constexpr const char* WINDOW_CLASS_NAME = "EdgeWindowClass";
		static bool class_registered_;
	};

	using Window = WindowsPlatformWindow;

	class WindowsPlatformContext final : public PlatformContextInterface {
	public:
		~WindowsPlatformContext() override = default;
		static auto construct(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow) -> std::unique_ptr<WindowsPlatformContext>;

		auto initialize() -> bool override;
		auto create_window(const window::Properties& props) -> bool override;
		auto shutdown() -> void override;
		auto get_platform_name() const->std::string_view override;

		auto get_window() -> PlatformWindowInterface& override;
		auto get_window() const->PlatformWindowInterface const& override;
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