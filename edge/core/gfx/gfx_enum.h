#pragma once

#include "../foundation/enum_flags.h"

namespace edge::gfx {
	enum class GraphicsDeviceType {
		eUnknown,
		eDiscrete,
		eIntegrated,
		eSoftware
	};

	enum class StageFlag {
		eNone = 1 << 0,
		eTopOfPipe = 1 << 1,
		eDrawIndirect = 1 << 2,
		eVertexInput = 1 << 3,
		eVertexShader = 1 << 4,
		eTessellationControlShader = 1 << 5,
		eTessellationEvaluationShader = 1 << 6,
		eGeometryShader = 1 << 7,
		eFragmentShader = 1 << 8,
		eEarlyFragmentTests = 1 << 9,
		eLateFragmentTests = 1 << 10,
		eColorAttachmentOutput = 1 << 11,
		eComputeShader = 1 << 12,
		eAllTransfer = 1 << 13,
		eTransfer = 1 << 14,
		eBottomOfPipe = 1 << 15,
		eHost = 1 << 16,
		eAllGraphics = 1 << 17,
		eAllCommands = 1 << 18,
		eCopy = 1 << 19,
		eResolve = 1 << 20,
		eBlit = 1 << 21,
		eClear = 1 << 22,
		eIndexInput = 1 << 23,
		eVertexAttributeInput = 1 << 24,
		ePreRasterizationShaders = 1 << 25
	};

	EDGE_MAKE_ENUM_FLAGS(StageFlags, StageFlag);
}