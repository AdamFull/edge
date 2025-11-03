#pragma once

#include <time.h>
#include <unistd.h>

namespace edge {
	class AndroidFrameHandler : public FrameHandlerBase {
	public:
		AndroidFrameHandler() = default;
		~AndroidFrameHandler() = default;

		auto sleep_(double seconds) -> void;
	};

	using FrameHandler = AndroidFrameHandler;
}