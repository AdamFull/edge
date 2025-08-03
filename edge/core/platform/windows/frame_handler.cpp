#include "../frame_handler.h"

#include <cstdint>

namespace edge {
	WindowsFrameHandler::WindowsFrameHandler() {
		if (!waitable_timer_) {
			waitable_timer_ = CreateWaitableTimer(NULL, FALSE, NULL);
		}
	}

	WindowsFrameHandler::~WindowsFrameHandler() {
		if (waitable_timer_) {
			CloseHandle(waitable_timer_);
		}
	}

	auto WindowsFrameHandler::sleep_(double seconds) -> void {
		// Windows waitable timer
		LARGE_INTEGER due;
		due.QuadPart = -int64_t(seconds * 1e7);  // Convert to 100ns units
		SetWaitableTimerEx(waitable_timer_, &due, 0, NULL, NULL, NULL, 0);
		WaitForSingleObject(waitable_timer_, INFINITE);
	}
}