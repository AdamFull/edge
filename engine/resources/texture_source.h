#ifndef EDGE_TEXTURE_SOURCE_H
#define EDGE_TEXTURE_SOURCE_H

#include <allocator.hpp>

#include <vulkan/vulkan_core.h>

#include <cstdio>

namespace edge {
	namespace detail {
		struct FormatInfo {
			u32 block_width;
			u32 block_height;
			u32 block_size;
			bool compressed;
			u32 gl_internal_format;
			u32 gl_format;
			u32 gl_type;
			u32 vk_format;
			u32 dxgi_format;

			usize calculate_size(u32 width, u32 height, u32 depth) const noexcept {
				if (compressed) {
					u32 blocks_x = (width + block_width - 1) / block_width;
					u32 blocks_y = (height + block_height - 1) / block_height;
					return blocks_x * blocks_y * depth * block_size;
				}

				return width * height * depth * block_size;
			}
		};
	}

	struct TextureSource {
		enum class Result : u32 {
			Success = 0,
			InvalidHeader,
			UnsupportedFileFormat,
			UnsupportedPixelFormat,
			OutOfMemory,
			UnexpectedEndOfStream,
			BadStream,
			CompressionFailed
		};

		struct LevelInfo {
			usize size;
			usize offset;
		};

		struct SubresourceInfo {
			u8* data;
			usize size;
		};

		const detail::FormatInfo* format_info = nullptr;

		u32 base_width = 1;
		u32 base_height = 1;
		u32 base_depth = 1;
		u32 mip_levels = 1;
		u32 array_layers = 1;
		u32 face_count = 1;

		u8* image_data = nullptr;
		usize data_size = 0;

		LevelInfo level_infos[16] = {};

		static bool is_ok(Result result) noexcept {
			return result == Result::Success;
		}

		Result from_stream(NotNull<const Allocator*> alloc, NotNull<FILE*> stream) noexcept;
		Result from_dds_stream(NotNull<const Allocator*> alloc, NotNull<FILE*> stream) noexcept;
		Result from_ktx1_stream(NotNull<const Allocator*> alloc, NotNull<FILE*> stream) noexcept;
		Result from_ktx2_stream(NotNull<const Allocator*> alloc, NotNull<FILE*> stream) noexcept;
		Result from_etex_stream(NotNull<const Allocator*> alloc, NotNull<FILE*> stream) noexcept;

		void destroy(NotNull<const Allocator*> alloc) noexcept;

		Result write_etex_stream(NotNull<const Allocator*> alloc, NotNull<FILE*> stream) noexcept;

		SubresourceInfo get_mip(u32 level) noexcept;
		SubresourceInfo get_subresource_data_ptr(u32 level, u32 layer = 0u, u32 face = 0u) noexcept;
	};
}

#endif