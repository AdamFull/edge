#pragma once

#include <sys/timerfd.h>
#include <unistd.h>
#include <time.h>

namespace edge {
	class LinuxFrameHandler : public FrameHandlerBase {
	public:
		LinuxFrameHandler();
		~LinuxFrameHandler();

		auto sleep_(double seconds) -> void;

	private:
		int timer_fd_ = -1;
	};

	using FrameHandler = LinuxFrameHandler;
}