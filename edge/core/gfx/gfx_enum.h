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

	enum class Result {
		eUndefined = -9999,
		eDeviceLost = -2,
		eTimeout = -1,
		eSuccess = 0
	};

	enum class ColorSpace {
		// sRGB color spaces
		eSrgbNonLinear = 0,		// Standard sRGB with gamma correction
		//eSrgbLinear,			// Linear sRGB (gamma = 1.0)

		// Rec.709 (HDTV standard)
		eRec709NonLinear,       // Rec.709 with gamma correction
		eRec709Linear,          // Linear Rec.709

		// Rec.2020 (UHD/4K standard)  
		//eRec2020NonLinear,      // Rec.2020 with gamma correction
		eRec2020Linear,         // Linear Rec.2020
		eRec2020Pq,             // Rec.2020 with ST.2084 PQ transfer function (HDR10)
		eRec2020Hlg,            // Rec.2020 with HLG transfer function

		// Display P3 (Apple displays)
		eDisplayP3NonLinear,	// Display P3 with gamma correction
		eDisplayP3Linear,		// Linear Display P3

		// Adobe RGB
		eAdobeRgbNonLinear,		// Adobe RGB with gamma correction
		eAdobeRgbLinear,		// Linear Adobe RGB

		// Extended sRGB (for HDR content)
		eExtendedSrgbLinear,	// Extended linear sRGB

		// Pass-through/Raw
		ePassThrough,           // No color space conversion
	};
}

EDGE_DEFINE_FLAG_NAMES(edge::gfx::GraphicsDeviceType,
	EDGE_FLAG_ENTRY(edge::gfx::GraphicsDeviceType::eUnknown, "Unknown"),
	EDGE_FLAG_ENTRY(edge::gfx::GraphicsDeviceType::eDiscrete, "Discrete"),
	EDGE_FLAG_ENTRY(edge::gfx::GraphicsDeviceType::eIntegrated, "Integrated"),
	EDGE_FLAG_ENTRY(edge::gfx::GraphicsDeviceType::eSoftware, "Software")
);

EDGE_DEFINE_FLAG_NAMES(edge::gfx::QueueType,
	EDGE_FLAG_ENTRY(edge::gfx::QueueType::eDirect, "Direct"),
	EDGE_FLAG_ENTRY(edge::gfx::QueueType::eCompute, "Compute"),
	EDGE_FLAG_ENTRY(edge::gfx::QueueType::eCopy, "Copy")
);

EDGE_DEFINE_FLAG_NAMES(edge::gfx::Result,
	EDGE_FLAG_ENTRY(edge::gfx::Result::eUndefined, "Undefined"),
	EDGE_FLAG_ENTRY(edge::gfx::Result::eDeviceLost, "DeviceLost"),
	EDGE_FLAG_ENTRY(edge::gfx::Result::eTimeout, "Timeout"),
	EDGE_FLAG_ENTRY(edge::gfx::Result::eSuccess, "Success")
);

EDGE_DEFINE_FLAG_NAMES(edge::gfx::ColorSpace,
	EDGE_FLAG_ENTRY(edge::gfx::ColorSpace::eSrgbNonLinear, "sRGB NonLinear"),
	EDGE_FLAG_ENTRY(edge::gfx::ColorSpace::eRec709NonLinear, "Rec809 NonLinear"),
	EDGE_FLAG_ENTRY(edge::gfx::ColorSpace::eRec709Linear, "Rec709 Linear"),
	EDGE_FLAG_ENTRY(edge::gfx::ColorSpace::eRec2020Linear, "Rec2020 Linear"),
	EDGE_FLAG_ENTRY(edge::gfx::ColorSpace::eRec2020Pq, "Rec2020 ST2084 PQ"),
	EDGE_FLAG_ENTRY(edge::gfx::ColorSpace::eRec2020Hlg, "Rec2020 HLG"),
	EDGE_FLAG_ENTRY(edge::gfx::ColorSpace::eDisplayP3NonLinear, "Display P3 NonLinear"),
	EDGE_FLAG_ENTRY(edge::gfx::ColorSpace::eDisplayP3Linear, "Display P3 Linear"),
	EDGE_FLAG_ENTRY(edge::gfx::ColorSpace::eAdobeRgbNonLinear, "Adobe RGB NonLinear"),
	EDGE_FLAG_ENTRY(edge::gfx::ColorSpace::eAdobeRgbLinear, "Adobe RGB Linear"),
	EDGE_FLAG_ENTRY(edge::gfx::ColorSpace::eExtendedSrgbLinear, "Extended sRGB Linear"),
	EDGE_FLAG_ENTRY(edge::gfx::ColorSpace::ePassThrough, "Pass Through")
);