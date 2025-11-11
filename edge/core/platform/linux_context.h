#pragma once

#include "platform.h"

namespace edge::platform {
	class LinuxPlatformContext final : public IPlatformContext {
	public:
		~LinuxPlatformContext() override = default;
		static auto construct() -> std::unique_ptr<LinuxPlatformContext>;

		auto shutdown() -> void override;
		auto get_platform_name() const -> std::string_view override;
	private:
		auto _construct() -> bool;
	};

	using PlatformContext = LinuxPlatformContext;
}