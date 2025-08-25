#pragma once

#include "../foundation/enum_flags.h"

namespace edge::gfx {
	enum class GraphicsDeviceType {
		eUnknown,
		eDiscrete,
		eIntegrated,
		eSoftware
	};

	enum class SyncResult {
		eSuccess,
		eTimeout,
		eDeviceLost,
		eError
	};

	enum class QueueType {
		eDirect,
		eCompute,
		eCopy
	};
}