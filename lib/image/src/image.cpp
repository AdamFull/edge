#include "image.hpp"

#include "gl/glcorearb.h"
#include "dxgi/dxgiformat.h"
#include <vulkan/vulkan_core.h>

#include <math.hpp>

#define LZ4F_STATIC_LINKING_ONLY
#include <lz4frame.h>

namespace edge {
	namespace detail {
		namespace ktx1 {
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
		}

		namespace ktx2 {
			// TODO: NOT IMPLEMENTED
			constexpr u8 IDENTIFIER[] = {
				0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A
			};

			static constexpr usize ident_size = sizeof(IDENTIFIER);
		}

		namespace dds {
			constexpr u8 IDENTIFIER[] = {
				0x44, 0x44, 0x53, 0x20
			};

			static constexpr usize ident_size = sizeof(IDENTIFIER);

#define MAKE_FOURCC(a, b, c, d) ((u32)(a) | ((u32)(b) << 8) | ((u32)(c) << 16) | ((u32)(d) << 24))

			constexpr u32 FOURCC_DXT1 = MAKE_FOURCC('D', 'X', 'T', '1');
			constexpr u32 FOURCC_DXT2 = MAKE_FOURCC('D', 'X', 'T', '2');
			constexpr u32 FOURCC_DXT3 = MAKE_FOURCC('D', 'X', 'T', '3');
			constexpr u32 FOURCC_DXT4 = MAKE_FOURCC('D', 'X', 'T', '4');
			constexpr u32 FOURCC_DXT5 = MAKE_FOURCC('D', 'X', 'T', '5');
			constexpr u32 FOURCC_BC4U = MAKE_FOURCC('B', 'C', '4', 'U');
			constexpr u32 FOURCC_BC4S = MAKE_FOURCC('B', 'C', '4', 'S');
			constexpr u32 FOURCC_ATI2 = MAKE_FOURCC('A', 'T', 'I', '2');
			constexpr u32 FOURCC_BC5U = MAKE_FOURCC('B', 'C', '5', 'U');
			constexpr u32 FOURCC_BC5S = MAKE_FOURCC('B', 'C', '5', 'S');
			constexpr u32 FOURCC_RGBG = MAKE_FOURCC('R', 'G', 'B', 'G');
			constexpr u32 FOURCC_GRGB = MAKE_FOURCC('G', 'R', 'G', 'B');
			constexpr u32 FOURCC_DX10 = MAKE_FOURCC('D', 'X', '1', '0');

#undef MAKE_FOURCC

			enum PixelFormatFlagBits : u32 {
				DDS_PIXEL_FORMAT_ALPHA_PIXELS_FLAG_BIT = 0x1,
				DDS_PIXEL_FORMAT_ALPHA_FLAG_BIT = 0x2,
				DDS_PIXEL_FORMAT_FOUR_CC_FLAG_BIT = 0x4,
				DDS_PIXEL_FORMAT_RGB_FLAG_BIT = 0x40,
				DDS_PIXEL_FORMAT_YUV_FLAG_BIT = 0x200,
				DDS_PIXEL_FORMAT_LUMINANCE_FLAG_BIT = 0x20000
			};

			enum MiscFlagBits : u32 {
				DDS_MISC_FLAG_NONE = 0,
				DDS_MISC_TEXTURE_CUBE_FLAG_BIT = 0x4
			};

			enum ResourceDimension : u32 {
				DDS_RESOURCE_DIMENSION_UNKNOWN = 0,
				DDS_RESOURCE_DIMENSION_BUFFER = 1,
				DDS_RESOURCE_DIMENSION_TEXTURE_1D = 2,
				DDS_RESOURCE_DIMENSION_TEXTURE_2D = 3,
				DDS_RESOURCE_DIMENSION_TEXTURE_3D = 4
			};

			enum CapsFlagBits : u32 {
				DDS_CAPS_COMPLEX_FLAG_BIT = 0x8,
				DDS_CAPS_TEXTURE_FLAG_BIT = 0x1000,
				DDS_CAPS_MIP_MAP_FLAG_BIT = 0x400000
			};

			enum Caps2FlagBits : u32 {
				DDS_CAPS2_CUBEMAP_FLAG_BIT = 0x200,
				DDS_CAPS2_CUBEMAP_POSITIVE_X_FLAG_BIT = 0x400,
				DDS_CAPS2_CUBEMAP_NEGATIVE_X_FLAG_BIT = 0x800,
				DDS_CAPS2_CUBEMAP_POSITIVE_Y_FLAG_BIT = 0x1000,
				DDS_CAPS2_CUBEMAP_NEGATIVE_Y_FLAG_BIT = 0x2000,
				DDS_CAPS2_CUBEMAP_POSITIVE_Z_FLAG_BIT = 0x4000,
				DDS_CAPS2_CUBEMAP_NEGATIVE_Z_FLAG_BIT = 0x8000,
				DDS_CAPS2_VOLUME_FLAG_BIT = 0x200000
			};

			struct PixelFormat {
				u32 size;
				u32 flags;
				u32 fourcc;
				u32 rgb_bit_count;
				u32 r_bit_mask;
				u32 g_bit_mask;
				u32 b_bit_mask;
				u32 a_bit_mask;

				DXGI_FORMAT get_format() const {
					if (flags & DDS_PIXEL_FORMAT_FOUR_CC_FLAG_BIT) {
						switch (fourcc)
						{
						case FOURCC_DXT1: return DXGI_FORMAT_BC1_UNORM;
						case FOURCC_DXT2:
						case FOURCC_DXT3: return DXGI_FORMAT_BC2_UNORM;
						case FOURCC_DXT4:
						case FOURCC_DXT5: return DXGI_FORMAT_BC3_UNORM;
						case FOURCC_BC4U: return DXGI_FORMAT_BC4_UNORM;
						case FOURCC_BC4S: return DXGI_FORMAT_BC4_SNORM;
						case FOURCC_ATI2:
						case FOURCC_BC5U: return DXGI_FORMAT_BC5_UNORM;
						case FOURCC_BC5S: return DXGI_FORMAT_BC5_SNORM;
						case FOURCC_RGBG: return DXGI_FORMAT_R8G8_B8G8_UNORM;
						case FOURCC_GRGB: return DXGI_FORMAT_G8R8_G8B8_UNORM;
						case 36: return DXGI_FORMAT_R16G16B16A16_UNORM;
						case 110: return DXGI_FORMAT_R16G16B16A16_SNORM;
						case 111: return DXGI_FORMAT_R16_FLOAT;
						case 112: return DXGI_FORMAT_R16G16_FLOAT;
						case 113: return DXGI_FORMAT_R16G16B16A16_FLOAT;
						case 114: return DXGI_FORMAT_R32_FLOAT;
						case 115: return DXGI_FORMAT_R32G32_FLOAT;
						case 116: return DXGI_FORMAT_R32G32B32A32_FLOAT;
						default: return DXGI_FORMAT_UNKNOWN;
						}
					}

					if (flags & DDS_PIXEL_FORMAT_RGB_FLAG_BIT) {
						switch (rgb_bit_count) {
						case 32:
							// D3DFMT_A8B8G8R8
							if (r_bit_mask == 0x000000FF && g_bit_mask == 0x0000FF00 &&
								b_bit_mask == 0x00FF0000 && a_bit_mask == 0xFF000000) {
								return DXGI_FORMAT_R8G8B8A8_UNORM;
							}
							// D3DFMT_G16R16
							if (r_bit_mask == 0x0000FFFF && g_bit_mask == 0xFFFF0000 && b_bit_mask == 0x00000000) {
								return DXGI_FORMAT_R16G16_UNORM;
							}
							// D3DFMT_A2B10G10R10
							if (r_bit_mask == 0x000003FF && g_bit_mask == 0x000FFC00 &&
								b_bit_mask == 0x3FF00000 && a_bit_mask == 0xC0000000) {
								return DXGI_FORMAT_R10G10B10A2_UNORM;
							}
							// D3DFMT_A8R8G8B8
							if (r_bit_mask == 0x00FF0000 && g_bit_mask == 0x0000FF00 &&
								b_bit_mask == 0x000000FF && a_bit_mask == 0xFF000000) {
								return DXGI_FORMAT_B8G8R8A8_UNORM;
							}
							// D3DFMT_X8R8G8B8
							if (r_bit_mask == 0x00FF0000 && g_bit_mask == 0x0000FF00 &&
								b_bit_mask == 0x000000FF && a_bit_mask == 0x00000000) {
								return DXGI_FORMAT_B8G8R8X8_UNORM;
							}
							break;
						case 24:
							// D3DFMT_R8G8B8
							if (r_bit_mask == 0xFF0000 && g_bit_mask == 0x00FF00 && b_bit_mask == 0x0000FF) {
								// TODO: N\A in DXGI
								return DXGI_FORMAT_UNKNOWN;
							}
							break;
						case 16:
							// D3DFMT_A1R5G5B5
							if (r_bit_mask == 0x7C00 && g_bit_mask == 0x03E0 &&
								b_bit_mask == 0x001F && a_bit_mask == 0x8000) {
								return DXGI_FORMAT_B5G5R5A1_UNORM;
							}
							// D3FMT_R5G6B5
							if (r_bit_mask == 0xF800 && g_bit_mask == 0x07E0 &&
								b_bit_mask == 0x001F && a_bit_mask == 0x0000) {
								return DXGI_FORMAT_B5G6R5_UNORM;
							}
							break;
						case 8:
							// D3DFMT_A8
							if (flags & DDS_PIXEL_FORMAT_ALPHA_FLAG_BIT && a_bit_mask == 0xFF) {
								return DXGI_FORMAT_A8_UNORM;
							}
							break;
						}
					}

					if (flags & DDS_PIXEL_FORMAT_LUMINANCE_FLAG_BIT) {
						switch (rgb_bit_count) {
						case 16:
							// D3DFMT_A8L8
							if (r_bit_mask == 0x00FF && a_bit_mask == 0xFF00) {
								// TODO: N\A in DXGI
								return DXGI_FORMAT_UNKNOWN;
							}
							// D3DFMT_L16
							if (r_bit_mask == 0xFFFF) {
								return DXGI_FORMAT_R16_UNORM;
							}
							break;
						case 8:
							// D3DFMT_L8
							if (r_bit_mask == 0xFF) {
								return DXGI_FORMAT_R8_UNORM;
							}
							// D3DFMT_A4L4
							if (r_bit_mask == 0xF && a_bit_mask == 0xF0) {
								// TODO: N\A in DXGI
								return DXGI_FORMAT_UNKNOWN;
							}
						}
					}

					return DXGI_FORMAT_UNKNOWN;
				}
			};

