#include "../frame_handler.h"

namespace edge {
	auto AndroidFrameHandler::sleep_(double seconds) -> void {
		// Android nanosleep implementation
		// Use nanosleep for better precision on mobile devices
		struct timespec req, rem;
		req.tv_sec = static_cast<time_t>(seconds);
		req.tv_nsec = static_cast<long>((seconds - req.tv_sec) * 1e9);

		// Handle potential interruptions
		while (nanosleep(&req, &rem) == -1) {
			req = rem;
		}
	}
}