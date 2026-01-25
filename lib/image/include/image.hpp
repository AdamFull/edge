#ifndef EDGE_IMAGE_H
#define EDGE_IMAGE_H

#include <cstdio>

#include <allocator.hpp>

namespace edge {
	struct ImageHeader;

	struct ImageFormatDesc {
		u32 block_width;
		u32 block_height;
		u32 block_size;
		bool compressed;
		u32 gl_internal_format;
		u32 gl_format;
		u32 gl_type;
		u32 vk_format;
		u32 dxgi_format;

		usize comp_size(u32 width, u32 height, u32 depth) const {
			if (compressed) {
				u32 blocks_x = (width + block_width - 1) / block_width;
				u32 blocks_y = (height + block_height - 1) / block_height;
				return blocks_x * blocks_y * depth * block_size;
			}

			return width * height * depth * block_size;
		}
	};

	struct IImageConsumer {
		virtual ~IImageConsumer() = default;

		virtual bool prepare(NotNull<const Allocator*> alloc, const ImageHeader& header) = 0;
		virtual bool read(NotNull<const Allocator*> alloc, const ImageHeader& header, u32 mip_level, u32 array_layer, u32 layer_count) = 0;
	};

	struct ImageHeader {
		enum class ContainerType {
			None,
			KTX_1_0,
			DDS,
			Internal
		};

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

		struct MipInfo {
			usize size;
			usize offset;
		};

		FILE* stream = nullptr;
		bool stream_owner = false;

		const ImageFormatDesc* format_desc = nullptr;

		u32 base_width = 1;
		u32 base_height = 1;
		u32 base_depth = 1;
		u32 mip_levels = 1;
		u32 array_layers = 1;
		u32 face_count = 1;

		ContainerType source_container_type = ContainerType::None;

		Result open_strem(const char* path, const char* mode);
		void close_stream();

		usize read_bytes(void* dst, usize bytes_to_read) const;
		bool try_read_bytes(void* dst, usize bytes_to_read) const;

		usize get_size() const;

		Result read_header(NotNull<const Allocator*> alloc);
		Result read_data(NotNull<const Allocator*> alloc, IImageConsumer* output);

		// DDS format
		Result read_header_dds(NotNull<const Allocator*> alloc);
		Result read_data_dds(NotNull<const Allocator*> alloc, IImageConsumer* output);

		// KTX1 format
		Result read_header_ktx1(NotNull<const Allocator*> alloc);
		Result read_data_ktx1(NotNull<const Allocator*> alloc, IImageConsumer* output);

		// Internal format
		Result read_header_internal(NotNull<const Allocator*> alloc);
		Result read_data_internal(NotNull<const Allocator*> alloc, IImageConsumer* output);

		Result write_header_internal(NotNull<const Allocator*> alloc);

		// TODO: Read data as

		Result write_internal_stream(NotNull<const Allocator*> alloc);
	};
}

#endif