			struct HeaderDXT10 {
				DXGI_FORMAT dxgi_format;
				ResourceDimension resource_dimension;
				u32 misc_flag;
				u32 array_size;
				u32 misc_flags2;
			};

			struct Header {
				u32 size;
				u32 flags;
				u32 height;
				u32 width;
				u32 pitch_or_linear_size;
				u32 depth;
				u32 mip_map_count;
				u32 reserved1[11];
				PixelFormat ddspf;
				u32 caps;
				u32 caps2;
				u32 caps3;
				u32 caps4;
				u32 reserved2;
			};

			static constexpr usize header_size = sizeof(Header);
			static constexpr usize header_dxt10_size = sizeof(HeaderDXT10);
		}

		namespace internal {
			constexpr u8 IDENTIFIER[] = {
				'E', 'D', 'G', 'E', ' ', 
				'I', 'N', 'T', 'E', 'R', 'N', 'A', 'L', ' ', 
				'I', 'M', 'A', 'G', 'E', 
				0x00, 0x00, 0x01, 0x00, 0x00
			};

			static constexpr usize ident_size = sizeof(IDENTIFIER);

			enum class ETexCompressionMethod : u32 {
				None = 0,
				LZ4 = 1,
				ZSTD = 2
			};

			struct Header {
				u32 vk_format;
				u32 pixel_width;
				u32 pixel_height;
				u32 pixel_depth;
				u32 number_of_array_elements;
				u32 number_of_faces;
				u32 number_of_mipmap_levels;
				ETexCompressionMethod compression;
			};

			static constexpr usize header_size = sizeof(Header);
			static constexpr usize chunk_size = 64 * 1024;

			static ImageHeader::Result decompress_lz4(
				NotNull<const Allocator*> alloc,
				NotNull<FILE*> stream,
				LZ4F_dctx* dctx,
				usize chunk_size,
				char* in_out_buffer,
				usize compressed_size,
				usize original_size,
				u8* dst_buffer
			) {
				usize total_decompressed = 0;
				usize compressed_remaining = compressed_size;
				usize ret = 1;

				while (ret != 0 && total_decompressed < original_size && compressed_remaining > 0) {
					usize to_read = (compressed_remaining < chunk_size) ? compressed_remaining : chunk_size;
					usize read_size = fread(in_out_buffer, 1, to_read, stream.m_ptr);

					if (read_size == 0 || read_size != to_read) {
						return ImageHeader::Result::UnexpectedEndOfStream;
					}

					compressed_remaining -= read_size;

					const void* src_ptr = in_out_buffer;
					const void* src_end = (const char*)in_out_buffer + read_size;

					while (src_ptr < src_end && ret != 0 && total_decompressed < original_size) {
						usize dst_size = chunk_size;
						usize src_size = (const char*)src_end - (const char*)src_ptr;

						ret = LZ4F_decompress(dctx, in_out_buffer + chunk_size, &dst_size, src_ptr, &src_size, NULL);

						if (LZ4F_isError(ret)) {
							return ImageHeader::Result::CompressionFailed;
						}

						if (dst_size > 0) {
							if (total_decompressed + dst_size > original_size) {
								return ImageHeader::Result::CompressionFailed;
							}

							memcpy(dst_buffer + total_decompressed, in_out_buffer + chunk_size, dst_size);
							total_decompressed += dst_size;
						}

						src_ptr = (const char*)src_ptr + src_size;
					}
				}

				if (total_decompressed != original_size) {
					return ImageHeader::Result::CompressionFailed;
				}

				return ImageHeader::Result::Success;
			}

			static ImageHeader::Result compress_lz4(
				NotNull<const Allocator*> alloc,
				NotNull<FILE*> stream,
				LZ4F_cctx* cctx,
				usize chunk_size,
				const u8* src_buffer, usize src_size,
				u32& total_compressed
			) {
				static const LZ4F_preferences_t lz4_prefs = {
					.frameInfo = {
						.blockSizeID = LZ4F_max256KB,
						.blockMode = LZ4F_blockLinked,
						.contentChecksumFlag = LZ4F_noContentChecksum,
						.frameType = LZ4F_frame,
						.contentSize = 0,
						.dictID = 0,
						.blockChecksumFlag = LZ4F_noBlockChecksum
					},
					.compressionLevel = 0,
					.autoFlush = 0,
					.favorDecSpeed = 0
				};

				i32 out_capacity = LZ4F_compressBound(chunk_size, &lz4_prefs);

				char* write_buffer = (char*)alloc->malloc(out_capacity, 1);
				if (!write_buffer) {
					LZ4F_freeCompressionContext(cctx);
					return ImageHeader::Result::OutOfMemory;
				}

				total_compressed = 0;
				usize bytes_remaining = src_size;

				usize header_size = LZ4F_compressBegin(cctx, write_buffer, out_capacity, &lz4_prefs);
				if (LZ4F_isError(header_size)) {
					alloc->free(write_buffer);
					return ImageHeader::Result::CompressionFailed;
				}

				usize bytes_written = fwrite(write_buffer, 1, header_size, stream.m_ptr);
				if (bytes_written != header_size) {
					alloc->free(write_buffer);
					return ImageHeader::Result::BadStream;
				}
				total_compressed += bytes_written;

				while (bytes_remaining > 0) {
					usize bytes_to_compress = (bytes_remaining < chunk_size) ? bytes_remaining : chunk_size;
					usize src_offset = src_size - bytes_remaining;
					usize compressed_size = LZ4F_compressUpdate(cctx, write_buffer, out_capacity, src_buffer + src_offset, bytes_to_compress, NULL);

					if (LZ4F_isError(compressed_size)) {
						alloc->free(write_buffer);
						return ImageHeader::Result::CompressionFailed;
					}

					bytes_written = fwrite(write_buffer, 1, compressed_size, stream.m_ptr);
					if (bytes_written != compressed_size) {
						alloc->free(write_buffer);
						return ImageHeader::Result::BadStream;
					}

					total_compressed += bytes_written;
					bytes_remaining -= bytes_to_compress;
				}

				usize end_size = LZ4F_compressEnd(cctx, write_buffer, out_capacity, NULL);
				if (LZ4F_isError(end_size)) {
					alloc->free(write_buffer);
					return ImageHeader::Result::CompressionFailed;
				}

				bytes_written = fwrite(write_buffer, 1, end_size, stream.m_ptr);
				if (bytes_written != end_size) {
					alloc->free(write_buffer);
					return ImageHeader::Result::BadStream;
				}
				total_compressed += end_size;

				alloc->free(write_buffer);

				return ImageHeader::Result::Success;
			}
		}

