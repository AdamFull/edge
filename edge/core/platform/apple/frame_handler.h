#pragma once

#include <dispatch/dispatch.h>
#include <mach/mach_time.h>

namespace edge {
	class AppleFrameHandler : public FrameHandlerBase {
	public:
		AppleFrameHandler();
		~AppleFrameHandler();

		auto sleep_(double seconds) -> void;

	private:
		dispatch_source_t dispatch_timer_ = nullptr;
		mach_timebase_info_data_t timebase_info_;
		uint64_t timer_start_ = 0;
		bool timer_fired_ = false;
	};

	using FrameHandler = AppleFrameHandler;
}