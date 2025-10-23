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
		eCopySource = 1 << 1,
		eCopyTarget = 1 << 2,
		eStorage = 1 << 3,
		eWriteColor = 1 << 4
	};

	EDGE_MAKE_ENUM_FLAGS(ImageFlags, ImageFlag);

	enum class ResourceStateFlag : uint16_t {
		eUndefined = 0,

		eVertexRead = 1 << 0,
		eIndexRead = 1 << 1,
		eRenderTarget = 1 << 2,
		eUnorderedAccess = 1 << 3,
		eDepthWrite = 1 << 4,
		eDepthRead = 1 << 5,
		eStencilWrite = 1 << 6,
		eStencilRead = 1 << 7,
		eDepthStencilWrite = eDepthWrite | eStencilWrite,
		eDepthStencilRead = eDepthRead | eStencilRead,
		eNonGraphicsShader = 1 << 8,
		eGraphicsShader = 1 << 9,
		eShaderResource = eNonGraphicsShader | eGraphicsShader,
		eIndirectArgument = 1 << 10,
		eCopyDst = 1 << 11,
		eCopySrc = 1 << 12,
		ePresent = 1 << 13,
		eAccelerationStructureRead = 1 << 14,
		eAccelerationStructureWrite = 1 << 15
	};

	EDGE_MAKE_ENUM_FLAGS(ResourceStateFlags, ResourceStateFlag);
}

EDGE_DEFINE_FLAG_NAMES(edge::gfx::QueueType,
	EDGE_FLAG_ENTRY(edge::gfx::QueueType::eDirect, "Direct"),
	EDGE_FLAG_ENTRY(edge::gfx::QueueType::eCompute, "Compute"),
	EDGE_FLAG_ENTRY(edge::gfx::QueueType::eCopy, "Copy")
);

EDGE_DEFINE_FLAG_NAMES(edge::gfx::ImageFlag,
	EDGE_FLAG_ENTRY(edge::gfx::ImageFlag::eSample, "Sample"),
	EDGE_FLAG_ENTRY(edge::gfx::ImageFlag::eCopySource, "CopySource"),
	EDGE_FLAG_ENTRY(edge::gfx::ImageFlag::eCopyTarget, "CopyTarget"),
	EDGE_FLAG_ENTRY(edge::gfx::ImageFlag::eStorage, "Storage"),
	EDGE_FLAG_ENTRY(edge::gfx::ImageFlag::eWriteColor, "WriteColor")
);