		constexpr usize max_ident_size = max(ktx1::ident_size, max(dds::ident_size, internal::ident_size));

		constexpr ImageFormatDesc g_format_table[] = {
			{ 1u, 1u, 1u, false, 0, 0, 0, VK_FORMAT_R4G4_UNORM_PACK8, DXGI_FORMAT_UNKNOWN },

			{ 1u, 1u, 2u, false, GL_RGBA4, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, VK_FORMAT_R4G4B4A4_UNORM_PACK16, DXGI_FORMAT_UNKNOWN },
			{ 1u, 1u, 2u, false, GL_RGBA4, GL_BGRA, GL_UNSIGNED_SHORT_4_4_4_4, VK_FORMAT_B4G4R4A4_UNORM_PACK16, DXGI_FORMAT_B4G4R4A4_UNORM },
			{ 1u, 1u, 2u, false, GL_RGB565, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, VK_FORMAT_R5G6B5_UNORM_PACK16, DXGI_FORMAT_UNKNOWN },
			{ 1u, 1u, 2u, false, GL_RGB565, GL_RGB, GL_UNSIGNED_SHORT_5_6_5_REV, VK_FORMAT_B5G6R5_UNORM_PACK16, DXGI_FORMAT_B5G6R5_UNORM },
			{ 1u, 1u, 2u, false, GL_RGB5_A1, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, VK_FORMAT_R5G5B5A1_UNORM_PACK16, DXGI_FORMAT_UNKNOWN },
			{ 1u, 1u, 2u, false, GL_RGB5_A1, GL_BGRA, GL_UNSIGNED_SHORT_5_5_5_1, VK_FORMAT_B5G5R5A1_UNORM_PACK16, DXGI_FORMAT_B5G5R5A1_UNORM },
			{ 1u, 1u, 2u, false, GL_RGB5_A1, GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, VK_FORMAT_A1R5G5B5_UNORM_PACK16, DXGI_FORMAT_UNKNOWN },

			{ 1u, 1u, 1u, false, GL_R8, GL_RED, GL_UNSIGNED_BYTE, VK_FORMAT_R8_UNORM, DXGI_FORMAT_R8_UNORM },
			{ 1u, 1u, 1u, false, GL_R8_SNORM, GL_RED, GL_BYTE, VK_FORMAT_R8_SNORM, DXGI_FORMAT_R8_SNORM },
			{ 1u, 1u, 1u, false, GL_R8UI, GL_RED_INTEGER, GL_UNSIGNED_BYTE, VK_FORMAT_R8_UINT, DXGI_FORMAT_R8_UINT },
			{ 1u, 1u, 1u, false, GL_R8I, GL_RED_INTEGER, GL_BYTE, VK_FORMAT_R8_SINT, DXGI_FORMAT_R8_SINT },
			{ 1u, 1u, 1u, false, GL_SR8_EXT, GL_RED, GL_UNSIGNED_BYTE, VK_FORMAT_R8_SRGB, DXGI_FORMAT_UNKNOWN },
			{ 1u, 1u, 2u, false, GL_RG8, GL_RG, GL_UNSIGNED_BYTE, VK_FORMAT_R8G8_UNORM, DXGI_FORMAT_R8G8_UNORM },
			{ 1u, 1u, 2u, false, GL_RG8_SNORM, GL_RG, GL_BYTE, VK_FORMAT_R8G8_SNORM, DXGI_FORMAT_R8G8_SNORM },
			{ 1u, 1u, 2u, false, GL_RG8UI, GL_RG_INTEGER, GL_UNSIGNED_BYTE, VK_FORMAT_R8G8_UINT, DXGI_FORMAT_R8G8_UINT },
			{ 1u, 1u, 2u, false, GL_RG8I, GL_RG_INTEGER, GL_BYTE, VK_FORMAT_R8G8_SINT, DXGI_FORMAT_R8G8_SINT },
			{ 1u, 1u, 2u, false, GL_SRG8_EXT, GL_RG, GL_UNSIGNED_BYTE, VK_FORMAT_R8G8_SRGB, DXGI_FORMAT_UNKNOWN },
			{ 1u, 1u, 3u, false, GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE, VK_FORMAT_R8G8B8_UNORM, DXGI_FORMAT_UNKNOWN },
			{ 1u, 1u, 3u, false, GL_RGB8_SNORM, GL_RGB, GL_BYTE, VK_FORMAT_R8G8B8_SNORM, DXGI_FORMAT_UNKNOWN },
			{ 1u, 1u, 3u, false, GL_RGB8UI, GL_RGB_INTEGER, GL_UNSIGNED_BYTE, VK_FORMAT_R8G8B8_UINT, DXGI_FORMAT_UNKNOWN },
			{ 1u, 1u, 3u, false, GL_RGB8I, GL_RGB_INTEGER, GL_BYTE, VK_FORMAT_R8G8B8_SINT, DXGI_FORMAT_UNKNOWN },
			{ 1u, 1u, 3u, false, GL_SRGB8, GL_RGB, GL_UNSIGNED_BYTE, VK_FORMAT_R8G8B8_SRGB, DXGI_FORMAT_UNKNOWN },
			{ 1u, 1u, 3u, false, GL_RGB8, GL_BGR, GL_UNSIGNED_BYTE, VK_FORMAT_B8G8R8_UNORM, DXGI_FORMAT_UNKNOWN },
			{ 1u, 1u, 3u, false, GL_RGB8_SNORM, GL_BGR, GL_BYTE, VK_FORMAT_B8G8R8_SNORM, DXGI_FORMAT_UNKNOWN },
			{ 1u, 1u, 3u, false, GL_RGB8UI, GL_BGR_INTEGER, GL_UNSIGNED_BYTE, VK_FORMAT_B8G8R8_UINT, DXGI_FORMAT_UNKNOWN },
			{ 1u, 1u, 3u, false, GL_RGB8I, GL_BGR_INTEGER, GL_BYTE, VK_FORMAT_B8G8R8_SINT, DXGI_FORMAT_UNKNOWN },
			{ 1u, 1u, 3u, false, GL_SRGB8, GL_BGR, GL_UNSIGNED_BYTE, VK_FORMAT_B8G8R8_SRGB, DXGI_FORMAT_UNKNOWN },
			{ 1u, 1u, 4u, false, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, VK_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM },
			{ 1u, 1u, 4u, false, GL_RGBA8_SNORM, GL_RGBA, GL_BYTE, VK_FORMAT_R8G8B8A8_SNORM, DXGI_FORMAT_R8G8B8A8_SNORM },
			{ 1u, 1u, 4u, false, GL_RGBA8UI, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, VK_FORMAT_R8G8B8A8_UINT, DXGI_FORMAT_R8G8B8A8_UINT },
			{ 1u, 1u, 4u, false, GL_RGBA8I, GL_RGBA_INTEGER, GL_BYTE, VK_FORMAT_R8G8B8A8_SINT, DXGI_FORMAT_R8G8B8A8_SINT },
			{ 1u, 1u, 4u, false, GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE, VK_FORMAT_R8G8B8A8_SRGB, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB },
			{ 1u, 1u, 4u, false, GL_RGBA8, GL_BGRA, GL_UNSIGNED_BYTE, VK_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_B8G8R8A8_UNORM },
			{ 1u, 1u, 4u, false, GL_RGBA8_SNORM, GL_BGRA, GL_BYTE, VK_FORMAT_B8G8R8A8_SNORM, DXGI_FORMAT_UNKNOWN },
			{ 1u, 1u, 4u, false, GL_RGBA8UI, GL_BGRA_INTEGER, GL_UNSIGNED_BYTE, VK_FORMAT_B8G8R8A8_UINT, DXGI_FORMAT_UNKNOWN },
			{ 1u, 1u, 4u, false, GL_RGBA8I, GL_BGRA_INTEGER, GL_BYTE, VK_FORMAT_B8G8R8A8_SINT, DXGI_FORMAT_UNKNOWN },
			{ 1u, 1u, 4u, false, GL_SRGB8_ALPHA8, GL_BGRA, GL_UNSIGNED_BYTE, VK_FORMAT_B8G8R8A8_SRGB, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB },

			{ 1u, 1u, 4u, false, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, VK_FORMAT_A8B8G8R8_UNORM_PACK32, DXGI_FORMAT_UNKNOWN },
			{ 1u, 1u, 4u, false, GL_RGBA8_SNORM, GL_RGBA, GL_BYTE, VK_FORMAT_A8B8G8R8_SNORM_PACK32, DXGI_FORMAT_UNKNOWN },
			{ 1u, 1u, 4u, false, GL_RGBA8UI, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, VK_FORMAT_A8B8G8R8_UINT_PACK32, DXGI_FORMAT_UNKNOWN },
			{ 1u, 1u, 4u, false, GL_RGBA8I, GL_RGBA_INTEGER, GL_BYTE, VK_FORMAT_A8B8G8R8_SINT_PACK32, DXGI_FORMAT_UNKNOWN },
			{ 1u, 1u, 4u, false, GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE, VK_FORMAT_A8B8G8R8_SRGB_PACK32, DXGI_FORMAT_UNKNOWN },
			{ 1u, 1u, 4u, false, GL_RGB10_A2, GL_BGRA, GL_UNSIGNED_INT_2_10_10_10_REV, VK_FORMAT_A2R10G10B10_UNORM_PACK32, DXGI_FORMAT_UNKNOWN },
			{ 1u, 1u, 4u, false, 0, 0, 0, VK_FORMAT_A2R10G10B10_SNORM_PACK32, DXGI_FORMAT_UNKNOWN },
			{ 1u, 1u, 4u, false, GL_RGB10_A2UI, GL_BGRA_INTEGER, GL_UNSIGNED_INT_2_10_10_10_REV, VK_FORMAT_A2R10G10B10_UINT_PACK32, DXGI_FORMAT_UNKNOWN },
			{ 1u, 1u, 4u, false, 0, 0, 0, VK_FORMAT_A2R10G10B10_SINT_PACK32, DXGI_FORMAT_UNKNOWN },
			{ 1u, 1u, 4u, false, GL_RGB10_A2, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, VK_FORMAT_A2B10G10R10_UNORM_PACK32, DXGI_FORMAT_R10G10B10A2_UNORM },
			{ 1u, 1u, 4u, false, 0, 0, 0, VK_FORMAT_A2B10G10R10_SNORM_PACK32, DXGI_FORMAT_UNKNOWN },
			{ 1u, 1u, 4u, false, GL_RGB10_A2UI, GL_RGBA_INTEGER, GL_UNSIGNED_INT_2_10_10_10_REV, VK_FORMAT_A2B10G10R10_UINT_PACK32, DXGI_FORMAT_R10G10B10A2_UINT },
			{ 1u, 1u, 4u, false, 0, 0, 0, VK_FORMAT_A2B10G10R10_SINT_PACK32, DXGI_FORMAT_UNKNOWN },


			{ 1u, 1u, 2u, false, GL_R16, GL_RED, GL_UNSIGNED_SHORT, VK_FORMAT_R16_UNORM, DXGI_FORMAT_R16_UNORM },
			{ 1u, 1u, 2u, false, GL_R16_SNORM, GL_RED, GL_SHORT, VK_FORMAT_R16_SNORM, DXGI_FORMAT_R16_SNORM },
			{ 1u, 1u, 2u, false, GL_R16UI, GL_RED_INTEGER, GL_UNSIGNED_SHORT, VK_FORMAT_R16_UINT, DXGI_FORMAT_R16_UINT },
			{ 1u, 1u, 2u, false, GL_R16I, GL_RED_INTEGER, GL_SHORT, VK_FORMAT_R16_SINT, DXGI_FORMAT_R16_SINT },
			{ 1u, 1u, 2u, false, GL_R16F, GL_RED, GL_HALF_FLOAT, VK_FORMAT_R16_SFLOAT, DXGI_FORMAT_R16_FLOAT },
			{ 1u, 1u, 4u, false, GL_RG16, GL_RG, GL_UNSIGNED_SHORT, VK_FORMAT_R16G16_UNORM, DXGI_FORMAT_R16G16_UNORM },
			{ 1u, 1u, 4u, false, GL_RG16_SNORM, GL_RG, GL_SHORT, VK_FORMAT_R16G16_SNORM, DXGI_FORMAT_R16G16_SNORM },
			{ 1u, 1u, 4u, false, GL_RG16UI, GL_RG_INTEGER, GL_UNSIGNED_SHORT, VK_FORMAT_R16G16_UINT, DXGI_FORMAT_R16G16_UINT },
			{ 1u, 1u, 4u, false, GL_RG16I, GL_RG_INTEGER, GL_SHORT, VK_FORMAT_R16G16_SINT, DXGI_FORMAT_R16G16_SINT },
			{ 1u, 1u, 4u, false, GL_RG16F, GL_RG, GL_HALF_FLOAT, VK_FORMAT_R16G16_SFLOAT, DXGI_FORMAT_R16G16_FLOAT },
			{ 1u, 1u, 6u, false, GL_RGB16, GL_RGB, GL_UNSIGNED_SHORT, VK_FORMAT_R16G16B16_UNORM, DXGI_FORMAT_UNKNOWN },
			{ 1u, 1u, 6u, false, GL_RGB16_SNORM, GL_RGB, GL_SHORT, VK_FORMAT_R16G16B16_SNORM, DXGI_FORMAT_UNKNOWN },
			{ 1u, 1u, 6u, false, GL_RGB16UI, GL_RGB_INTEGER, GL_UNSIGNED_SHORT, VK_FORMAT_R16G16B16_UINT, DXGI_FORMAT_UNKNOWN },
			{ 1u, 1u, 6u, false, GL_RGB16I, GL_RGB_INTEGER, GL_SHORT, VK_FORMAT_R16G16B16_SINT, DXGI_FORMAT_UNKNOWN },
			{ 1u, 1u, 6u, false, GL_RGB16F, GL_RGB, GL_HALF_FLOAT, VK_FORMAT_R16G16B16_SFLOAT, DXGI_FORMAT_UNKNOWN },
			{ 1u, 1u, 8u, false, GL_RGBA16, GL_RGBA, GL_UNSIGNED_SHORT, VK_FORMAT_R16G16B16A16_UNORM, DXGI_FORMAT_R16G16B16A16_UNORM },
			{ 1u, 1u, 8u, false, GL_RGBA16_SNORM, GL_RGBA, GL_SHORT, VK_FORMAT_R16G16B16A16_SNORM, DXGI_FORMAT_R16G16B16A16_SNORM },
			{ 1u, 1u, 8u, false, GL_RGBA16UI, GL_RGBA_INTEGER, GL_UNSIGNED_SHORT, VK_FORMAT_R16G16B16A16_UINT, DXGI_FORMAT_R16G16B16A16_UINT },
			{ 1u, 1u, 8u, false, GL_RGBA16I, GL_RGBA_INTEGER, GL_SHORT, VK_FORMAT_R16G16B16A16_SINT, DXGI_FORMAT_R16G16B16A16_SINT },
			{ 1u, 1u, 8u, false, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, VK_FORMAT_R16G16B16A16_SFLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT },

			{ 1u, 1u, 4u, false, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, VK_FORMAT_R32_UINT, DXGI_FORMAT_R32_UINT },
			{ 1u, 1u, 4u, false, GL_R32I, GL_RED_INTEGER, GL_INT, VK_FORMAT_R32_SINT, DXGI_FORMAT_R32_SINT },
			{ 1u, 1u, 4u, false, GL_R32F, GL_RED, GL_FLOAT, VK_FORMAT_R32_SFLOAT, DXGI_FORMAT_R32_FLOAT },
			{ 1u, 1u, 8u, false, GL_RG32UI, GL_RG_INTEGER, GL_UNSIGNED_INT, VK_FORMAT_R32G32_UINT, DXGI_FORMAT_R32G32_UINT },
			{ 1u, 1u, 8u, false, GL_RG32I, GL_RG_INTEGER, GL_INT, VK_FORMAT_R32G32_SINT, DXGI_FORMAT_R32G32_SINT },
			{ 1u, 1u, 8u, false, GL_RG32F, GL_RG, GL_FLOAT, VK_FORMAT_R32G32_SFLOAT, DXGI_FORMAT_R32G32_FLOAT },
			{ 1u, 1u, 12u, false, GL_RGB32UI, GL_RGB_INTEGER, GL_UNSIGNED_INT, VK_FORMAT_R32G32B32_UINT, DXGI_FORMAT_R32G32B32_UINT },
			{ 1u, 1u, 12u, false, GL_RGB32I, GL_RGB_INTEGER, GL_INT, VK_FORMAT_R32G32B32_SINT, DXGI_FORMAT_R32G32B32_SINT },
			{ 1u, 1u, 12u, false, GL_RGB32F, GL_RGB, GL_FLOAT, VK_FORMAT_R32G32B32_SFLOAT, DXGI_FORMAT_R32G32B32_FLOAT },
			{ 1u, 1u, 16u, false, GL_RGBA32UI, GL_RGBA_INTEGER, GL_UNSIGNED_INT, VK_FORMAT_R32G32B32A32_UINT, DXGI_FORMAT_R32G32B32A32_UINT },
			{ 1u, 1u, 16u, false, GL_RGBA32I, GL_RGBA_INTEGER, GL_INT, VK_FORMAT_R32G32B32A32_SINT, DXGI_FORMAT_R32G32B32A32_SINT },
			{ 1u, 1u, 16u, false, GL_RGBA32F, GL_RGBA, GL_FLOAT, VK_FORMAT_R32G32B32A32_SFLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT },

			{ 1u, 1u, 8u, false, 0, 0, 0, VK_FORMAT_R64_UINT, DXGI_FORMAT_UNKNOWN },
			{ 1u, 1u, 8u, false, 0, 0, 0, VK_FORMAT_R64_SINT, DXGI_FORMAT_UNKNOWN },
			{ 1u, 1u, 8u, false, 0, 0, 0, VK_FORMAT_R64_SFLOAT, DXGI_FORMAT_UNKNOWN },
			{ 1u, 1u, 16u, false, 0, 0, 0, VK_FORMAT_R64G64_UINT, DXGI_FORMAT_UNKNOWN },
			{ 1u, 1u, 16u, false, 0, 0, 0, VK_FORMAT_R64G64_SINT, DXGI_FORMAT_UNKNOWN },
			{ 1u, 1u, 16u, false, 0, 0, 0, VK_FORMAT_R64G64_SFLOAT, DXGI_FORMAT_UNKNOWN },
			{ 1u, 1u, 24u, false, 0, 0, 0, VK_FORMAT_R64G64B64_UINT, DXGI_FORMAT_UNKNOWN },
			{ 1u, 1u, 24u, false, 0, 0, 0, VK_FORMAT_R64G64B64_SINT, DXGI_FORMAT_UNKNOWN },
			{ 1u, 1u, 24u, false, 0, 0, 0, VK_FORMAT_R64G64B64_SFLOAT, DXGI_FORMAT_UNKNOWN },
			{ 1u, 1u, 32u, false, 0, 0, 0, VK_FORMAT_R64G64B64A64_UINT, DXGI_FORMAT_UNKNOWN },
			{ 1u, 1u, 32u, false, 0, 0, 0, VK_FORMAT_R64G64B64A64_SINT, DXGI_FORMAT_UNKNOWN },
			{ 1u, 1u, 32u, false, 0, 0, 0, VK_FORMAT_R64G64B64A64_SFLOAT, DXGI_FORMAT_UNKNOWN },

			{ 1u, 1u, 4u, false, GL_R11F_G11F_B10F, GL_RGB, GL_UNSIGNED_INT_10F_11F_11F_REV, VK_FORMAT_B10G11R11_UFLOAT_PACK32, DXGI_FORMAT_R11G11B10_FLOAT },
			{ 1u, 1u, 4u, false, GL_RGB9_E5, GL_RGB, GL_UNSIGNED_INT_5_9_9_9_REV, VK_FORMAT_E5B9G9R9_UFLOAT_PACK32, DXGI_FORMAT_R9G9B9E5_SHAREDEXP },

			{ 1u, 1u, 2u, false, GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, VK_FORMAT_D16_UNORM, DXGI_FORMAT_D16_UNORM },
			{ 1u, 1u, 4u, false, 0, 0, 0, VK_FORMAT_X8_D24_UNORM_PACK32, DXGI_FORMAT_D24_UNORM_S8_UINT },
			{ 1u, 1u, 4u, false, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT, VK_FORMAT_D32_SFLOAT, DXGI_FORMAT_D32_FLOAT },
			{ 1u, 1u, 1u, false, GL_STENCIL_INDEX8, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, VK_FORMAT_S8_UINT, DXGI_FORMAT_UNKNOWN },
			{ 1u, 1u, 3u, false, 0, 0, 0, VK_FORMAT_D16_UNORM_S8_UINT, DXGI_FORMAT_UNKNOWN },
			{ 1u, 1u, 4u, false, GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, VK_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT_D24_UNORM_S8_UINT },
			{ 1u, 1u, 5u, false, GL_DEPTH32F_STENCIL8, GL_DEPTH_STENCIL, GL_FLOAT_32_UNSIGNED_INT_24_8_REV, VK_FORMAT_D32_SFLOAT_S8_UINT, DXGI_FORMAT_D32_FLOAT_S8X24_UINT },

			{ 4u, 4u, 8u, true, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 0, 0, VK_FORMAT_BC1_RGB_UNORM_BLOCK, DXGI_FORMAT_BC1_UNORM },
			{ 4u, 4u, 8u, true, GL_COMPRESSED_SRGB_S3TC_DXT1_EXT, 0, 0, VK_FORMAT_BC1_RGB_SRGB_BLOCK, DXGI_FORMAT_BC1_UNORM_SRGB },
			{ 4u, 4u, 8u, true, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, 0, 0, VK_FORMAT_BC1_RGBA_UNORM_BLOCK, DXGI_FORMAT_BC1_UNORM },
			{ 4u, 4u, 8u, true, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT, 0, 0, VK_FORMAT_BC1_RGBA_SRGB_BLOCK, DXGI_FORMAT_BC1_UNORM_SRGB },
			{ 4u, 4u, 16u, true, GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, 0, 0, VK_FORMAT_BC2_UNORM_BLOCK, DXGI_FORMAT_BC2_UNORM },
			{ 4u, 4u, 16u, true, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT, 0, 0, VK_FORMAT_BC2_SRGB_BLOCK, DXGI_FORMAT_BC2_UNORM_SRGB },
			{ 4u, 4u, 16u, true, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, 0, 0, VK_FORMAT_BC3_UNORM_BLOCK, DXGI_FORMAT_BC3_UNORM },
			{ 4u, 4u, 16u, true, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT, 0, 0, VK_FORMAT_BC3_SRGB_BLOCK, DXGI_FORMAT_BC3_UNORM_SRGB },
			{ 4u, 4u, 8u, true, GL_COMPRESSED_RED_RGTC1, 0, 0, VK_FORMAT_BC4_UNORM_BLOCK, DXGI_FORMAT_BC4_UNORM },
			{ 4u, 4u, 8u, true, GL_COMPRESSED_SIGNED_RED_RGTC1, 0, 0, VK_FORMAT_BC4_SNORM_BLOCK, DXGI_FORMAT_BC4_SNORM },
			{ 4u, 4u, 16u, true, GL_COMPRESSED_RG_RGTC2, 0, 0, VK_FORMAT_BC5_UNORM_BLOCK, DXGI_FORMAT_BC5_UNORM },
			{ 4u, 4u, 16u, true, GL_COMPRESSED_SIGNED_RG_RGTC2, 0, 0, VK_FORMAT_BC5_SNORM_BLOCK, DXGI_FORMAT_BC5_SNORM },
			{ 4u, 4u, 16u, true, GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT, 0, 0, VK_FORMAT_BC6H_UFLOAT_BLOCK, DXGI_FORMAT_BC6H_UF16 },
			{ 4u, 4u, 16u, true, GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT, 0, 0, VK_FORMAT_BC6H_SFLOAT_BLOCK, DXGI_FORMAT_BC6H_SF16 },
			{ 4u, 4u, 16u, true, GL_COMPRESSED_RGBA_BPTC_UNORM, 0, 0, VK_FORMAT_BC7_UNORM_BLOCK, DXGI_FORMAT_BC7_UNORM },
			{ 4u, 4u, 16u, true, GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM, 0, 0, VK_FORMAT_BC7_SRGB_BLOCK, DXGI_FORMAT_BC7_UNORM_SRGB },

			{ 4u, 4u, 8u, true, GL_COMPRESSED_RGB8_ETC2, 0, 0, VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK, DXGI_FORMAT_UNKNOWN },
			{ 4u, 4u, 8u, true, GL_COMPRESSED_SRGB8_ETC2, 0, 0, VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK, DXGI_FORMAT_UNKNOWN },
			{ 4u, 4u, 8u, true, GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2, 0, 0, VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK, DXGI_FORMAT_UNKNOWN },
			{ 4u, 4u, 8u, true, GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2, 0, 0, VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK, DXGI_FORMAT_UNKNOWN },
			{ 4u, 4u, 16u, true, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0, VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK, DXGI_FORMAT_UNKNOWN },
			{ 4u, 4u, 16u, true, GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC, 0, 0, VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK, DXGI_FORMAT_UNKNOWN },
			{ 4u, 4u, 8u, true, GL_COMPRESSED_R11_EAC, 0, 0, VK_FORMAT_EAC_R11_UNORM_BLOCK, DXGI_FORMAT_UNKNOWN },
			{ 4u, 4u, 8u, true, GL_COMPRESSED_SIGNED_R11_EAC, 0, 0, VK_FORMAT_EAC_R11_SNORM_BLOCK, DXGI_FORMAT_UNKNOWN },
			{ 4u, 4u, 16u, true, GL_COMPRESSED_RG11_EAC, 0, 0, VK_FORMAT_EAC_R11G11_UNORM_BLOCK, DXGI_FORMAT_UNKNOWN },
			{ 4u, 4u, 16u, true, GL_COMPRESSED_SIGNED_RG11_EAC, 0, 0, VK_FORMAT_EAC_R11G11_SNORM_BLOCK, DXGI_FORMAT_UNKNOWN },

			{ 4u, 4u, 16u, true, GL_COMPRESSED_RGBA_ASTC_4x4_KHR, 0, 0, VK_FORMAT_ASTC_4x4_UNORM_BLOCK, DXGI_FORMAT_UNKNOWN },
			{ 4u, 4u, 16u, true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR, 0, 0, VK_FORMAT_ASTC_4x4_SRGB_BLOCK, DXGI_FORMAT_UNKNOWN },
			{ 4u, 4u, 16u, true, GL_COMPRESSED_RGBA_ASTC_4x4_KHR, 0, 0, VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK, DXGI_FORMAT_UNKNOWN },
			{ 5u, 4u, 16u, true, GL_COMPRESSED_RGBA_ASTC_5x4_KHR, 0, 0, VK_FORMAT_ASTC_5x4_UNORM_BLOCK, DXGI_FORMAT_UNKNOWN },
			{ 5u, 4u, 16u, true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR, 0, 0, VK_FORMAT_ASTC_5x4_SRGB_BLOCK, DXGI_FORMAT_UNKNOWN },
			{ 5u, 4u, 16u, true, GL_COMPRESSED_RGBA_ASTC_5x4_KHR, 0, 0, VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK, DXGI_FORMAT_UNKNOWN },
			{ 5u, 5u, 16u, true, GL_COMPRESSED_RGBA_ASTC_5x5_KHR, 0, 0, VK_FORMAT_ASTC_5x5_UNORM_BLOCK, DXGI_FORMAT_UNKNOWN },
			{ 5u, 5u, 16u, true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR, 0, 0, VK_FORMAT_ASTC_5x5_SRGB_BLOCK, DXGI_FORMAT_UNKNOWN },
			{ 5u, 5u, 16u, true, GL_COMPRESSED_RGBA_ASTC_5x5_KHR, 0, 0, VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK, DXGI_FORMAT_UNKNOWN },
			{ 6u, 5u, 16u, true, GL_COMPRESSED_RGBA_ASTC_6x5_KHR, 0, 0, VK_FORMAT_ASTC_6x5_UNORM_BLOCK, DXGI_FORMAT_UNKNOWN },
			{ 6u, 5u, 16u, true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR, 0, 0, VK_FORMAT_ASTC_6x5_SRGB_BLOCK, DXGI_FORMAT_UNKNOWN },
			{ 6u, 5u, 16u, true, GL_COMPRESSED_RGBA_ASTC_6x5_KHR, 0, 0, VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK, DXGI_FORMAT_UNKNOWN },
			{ 6u, 6u, 16u, true, GL_COMPRESSED_RGBA_ASTC_6x6_KHR, 0, 0, VK_FORMAT_ASTC_6x6_UNORM_BLOCK, DXGI_FORMAT_UNKNOWN },
			{ 6u, 6u, 16u, true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR, 0, 0, VK_FORMAT_ASTC_6x6_SRGB_BLOCK, DXGI_FORMAT_UNKNOWN },
			{ 6u, 6u, 16u, true, GL_COMPRESSED_RGBA_ASTC_6x6_KHR, 0, 0, VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK, DXGI_FORMAT_UNKNOWN },
			{ 8u, 5u, 16u, true, GL_COMPRESSED_RGBA_ASTC_8x5_KHR, 0, 0, VK_FORMAT_ASTC_8x5_UNORM_BLOCK, DXGI_FORMAT_UNKNOWN },
			{ 8u, 5u, 16u, true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR, 0, 0, VK_FORMAT_ASTC_8x5_SRGB_BLOCK, DXGI_FORMAT_UNKNOWN },
			{ 8u, 5u, 16u, true, GL_COMPRESSED_RGBA_ASTC_8x5_KHR, 0, 0, VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK, DXGI_FORMAT_UNKNOWN },
			{ 8u, 6u, 16u, true, GL_COMPRESSED_RGBA_ASTC_8x6_KHR, 0, 0, VK_FORMAT_ASTC_8x6_UNORM_BLOCK, DXGI_FORMAT_UNKNOWN },
			{ 8u, 6u, 16u, true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR, 0, 0, VK_FORMAT_ASTC_8x6_SRGB_BLOCK, DXGI_FORMAT_UNKNOWN },
			{ 8u, 6u, 16u, true, GL_COMPRESSED_RGBA_ASTC_8x6_KHR, 0, 0, VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK, DXGI_FORMAT_UNKNOWN },
			{ 8u, 8u, 16u, true, GL_COMPRESSED_RGBA_ASTC_8x8_KHR, 0, 0, VK_FORMAT_ASTC_8x8_UNORM_BLOCK, DXGI_FORMAT_UNKNOWN },
			{ 8u, 8u, 16u, true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR, 0, 0, VK_FORMAT_ASTC_8x8_SRGB_BLOCK, DXGI_FORMAT_UNKNOWN },
			{ 8u, 8u, 16u, true, GL_COMPRESSED_RGBA_ASTC_8x8_KHR, 0, 0, VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK, DXGI_FORMAT_UNKNOWN },
			{ 10u, 5u, 16u, true, GL_COMPRESSED_RGBA_ASTC_10x5_KHR, 0, 0, VK_FORMAT_ASTC_10x5_UNORM_BLOCK, DXGI_FORMAT_UNKNOWN },
			{ 10u, 5u, 16u, true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR, 0, 0, VK_FORMAT_ASTC_10x5_SRGB_BLOCK, DXGI_FORMAT_UNKNOWN },
			{ 10u, 5u, 16u, true, GL_COMPRESSED_RGBA_ASTC_10x5_KHR, 0, 0, VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK, DXGI_FORMAT_UNKNOWN },
			{ 10u, 6u, 16u, true, GL_COMPRESSED_RGBA_ASTC_10x6_KHR, 0, 0, VK_FORMAT_ASTC_10x6_UNORM_BLOCK, DXGI_FORMAT_UNKNOWN },
			{ 10u, 6u, 16u, true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR, 0, 0, VK_FORMAT_ASTC_10x6_SRGB_BLOCK, DXGI_FORMAT_UNKNOWN },
			{ 10u, 6u, 16u, true, GL_COMPRESSED_RGBA_ASTC_10x6_KHR, 0, 0, VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK, DXGI_FORMAT_UNKNOWN },
			{ 10u, 8u, 16u, true, GL_COMPRESSED_RGBA_ASTC_10x8_KHR, 0, 0, VK_FORMAT_ASTC_10x8_UNORM_BLOCK, DXGI_FORMAT_UNKNOWN },
			{ 10u, 8u, 16u, true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR, 0, 0, VK_FORMAT_ASTC_10x8_SRGB_BLOCK, DXGI_FORMAT_UNKNOWN },
			{ 10u, 8u, 16u, true, GL_COMPRESSED_RGBA_ASTC_10x8_KHR, 0, 0, VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK, DXGI_FORMAT_UNKNOWN },
			{ 10u, 10u, 16u, true, GL_COMPRESSED_RGBA_ASTC_10x10_KHR, 0, 0, VK_FORMAT_ASTC_10x10_UNORM_BLOCK, DXGI_FORMAT_UNKNOWN },
			{ 10u, 10u, 16u, true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR, 0, 0, VK_FORMAT_ASTC_10x10_SRGB_BLOCK, DXGI_FORMAT_UNKNOWN },
			{ 10u, 10u, 16u, true, GL_COMPRESSED_RGBA_ASTC_10x10_KHR, 0, 0, VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK, DXGI_FORMAT_UNKNOWN },
			{ 12u, 10u, 16u, true, GL_COMPRESSED_RGBA_ASTC_12x10_KHR, 0, 0, VK_FORMAT_ASTC_12x10_UNORM_BLOCK, DXGI_FORMAT_UNKNOWN },
			{ 12u, 10u, 16u, true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR, 0, 0, VK_FORMAT_ASTC_12x10_SRGB_BLOCK, DXGI_FORMAT_UNKNOWN },
			{ 12u, 10u, 16u, true, GL_COMPRESSED_RGBA_ASTC_12x10_KHR, 0, 0, VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK, DXGI_FORMAT_UNKNOWN },
			{ 12u, 12u, 16u, true, GL_COMPRESSED_RGBA_ASTC_12x12_KHR, 0, 0, VK_FORMAT_ASTC_12x12_UNORM_BLOCK, DXGI_FORMAT_UNKNOWN },
			{ 12u, 12u, 16u, true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR, 0, 0, VK_FORMAT_ASTC_12x12_SRGB_BLOCK, DXGI_FORMAT_UNKNOWN },
			{ 12u, 12u, 16u, true, GL_COMPRESSED_RGBA_ASTC_12x12_KHR, 0, 0, VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK, DXGI_FORMAT_UNKNOWN },

			{ 1u, 1u, 2u, false, GL_RGBA4, GL_BGRA, GL_UNSIGNED_SHORT_4_4_4_4_REV, VK_FORMAT_A4R4G4B4_UNORM_PACK16, DXGI_FORMAT_UNKNOWN },
			{ 1u, 1u, 2u, false, GL_RGBA4, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4_REV, VK_FORMAT_A4B4G4R4_UNORM_PACK16, DXGI_FORMAT_UNKNOWN },
			{ 1u, 1u, 2u, false, GL_RGB5_A1, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, VK_FORMAT_A1B5G5R5_UNORM_PACK16, DXGI_FORMAT_UNKNOWN },
			{ 1u, 1u, 1u, false, GL_ALPHA8_EXT, GL_ALPHA, GL_UNSIGNED_BYTE, VK_FORMAT_A8_UNORM, DXGI_FORMAT_A8_UNORM }
		};

