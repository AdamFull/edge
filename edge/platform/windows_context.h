#pragma once

#include "platform_context.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#undef NOMINMAX
#undef WIN32_LEAN_AND_MEAN

namespace edge::platform {
	class WindowsPlatformContext final : public PlatformContextInterface {
	public:
		friend class PlatformContextInterface;
		static auto construct(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow) -> std::unique_ptr<WindowsPlatformContext>;
	private:
		auto _construct(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow) -> bool;
		auto _initialize() -> bool;
		auto _shutdown() -> void;
		auto _get_platform_name() const -> std::string_view;
	};

	using PlatformContext = WindowsPlatformContext;
}