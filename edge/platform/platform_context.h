#pragma once

#include <memory>
#include <string_view>

namespace edge::platform {
	template<typename _Ty>
	concept PlatformContextConcept = requires(_Ty& ctx) {
		{ ctx.initialize() } -> std::same_as<bool>;
		{ ctx.shutdown() } -> std::same_as<void>;
		{ ctx.get_platform_name() } -> std::convertible_to<std::string_view>;
	};

	class PlatformContextInterface {
	public:
		template<typename _Self>
		auto initialize(this _Self& self) -> bool {
			return self._initialize();
		}

		template<typename _Self>
		auto shutdown(this _Self& self) -> void {
			return self._shutdown();
		}

		template<typename _Self>
		auto get_platform_name(this const _Self& self) -> std::string_view {
			return self._get_platform_name();
		}
	};
}