		static const ImageFormatDesc* find_format_entry_by_vk(u32 vk_format) {
			for (const auto& entry : g_format_table) {
				if (entry.vk_format == vk_format) {
					return &entry;
				}
			}
			return nullptr;
		}

		static const ImageFormatDesc* find_format_entry_by_gl(u32 gl_internal) {
			for (const auto& entry : g_format_table) {
				if (entry.gl_internal_format == gl_internal) {
					return &entry;
				}
			}
			return nullptr;
		}

		static const ImageFormatDesc* find_format_entry_by_dxgi(u32 dxgi_format) {
			for (const auto& entry : g_format_table) {
				if (entry.dxgi_format == dxgi_format) {
					return &entry;
				}
			}
			return nullptr;
		}
	}

	ImageHeader::Result ImageHeader::open_strem(const char* path, const char* mode) {
		stream = fopen(path, mode);
		if (!stream) {
			return Result::BadStream;
		}

		stream_owner = true;
		return Result::Success;
	}

	void ImageHeader::close_stream() {
		if (stream && stream_owner) {
			fclose(stream);
		}
	}

	usize ImageHeader::read_bytes(void* dst, usize bytes_to_read) const {
		return dst ? fread(dst, 1, bytes_to_read, stream) : 0;
	}

	bool ImageHeader::try_read_bytes(void* dst, usize bytes_to_read) const {
		return dst ? fread(dst, 1, bytes_to_read, stream) == bytes_to_read : false;
	}

