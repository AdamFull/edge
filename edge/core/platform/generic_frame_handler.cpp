#include "frame_handler.h"

namespace edge {
	auto GenericFrameHandler::sleep_(double seconds) -> void {
		// Fallback for other platforms
		std::this_thread::sleep_for(std::chrono::duration<double>(seconds));
	}
}