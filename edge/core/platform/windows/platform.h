#pragma once

#include "../desktop/platform.h"

#include <Windows.h>

namespace edge::platform {
	class WindowsPlatformContext final : public PlatformContextInterface {
	public:
		~WindowsPlatformContext() override = default;
		static auto construct(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow) -> std::unique_ptr<WindowsPlatformContext>;

		auto initialize(const PlatformCreateInfo& create_info) -> bool override;
		auto shutdown() -> void override;
		auto get_platform_name() const->std::string_view override;
	private:
		auto _construct(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow) -> bool;

		HINSTANCE hInstance_{ nullptr };
		HINSTANCE hPrevInstance_{ nullptr };
		PSTR lpCmdLine_{ nullptr };
		INT nCmdShow_{ 0 };
	};

	using PlatformContext = WindowsPlatformContext;
}