	usize ImageHeader::get_size() const {
		usize whole_size = 0;
		for (usize i = 0; i < static_cast<usize>(mip_levels); ++i) {
			u32 mip_width = max(base_width >> static_cast<u32>(i), 1u);
			u32 mip_height = max(base_height >> static_cast<u32>(i), 1u);
			u32 mip_depth = max(base_depth >> static_cast<u32>(i), 1u);

			whole_size += format_desc->comp_size(mip_width, mip_height, mip_depth) * array_layers * face_count;
		}
		return whole_size;
	}

	ImageHeader::Result ImageHeader::read_header(NotNull<const Allocator*> alloc) {
		u8 buffer[detail::max_ident_size];
		if (!try_read_bytes(buffer, detail::max_ident_size)) {
			return Result::InvalidHeader;
		}

		if (memcmp(buffer, detail::dds::IDENTIFIER, detail::dds::ident_size) == 0) {
			source_container_type = ContainerType::DDS;

			fseek(stream, detail::dds::ident_size, SEEK_SET);
			return read_header_dds(alloc);
		}
		else if (memcmp(buffer, detail::ktx1::IDENTIFIER, detail::ktx1::ident_size) == 0) {
			source_container_type = ContainerType::KTX_1_0;

			fseek(stream, detail::ktx1::ident_size, SEEK_SET);
			return read_header_ktx1(alloc);
		}
		else if (memcmp(buffer, detail::internal::IDENTIFIER, detail::internal::ident_size) == 0) {
			source_container_type = ContainerType::Internal;

			fseek(stream, detail::internal::ident_size, SEEK_SET);
			return read_header_internal(alloc);
		}

		return Result::UnsupportedFileFormat;
	}

