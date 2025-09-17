#pragma once

#include "../foundation/enum_flags.h"

namespace edge::gfx {
	enum class QueueType {
		eDirect,
		eCompute,
		eCopy
	};

	enum class BufferFlag {
		eReadback = 1 << 0,
		eStaging = 1 << 1,
		eDynamic = 1 << 2,
		eVertex = 1 << 3,
		eIndex = 1 << 4,
		eUniform = 1 << 5,
		eStorage = 1 << 6,
		eIndirect = 1 << 7,
		eAccelerationBuild = 1 << 8,
		eAccelerationStore = 1 << 9,
		eShaderBindingTable = 1 << 10,
	};

	EDGE_MAKE_ENUM_FLAGS(BufferFlags, BufferFlag);

	static constexpr BufferFlags kReadbackBuffer{ BufferFlag::eReadback };
	static constexpr BufferFlags kStagingBuffer{ BufferFlag::eStaging };
	static constexpr BufferFlags kVertexBuffer{ BufferFlag::eVertex };
	static constexpr BufferFlags kDynamicVertexBuffer{ BufferFlag::eVertex | BufferFlag::eDynamic };
	static constexpr BufferFlags kIndexBuffer{ BufferFlag::eIndex };
	static constexpr BufferFlags kDynamicIndexBuffer{ BufferFlag::eIndex | BufferFlag::eDynamic };
	static constexpr BufferFlags kUniformBuffer{ BufferFlag::eUniform };
	static constexpr BufferFlags kDynamicUniformBuffer{ BufferFlag::eUniform | BufferFlag::eDynamic };
	static constexpr BufferFlags kStorageBuffer{ BufferFlag::eStorage };
	static constexpr BufferFlags kDynamicStorageBuffer{ BufferFlag::eStorage | BufferFlag::eDynamic };
	static constexpr BufferFlags kIndirectBuffer{ BufferFlag::eIndirect };
	static constexpr BufferFlags kDynamicIndirectBuffer{ BufferFlag::eIndirect | BufferFlag::eDynamic };

	enum class ImageFlag : uint16_t {
		eSample = 1 << 0,
		eCopy = 1 << 1,
		eStorage = 1 << 2,
		eWriteColor = 1 << 3
	};

	EDGE_MAKE_ENUM_FLAGS(ImageFlags, ImageFlag);
}

EDGE_DEFINE_FLAG_NAMES(edge::gfx::QueueType,
	EDGE_FLAG_ENTRY(edge::gfx::QueueType::eDirect, "Direct"),
	EDGE_FLAG_ENTRY(edge::gfx::QueueType::eCompute, "Compute"),
	EDGE_FLAG_ENTRY(edge::gfx::QueueType::eCopy, "Copy")
);

EDGE_DEFINE_FLAG_NAMES(edge::gfx::ImageFlag,
	EDGE_FLAG_ENTRY(edge::gfx::ImageFlag::eSample, "Sample"),
	EDGE_FLAG_ENTRY(edge::gfx::ImageFlag::eCopy, "Copy"),
	EDGE_FLAG_ENTRY(edge::gfx::ImageFlag::eStorage, "Storage"),
	EDGE_FLAG_ENTRY(edge::gfx::ImageFlag::eWriteColor, "WriteColor")
);