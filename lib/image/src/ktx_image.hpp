#ifndef EDGE_KTX_IMAGE_H
#define EDGE_KTX_IMAGE_H

#include "image.hpp"
#include "image_format.hpp"

namespace edge {
	namespace detail::ktx1 {
		[[maybe_unused]] constexpr u32 KTX_ENDIAN_REF = 0x04030201;
		constexpr u32 KTX_ENDIAN_REF_REV = 0x01020304;

		constexpr u8 IDENTIFIER[] = {
			0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A
		};

		static constexpr usize ident_size = sizeof(IDENTIFIER);

		struct Header {
			u32 endianness;
			u32 gl_type;
			u32 gl_type_size;
			u32 gl_format;
			u32 gl_internal_format;
			u32 gl_base_internal_format;
			u32 pixel_width;
			u32 pixel_height;
			u32 pixel_depth;
			u32 number_of_array_elements;
			u32 number_of_faces;
			u32 number_of_mipmap_levels;
			u32 bytes_of_key_value_data;
		};

		static constexpr usize header_size = sizeof(Header);

		static u32 swap_u32(u32 val) {
			return ((val & 0xFF000000) >> 24) |
				((val & 0x00FF0000) >> 8) |
				((val & 0x0000FF00) << 8) |
				((val & 0x000000FF) << 24);
		}

		static u32 gl_type_size_from_gl_type(u32 gl_type) {
			switch (gl_type) {
			case GL_BYTE:
			case GL_UNSIGNED_BYTE:
				return 1;
			case GL_SHORT:
			case GL_UNSIGNED_SHORT:
			case GL_HALF_FLOAT:
			case GL_UNSIGNED_SHORT_4_4_4_4:
			case GL_UNSIGNED_SHORT_4_4_4_4_REV:
			case GL_UNSIGNED_SHORT_5_5_5_1:
			case GL_UNSIGNED_SHORT_1_5_5_5_REV:
			case GL_UNSIGNED_SHORT_5_6_5:
			case GL_UNSIGNED_SHORT_5_6_5_REV:
				return 2;
			case GL_INT:
			case GL_UNSIGNED_INT:
			case GL_FLOAT:
			case GL_UNSIGNED_INT_24_8:
			case GL_UNSIGNED_INT_2_10_10_10_REV:
			case GL_UNSIGNED_INT_10F_11F_11F_REV:
			case GL_UNSIGNED_INT_5_9_9_9_REV:
				return 4;
			case GL_DOUBLE:
				return 8;
			default:
				return 0;
			}
		}
	}

	struct KTX10Reader final : IImageReader {
		FILE* stream = nullptr;
		ImageInfo info = {};
		u32 endianness = 0;

		usize current_mip = 0;

		KTX10Reader(NotNull<FILE*> fstream)
			: stream{ fstream.m_ptr } {
		}

		Result create(NotNull<const Allocator*> alloc) override {
			using namespace detail::ktx1;

			Header header;
			if (read_bytes(&header, header_size) != header_size) {
				return Result::InvalidHeader;
			}

			endianness = header.endianness;

			if (endianness == KTX_ENDIAN_REF_REV) {
				header.gl_type = swap_u32(header.gl_type);
				header.gl_type_size = swap_u32(header.gl_type_size);
				header.gl_format = swap_u32(header.gl_format);
				header.gl_internal_format = swap_u32(header.gl_internal_format);
				header.gl_base_internal_format = swap_u32(header.gl_base_internal_format);
				header.pixel_width = swap_u32(header.pixel_width);
				header.pixel_height = swap_u32(header.pixel_height);
				header.pixel_depth = swap_u32(header.pixel_depth);
				header.number_of_array_elements = swap_u32(header.number_of_array_elements);
				header.number_of_faces = swap_u32(header.number_of_faces);
				header.number_of_mipmap_levels = swap_u32(header.number_of_mipmap_levels);
				header.bytes_of_key_value_data = swap_u32(header.bytes_of_key_value_data);
			}

			const ImageFormatDesc* format_desc = detail::find_format_entry_by_gl(header.gl_internal_format);
			if (!format_desc) {
				return Result::InvalidPixelFormat;
			}

			info.init(format_desc,
				header.pixel_width, header.pixel_height, header.pixel_depth,
				header.number_of_mipmap_levels, header.number_of_array_elements,
				header.number_of_faces);

			// Skip metadata
			fseek(stream, header.bytes_of_key_value_data, SEEK_CUR);

			return Result::Success;
		}

		void destroy(NotNull<const Allocator*> alloc) override {}