	ImageHeader::Result ImageHeader::read_data(NotNull<const Allocator*> alloc, IImageConsumer* output) {
		switch (source_container_type)
		{
		case edge::ImageHeader::ContainerType::KTX_1_0:
			return read_data_ktx1(alloc, output);
		case edge::ImageHeader::ContainerType::DDS:
			return read_data_dds(alloc, output);
		case edge::ImageHeader::ContainerType::Internal:
			return read_data_internal(alloc, output);
		default:
			return Result::UnsupportedFileFormat;
		}
	}

	ImageHeader::Result ImageHeader::read_header_dds(NotNull<const Allocator*> alloc) {
		using namespace detail::dds;

		Header header;
		if (!try_read_bytes(&header, header_size)) {
			return Result::InvalidHeader;
		}

		base_width = header.width;
		base_height = max(header.height, 1u);
		base_depth = max(header.depth, 1u);
		mip_levels = max(header.mip_map_count, 1u);

		bool fourcc_pixel_format = header.ddspf.flags & DDS_PIXEL_FORMAT_FOUR_CC_FLAG_BIT;

		if (fourcc_pixel_format && (header.ddspf.fourcc == FOURCC_DX10)) {
			HeaderDXT10 header_dxt10;
			if (!try_read_bytes(&header_dxt10, header_dxt10_size)) {
				return Result::InvalidHeader;
			}

			format_desc = detail::find_format_entry_by_dxgi(header_dxt10.dxgi_format);
			if (!format_desc) {
				return Result::UnsupportedPixelFormat;
			}

			array_layers = max(header_dxt10.array_size, 1u);

			bool is_cubemap = (header_dxt10.misc_flag & DDS_MISC_TEXTURE_CUBE_FLAG_BIT) == DDS_MISC_TEXTURE_CUBE_FLAG_BIT;
			face_count = is_cubemap ? 6u : 1u;
		}
		else {
			auto dxgi_format = header.ddspf.get_format();

			format_desc = detail::find_format_entry_by_dxgi(dxgi_format);
			if (!format_desc) {
				return Result::UnsupportedPixelFormat;
			}

			bool is_cubemap = header.caps2 & DDS_CAPS2_CUBEMAP_FLAG_BIT;
			if (is_cubemap) {
				face_count = 6u;
			}
		}

		// TODO: same for ktx and dds
		

		return Result::Success;
	}

