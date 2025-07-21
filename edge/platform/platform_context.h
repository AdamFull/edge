#pragma once

#include <memory>
#include <string_view>

namespace edge::platform {
	class PlatformContextBase {
	public:
		template<typename _Self>
		auto initialize(this _Self& self) -> bool {
			return self._initialize();
		}

		template<typename _Self>
		auto get_platform_name(this const _Self& self) -> std::string_view {
			return self._get_platform_name();
		}
	};
}