		Result read_next_block(void* dst_memory, usize& dst_offset, ImageBlockInfo& block_info) override {
			using namespace detail::ktx1;

			if (current_mip >= static_cast<usize>(info.mip_levels)) {
				return Result::EndOfStream;
			}

			block_info.write_offset = dst_offset;
			block_info.mip_level = static_cast<u32>(current_mip++);
			block_info.array_layer = 0;
			block_info.layer_count = info.array_layers;

			block_info.block_width = max(info.base_width >> block_info.mip_level, 1u);
			block_info.block_height = max(info.base_height >> block_info.mip_level, 1u);
			block_info.block_depth = max(info.base_depth >> block_info.mip_level, 1u);

			u32 next_block_size = 0;
			if (read_bytes(&next_block_size, 4) != 4) {
				//EDGE_LOG_ERROR();
				return Result::EndOfStream;
			}

			if (endianness == KTX_ENDIAN_REF_REV) {
				next_block_size = swap_u32(next_block_size);
			}

			usize calculated_block_size = info.format_desc->comp_size(block_info.block_width, block_info.block_height, block_info.block_depth) * block_info.layer_count;
			if (calculated_block_size != static_cast<usize>(next_block_size)) {
				// Invalid image data
				return Result::EndOfStream;
			}

			usize bytes_readed = read_bytes((u8*)dst_memory + dst_offset, calculated_block_size);
			if (bytes_readed != calculated_block_size) {
				return Result::EndOfStream;
			}

			dst_offset += calculated_block_size;

			usize current_stream_offset = ftell(stream);
			fseek(stream, (current_stream_offset + 3) & ~3, SEEK_SET);

			return Result::Success;
		}

		const ImageInfo& get_info() const override {
			return info;
		}

		ImageContainerType get_container_type() const override {
			return ImageContainerType::KTX_1_0;
		}

	private:
		usize read_bytes(void* buffer, usize count) const {
			return fread(buffer, 1, count, stream);
		}
	};

	struct KTX10Writer final : IImageWriter {
		FILE* stream = nullptr;
		ImageInfo info = {};

		KTX10Writer(NotNull<FILE*> fstream)
			: stream{ fstream.m_ptr } {
		}

		Result create(NotNull<const Allocator*> alloc, const ImageInfo& image_info) override {
			using namespace detail::ktx1;

			if (!image_info.format_desc || image_info.format_desc->gl_internal_format == 0) {
				return Result::InvalidPixelFormat;
			}

			info = image_info;

			if (write_bytes(IDENTIFIER, ident_size) != ident_size) {
				return Result::BadStream;
			}

			Header header = {
				.endianness = KTX_ENDIAN_REF,
				.gl_type = info.format_desc->gl_type,
				.gl_format = info.format_desc->gl_format,
				.gl_internal_format = info.format_desc->gl_internal_format,
				.gl_base_internal_format = info.format_desc->gl_format != 0 ? info.format_desc->gl_format : info.format_desc->gl_internal_format,
				.pixel_width = info.base_width,
				.pixel_height = info.base_height,
				.pixel_depth = info.base_depth,
				.number_of_array_elements = info.array_layers,
				.number_of_faces = 1,
				.number_of_mipmap_levels = info.mip_levels,
				.bytes_of_key_value_data = 0
			};

			if (info.type == ImageType::ImageCube) {
				header.number_of_faces = 6;
				header.number_of_array_elements = max(info.array_layers / 6u, 1u);
			}

			if (info.format_desc->compressed) {
				header.gl_type = 0;
				header.gl_format = 0;
				header.gl_type_size = 1;
			}
			else {
				header.gl_type_size = gl_type_size_from_gl_type(header.gl_type);
				if (header.gl_type_size == 0) {
					return Result::UnsupportedFormat;
				}
			}

			if (write_bytes(&header, header_size) != header_size) {
				return Result::BadStream;
			}

			return Result::Success;
		}

		void destroy(NotNull<const Allocator*> alloc) override {
			if (stream) {
				fclose(stream);
			}
		}

		Result write_next_block(const void* src_memory, usize& src_offset, const ImageBlockInfo& block_info) override {
			using namespace detail::ktx1;

			return Result::Success;
		}

		const ImageInfo& get_info() const override {
			return info;
		}

		ImageContainerType get_container_type() const override {
			return ImageContainerType::KTX_1_0;
		}

	private:
		usize write_bytes(const void* buffer, usize count) const {
			return fwrite(buffer, 1, count, stream);
		}
	};
}

namespace edge {
	namespace detail::ktx2 {
		// TODO: NOT IMPLEMENTED
		constexpr u8 IDENTIFIER[] = {
			0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A
		};

		static constexpr usize ident_size = sizeof(IDENTIFIER);
	}
}

#endif