#pragma once

#include <windows.h>

namespace edge {
	class WindowsFrameHandler : public FrameHandlerBase {
	public:
		WindowsFrameHandler();
		~WindowsFrameHandler();

		auto sleep_(double seconds) -> void;

	private:
		HANDLE waitable_timer_ = nullptr;
	};

	using FrameHandler = WindowsFrameHandler;
}