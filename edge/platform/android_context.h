#pragma once

#include "platform_context.h"

struct android_app;

namespace edge::platform {
	class AndroidPlatformContext final : public PlatformContextInterface {
	public:
		friend class PlatformContextInterface;
		static auto construct(android_app* app) -> std::unique_ptr<AndroidPlatformContext>;

		auto get_android_app() -> android_app*;
		auto get_android_app() const -> const android_app*;
	private:
		auto _construct(android_app* app) -> bool;
		auto _initialize() -> bool;
		auto _shutdown() -> void;
		auto _get_platform_name() const -> std::string_view;

		android_app* android_app_{ nullptr };
	};

	using PlatformContext = AndroidPlatformContext;
}