#pragma once

namespace edge {
	class GenericFrameHandler : public FrameHandlerBase {
	public:
		GenericFrameHandler() = default;
		~GenericFrameHandler() = default;

		auto sleep_(double seconds) -> void;
	};

	using FrameHandler = GenericFrameHandler;
}