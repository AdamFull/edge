#include "../frame_handler.h"

namespace edge {
	LinuxFrameHandler::LinuxFrameHandler() {
		if (timer_fd_ == -1) {
			timer_fd_ = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
		}
	}

	LinuxFrameHandler::~LinuxFrameHandler() {
		if (timer_fd_ >= 0) {
			close(timer_fd_);
		}
	}

	auto LinuxFrameHandler::sleep_(double seconds) -> void {
		// Linux timerfd
		struct itimerspec timer_spec = {};
		timer_spec.it_value.tv_sec = static_cast<time_t>(seconds);
		timer_spec.it_value.tv_nsec = static_cast<long>((seconds - timer_spec.it_value.tv_sec) * 1e9);

		timerfd_settime(timer_fd_, 0, &timer_spec, nullptr);
		uint64_t expirations;
		read(timer_fd_, &expirations, sizeof(expirations));
	}
}