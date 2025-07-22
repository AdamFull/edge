#include "entry_point.h"

namespace edge::platform {
	auto WindowsPlatformContext::construct(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow)
		-> std::unique_ptr<WindowsPlatformContext> {
		auto ctx = std::make_unique<WindowsPlatformContext>();
		ctx->_construct(hInstance, hPrevInstance, lpCmdLine, nCmdShow);
		return ctx;
	}

	auto WindowsPlatformContext::_construct(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow) -> bool {
		return true;
	}

	auto WindowsPlatformContext::_initialize() -> bool {
		// Attempt to attach to the parent process console if it exists
		if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
			// No parent console, allocate a new one for this process
			if (!AllocConsole()) {
				return false;
				//EDGE_LOGE("[Windows Runtime Context] AllocConsole error");
			}
		}

		FILE* fp;
		freopen_s(&fp, "conin$", "r", stdin);
		freopen_s(&fp, "conout$", "w", stdout);
		freopen_s(&fp, "conout$", "w", stderr);

		return true;
	}

	auto WindowsPlatformContext::_get_platform_name() const -> std::string_view {
		return "Windows";
	}

	auto WindowsPlatformContext::_shutdown() -> void {

	}
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow) {
	auto context = edge::platform::PlatformContext::construct(hInstance, hPrevInstance, lpCmdLine, nCmdShow);
	return platform_main(*context);
}