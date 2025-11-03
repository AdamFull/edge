#pragma once

#include "platform.h"

struct android_app;

namespace edge::platform {
	class AndroidPlatformContext final : public IPlatformContext {
	public:
		static auto construct(android_app* app) -> std::unique_ptr<AndroidPlatformContext>;

		auto shutdown() -> void override;
		[[nodiscard]] auto get_platform_name() const -> std::string_view override;

		auto get_android_app() -> android_app*;
		[[nodiscard]] auto get_android_app() const -> const android_app*;
	private:
		auto _construct(android_app* app) -> bool;

		android_app* android_app_{ nullptr };
	};

	using PlatformContext = AndroidPlatformContext;
}