	ImageHeader::Result ImageHeader::read_data_dds(NotNull<const Allocator*> alloc, IImageConsumer* output) {
		if (!output) {
			return Result::OutOfMemory;
		}

		if (!output->prepare(alloc, *this)) {
			return Result::OutOfMemory;
		}

		// Load for match ktx-like layout
		for (u32 layer = 0; layer < array_layers * face_count; ++layer) {
			for (u32 mip = 0; mip < mip_levels; ++mip) {
				if (!output->read(alloc, *this, mip, layer, 1)) {
					return Result::UnexpectedEndOfStream;
				}
			}
		}

		return Result::Success;


		//usize mip_face_size = format_desc->comp_size(mip_width, mip_height, mip_depth);
		//
		//LevelInfo& level_info = level_infos[mip];
		//usize subresource_offset = (layer * face_count + face) * mip_face_size;
		//u8* dest_ptr = image_data + level_info.offset + subresource_offset;
		//
		//bytes_readed = fread(dest_ptr, 1, mip_face_size, stream);
		//if (bytes_readed != mip_face_size) {
		//	return Result::UnexpectedEndOfStream;
		//}
	}

	ImageHeader::Result ImageHeader::read_header_ktx1(NotNull<const Allocator*> alloc) {
		using namespace detail::ktx1;

		Header header;
		if (!try_read_bytes(&header, header_size)) {
			return Result::InvalidHeader;
		}

		bool reversed_endian = header.endianness == KTX_ENDIAN_REF_REV;
		if (reversed_endian) {
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

		format_desc = detail::find_format_entry_by_gl(header.gl_internal_format);
		if (!format_desc) {
			return Result::UnsupportedPixelFormat;
		}

		base_width = header.pixel_width;
		base_height = max(header.pixel_height, 1u);
		base_depth = max(header.pixel_depth, 1u);
		mip_levels = max(header.number_of_mipmap_levels, 1u);
		array_layers = max(header.number_of_array_elements, 1u);
		face_count = max(header.number_of_faces, 1u);

		// TODO: Figure out how to read so that it can be done externally. (Request the memory area to read, etc.)
#if 0
		image_data = (u8*)alloc->malloc(data_size, 1);
		if (!image_data) {
			return Result::OutOfMemory;
		}

		// Skip metadata for now
		fseek(stream.m_ptr, header.bytes_of_key_value_data, SEEK_CUR);

		for (u32 mip = 0; mip < mip_levels; ++mip) {
			u32 level_data_size;
			bytes_readed = fread(&level_data_size, 1, sizeof(u32), stream.m_ptr);
			if (bytes_readed != sizeof(u32)) {
				return Result::UnexpectedEndOfStream;
			}

			if (reversed_endian) {
				level_data_size = detail::swap_u32(level_data_size);
			}

			LevelInfo& level_info = level_infos[mip];

			if (level_data_size != level_info.size) {
				return Result::InvalidHeader;
			}

			bytes_readed = fread(image_data + level_info.offset, 1, level_data_size, stream.m_ptr);
			if (bytes_readed != level_data_size) {
				return Result::UnexpectedEndOfStream;
			}

			usize cur_offset = ftell(stream.m_ptr);
			fseek(stream.m_ptr, (cur_offset + 3) & ~3, SEEK_SET);
		}
#endif

		return Result::Success;
	}

	ImageHeader::Result ImageHeader::read_data_ktx1(NotNull<const Allocator*> alloc, IImageConsumer* output) {

	}

	ImageHeader::Result ImageHeader::read_header_internal(NotNull<const Allocator*> alloc) {
		using namespace detail::internal;

		Header header;
		if (!try_read_bytes(&header, header_size)) {
			return Result::InvalidHeader;
		}

		format_desc = detail::find_format_entry_by_vk(header.vk_format);
		if (!format_desc) {
			return Result::UnsupportedPixelFormat;
		}

		base_width = header.pixel_width;
		base_height = max(header.pixel_height, 1u);
		base_depth = max(header.pixel_depth, 1u);
		mip_levels = max(header.number_of_mipmap_levels, 1u);
		array_layers = max(header.number_of_array_elements, 1u);
		face_count = max(header.number_of_faces, 1u);

		// TODO: Figure out how to read so that it can be done externally. (Request the memory area to read, etc.)
#if 0
		image_data = (u8*)alloc->malloc(data_size, 1);
		if (!image_data) {
			return Result::OutOfMemory;
		}

		LZ4F_CustomMem custom_alloc = {
			.customAlloc = [](void* opaqueState, usize size) -> void* {
				const Allocator* alloc = (const Allocator*)opaqueState;
				return alloc->malloc(size, 1);
			},
			.customCalloc = nullptr,
			.customFree = [](void* opaqueState, void* address) -> void {
				const Allocator* alloc = (const Allocator*)opaqueState;
				alloc->free(address);
			},
			.opaqueState = (void*)alloc.m_ptr
		};

		LZ4F_dctx* lz4dctx = LZ4F_createDecompressionContext_advanced(custom_alloc, LZ4F_VERSION);
		if (!lz4dctx) {
			return TextureSource::Result::OutOfMemory;
		}

		char* in_out_buffer = (char*)alloc->malloc(chunk_size * 2, 1);
		if (!in_out_buffer) {
			LZ4F_freeDecompressionContext(lz4dctx);
			return TextureSource::Result::OutOfMemory;
		}

		for (u32 mip = 0; mip < mip_levels; ++mip) {
			u32 frame_size;
			bytes_readed = fread(&frame_size, 1, sizeof(u32), stream.m_ptr);
			if (bytes_readed != sizeof(u32)) {
				alloc->free(in_out_buffer);
				LZ4F_freeDecompressionContext(lz4dctx);
				return Result::UnexpectedEndOfStream;
			}

			LevelInfo& level_info = level_infos[mip];

			Result decompression_result = decompress_lz4(alloc, stream, lz4dctx, chunk_size, in_out_buffer, frame_size, level_info.size, image_data + level_info.offset);
			if (decompression_result != Result::Success) {
				alloc->free(in_out_buffer);
				LZ4F_freeDecompressionContext(lz4dctx);
				return decompression_result;
			}

			usize cur_offset = ftell(stream.m_ptr);
			fseek(stream.m_ptr, (cur_offset + 3) & ~3, SEEK_SET);
		}

		alloc->free(in_out_buffer);
		LZ4F_freeDecompressionContext(lz4dctx);
#endif

		return Result::Success;
	}

	ImageHeader::Result ImageHeader::read_data_internal(NotNull<const Allocator*> alloc, IImageConsumer* output) {

	}

	ImageHeader::Result ImageHeader::write_header_internal(NotNull<const Allocator*> alloc) {
		using namespace detail::internal;

		usize bytes_written = fwrite(IDENTIFIER, 1, ident_size, stream);
		if (bytes_written != ident_size) {
			return Result::BadStream;
		}

		Header header = {
			.vk_format = format_desc->vk_format,
			.pixel_width = base_width,
			.pixel_height = base_height,
			.pixel_depth = base_depth,
			.number_of_array_elements = array_layers,
			.number_of_faces = face_count,
			.number_of_mipmap_levels = mip_levels,
			.compression = ETexCompressionMethod::LZ4
		};

		bytes_written = fwrite(&header, 1, header_size, stream);
		if (bytes_written != header_size) {
			return Result::BadStream;
		}

		return Result::Success;
	}

	ImageHeader::Result ImageHeader::write_internal_stream(NotNull<const Allocator*> alloc) {
		// TODO: Write data
		using namespace detail::internal;

		

		LZ4F_CustomMem custom_alloc = {
			.customAlloc = [](void* opaqueState, usize size) -> void* {
				const Allocator* alloc = (const Allocator*)opaqueState;
				return alloc->malloc(size, 1);
			},
			.customCalloc = nullptr,
			.customFree = [](void* opaqueState, void* address) -> void {
				const Allocator* alloc = (const Allocator*)opaqueState;
				alloc->free(address);
			},
			.opaqueState = (void*)alloc.m_ptr
		};

		LZ4F_cctx* lz4cctx = LZ4F_createCompressionContext_advanced(custom_alloc, LZ4F_VERSION);
		if (!lz4cctx) {
			return Result::OutOfMemory;
		}

#if 0
		for (usize mip = 0; mip < mip_levels; ++mip) {
			LevelInfo& level_info = level_infos[mip];

			usize frame_start_offset = ftell(stream.m_ptr);
			fseek(stream.m_ptr, frame_start_offset + sizeof(u32), SEEK_SET);

			u32 compressed_size;
			Result compression_result = compress_lz4(alloc, stream, lz4cctx, chunk_size, image_data + level_info.offset, level_info.size, compressed_size);
			if (compression_result != Result::Success) {
				LZ4F_freeCompressionContext(lz4cctx);
				return compression_result;
			}

			usize frame_end_offset = ftell(stream.m_ptr);
			frame_end_offset = (frame_end_offset + 3) & ~3;

			fseek(stream.m_ptr, frame_start_offset, SEEK_SET);

			bytes_written = fwrite(&compressed_size, 1, sizeof(u32), stream.m_ptr);
			if (bytes_written != sizeof(u32)) {
				LZ4F_freeCompressionContext(lz4cctx);
				return Result::BadStream;
			}

			fseek(stream.m_ptr, frame_end_offset, SEEK_SET);
		}
#endif

		LZ4F_freeCompressionContext(lz4cctx);

		return Result::Success;
	}
}