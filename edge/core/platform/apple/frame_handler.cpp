#include "../frame_handler.h"

namespace edge {
	AppleFrameHandler::AppleFrameHandler() {
		if (!dispatch_timer_) {
			mach_timebase_info(&timebase_info_);
			dispatch_timer_ = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0,
				dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0));
			if (dispatch_timer_) {
				dispatch_source_set_event_handler(dispatch_timer_, ^ {
					timer_fired_ = true;
					});
				dispatch_resume(dispatch_timer_);
			}
		}
	}

	AppleFrameHandler::~AppleFrameHandler() {
		if (dispatch_timer_) {
			dispatch_source_cancel(dispatch_timer_);
			dispatch_release(dispatch_timer_);
		}
	}

	auto AppleFrameHandler::sleep_(double seconds) -> void {
		// macOS dispatch timer
		timer_fired_ = false;
		uint64_t nanos = static_cast<uint64_t>(seconds * 1e9);
		uint64_t when = dispatch_time(DISPATCH_TIME_NOW, nanos);
		dispatch_source_set_timer(dispatch_timer_, when, DISPATCH_TIME_FOREVER, 0);

		// Wait for timer to fire
		while (!timer_fired_) {
			std::this_thread::sleep_for(std::chrono::microseconds(1));
		}
	}
}