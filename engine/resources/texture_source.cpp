#include "texture_source.h"

#include "gl/glcorearb.h"
#include "dxgi/dxgiformat.h"

#include <math.hpp>

namespace edge {
	namespace detail {
		namespace ktx1 {
			constexpr u32 KTX_ENDIAN_REF = 0x04030201;
			constexpr u32 KTX_ENDIAN_REF_REV = 0x01020304;

			constexpr u8 IDENTIFIER[] = {
				0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A
			};

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
		}

		namespace ktx2 {
			constexpr u8 IDENTIFIER[] = {
				0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A
			};

			// static constexpr usize header_size = sizeof(Header);
		}

		namespace dds {
			constexpr u8 IDENTIFIER[] = {
				0x44, 0x44, 0x53, 0x20
			};

			constexpr u32 FOURCC_DXT1 = 0x31545844;
			constexpr u32 FOURCC_DXT2 = 0x32545844;
			constexpr u32 FOURCC_DXT3 = 0x33545844;
			constexpr u32 FOURCC_DXT4 = 0x34545844;
			constexpr u32 FOURCC_DXT5 = 0x35545844;
			constexpr u32 FOURCC_ATI1 = 0x31495441;
			constexpr u32 FOURCC_BC4U = 0x55344342;
			constexpr u32 FOURCC_BC4S = 0x53344342;
			constexpr u32 FOURCC_ATI2 = 0x32495441;
			constexpr u32 FOURCC_BC5U = 0x55354342;
			constexpr u32 FOURCC_BC5S = 0x53354342;
			constexpr u32 FOURCC_BC6H = 0x48364342;
			constexpr u32 FOURCC_BC7L = 0x4C374342;
			constexpr u32 FOURCC_DX10 = 0x30315844;

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

				DXGI_FORMAT get_format() const noexcept {
					if (flags & DDS_PIXEL_FORMAT_FOUR_CC_FLAG_BIT) {
						switch (fourcc)
						{
						case FOURCC_DXT1: return DXGI_FORMAT_BC1_UNORM;
						case FOURCC_DXT2:
						case FOURCC_DXT3: return DXGI_FORMAT_BC2_UNORM;
						case FOURCC_DXT4:
						case FOURCC_DXT5: return DXGI_FORMAT_BC3_UNORM;
						case FOURCC_ATI1:
						case FOURCC_BC4U: return DXGI_FORMAT_BC4_UNORM;
						case FOURCC_BC4S: return DXGI_FORMAT_BC4_SNORM;
						case FOURCC_ATI2:
						case FOURCC_BC5U: return DXGI_FORMAT_BC5_UNORM;
						case FOURCC_BC5S: return DXGI_FORMAT_BC5_SNORM;
						case FOURCC_BC6H: return DXGI_FORMAT_BC6H_UF16;
						case FOURCC_BC7L: return DXGI_FORMAT_BC7_UNORM;
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
					else if (flags & DDS_PIXEL_FORMAT_RGB_FLAG_BIT) {
						switch (rgb_bit_count) {
						case 32:
							if (r_bit_mask == 0x000000ff && g_bit_mask == 0x0000ff00 &&
								b_bit_mask == 0x00ff0000 && a_bit_mask == 0xff000000) {
								return DXGI_FORMAT_R8G8B8A8_UNORM;
							}
							if (r_bit_mask == 0x00ff0000 && g_bit_mask == 0x0000ff00 &&
								b_bit_mask == 0x000000ff && a_bit_mask == 0xff000000) {
								return DXGI_FORMAT_B8G8R8A8_UNORM;
							}
							if (r_bit_mask == 0x00ff0000 && g_bit_mask == 0x0000ff00 &&
								b_bit_mask == 0x000000ff && a_bit_mask == 0x00000000) {
								return DXGI_FORMAT_B8G8R8X8_UNORM;
							}
							if (r_bit_mask == 0x3ff00000 && g_bit_mask == 0x000ffc00 &&
								b_bit_mask == 0x000003ff && a_bit_mask == 0xc0000000) {
								return DXGI_FORMAT_R10G10B10A2_UNORM;
							}
							break;
						case 16:
							if (r_bit_mask == 0xf800 && g_bit_mask == 0x07e0 && b_bit_mask == 0x001f) {
								return DXGI_FORMAT_B5G6R5_UNORM;
							}
							if (r_bit_mask == 0x7c00 && g_bit_mask == 0x03e0 &&
								b_bit_mask == 0x001f && a_bit_mask == 0x8000) {
								return DXGI_FORMAT_B5G5R5A1_UNORM;
							}
							if (r_bit_mask == 0x0f00 && g_bit_mask == 0x00f0 &&
								b_bit_mask == 0x000f && a_bit_mask == 0xf000) {
								return DXGI_FORMAT_B4G4R4A4_UNORM;
							}
							break;
						}
					}
					else if (flags & DDS_PIXEL_FORMAT_ALPHA_FLAG_BIT) {
						return DXGI_FORMAT_A8_UNORM;
					}
					else if (flags & DDS_PIXEL_FORMAT_LUMINANCE_FLAG_BIT) {
						if (rgb_bit_count == 8) {
							return DXGI_FORMAT_R8_UNORM;
						}
						else if (rgb_bit_count == 16) {
							if (r_bit_mask == 0xffff) {
								return DXGI_FORMAT_R16_UNORM;
							}
							if (r_bit_mask == 0x00ff && a_bit_mask == 0xff00) {
								return DXGI_FORMAT_R8G8_UNORM;
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

		namespace png {
			constexpr u8 IDENTIFIER[] = {
				0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A
			};
		}

		constexpr FormatInfo g_format_table[] = {
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

		static const FormatInfo* find_format_entry_by_vk(u32 vk_format) {
			for (const auto& entry : g_format_table) {
				if (entry.vk_format == vk_format) {
					return &entry;
				}
			}
			return nullptr;
		}

		static const FormatInfo* find_format_entry_by_gl(u32 gl_internal) {
			for (const auto& entry : g_format_table) {
				if (entry.gl_internal_format == gl_internal) {
					return &entry;
				}
			}
			return nullptr;
		}

		static const FormatInfo* find_format_entry_by_dxgi(u32 dxgi_format) {
			for (const auto& entry : g_format_table) {
				if (entry.dxgi_format == dxgi_format) {
					return &entry;
				}
			}
			return nullptr;
		}

		static u32 swap_u32(u32 val) {
			return ((val & 0xFF000000) >> 24) |
				((val & 0x00FF0000) >> 8) |
				((val & 0x0000FF00) << 8) |
				((val & 0x000000FF) << 24);
		}
	}

	bool TextureSource::from_stream(NotNull<const Allocator*> alloc, NotNull<FILE*> stream) noexcept {
		u8 buffer[16];
		usize bytes_readed = fread(buffer, 1, 16, stream.m_ptr);
		if (bytes_readed != 16) {
			return false;
		}

		if (memcmp(buffer, detail::png::IDENTIFIER, 8) == 0) {
			fseek(stream.m_ptr, 8, SEEK_SET);
			return false; // NOT SUPPORTED YET
		}
		else if (memcmp(buffer, detail::dds::IDENTIFIER, 4) == 0) {
			fseek(stream.m_ptr, 4, SEEK_SET);
			return from_dds_stream(alloc, stream);
		}
		else if (memcmp(buffer, detail::ktx1::IDENTIFIER, 12) == 0) {
			fseek(stream.m_ptr, 12, SEEK_SET);
			return from_ktx1_stream(alloc, stream);
		}
		else if (memcmp(buffer, detail::ktx2::IDENTIFIER, 12) == 0) {
			fseek(stream.m_ptr, 12, SEEK_SET);
			return from_ktx2_stream(alloc, stream);
		}

		return false;
	}

	bool TextureSource::from_dds_stream(NotNull<const Allocator*> alloc, NotNull<FILE*> stream) noexcept {
		using namespace detail::dds;

		Header header;
		usize bytes_readed = fread(&header, 1, header_size, stream.m_ptr);
		if (bytes_readed != header_size) {
			return false; // InvalidHeader or stream end
		}

		base_width = header.width;
		base_height = max(header.height, 1u);
		base_depth = max(header.depth, 1u);
		mip_levels = max(header.mip_map_count, 1u);

		bool fourcc_pixel_format = header.ddspf.flags & DDS_PIXEL_FORMAT_FOUR_CC_FLAG_BIT;

		if (fourcc_pixel_format && (header.ddspf.fourcc == FOURCC_DX10)) {
			HeaderDXT10 header_dxt10;
			bytes_readed = fread(&header_dxt10, 1, header_dxt10_size, stream.m_ptr);
			if (bytes_readed != header_dxt10_size) {
				return false; // InvalidHeader or stream end
			}

			fromat_info = detail::find_format_entry_by_dxgi(header_dxt10.dxgi_format);
			if (!fromat_info) {
				return false; // InvalidFormat / UnsupportedFormat
			}

			array_layers = max(header_dxt10.array_size, 1u);

			bool is_cubemap = (header_dxt10.misc_flag & DDS_MISC_TEXTURE_CUBE_FLAG_BIT) == DDS_MISC_TEXTURE_CUBE_FLAG_BIT;
			face_count = is_cubemap ? 6u : 1u;
		}
		else {
			auto dxgi_format = header.ddspf.get_format();

			fromat_info = detail::find_format_entry_by_dxgi(dxgi_format);
			if (!fromat_info) {
				return false; // InvalidFormat / UnsupportedFormat
			}

			bool is_cubemap = header.caps2 & DDS_CAPS2_CUBEMAP_FLAG_BIT;
			if (is_cubemap) {
				face_count = 6u;
			}
		}


		// TODO: same for ktx and dds
		for (u32 mip = 0; mip < mip_levels; ++mip) {
			LevelInfo& level_info = level_infos[mip];

			u32 mip_width = max(base_width >> mip, 1u);
			u32 mip_height = max(base_height >> mip, 1u);
			u32 mip_depth = max(base_depth >> mip, 1u);

			usize level_size = fromat_info->calculate_size(mip_width, mip_height, mip_depth);
			level_info.offset = data_size;
			level_info.size = level_size * array_layers * face_count;

			data_size += level_info.size;
		}

		image_data = (u8*)alloc->malloc(data_size, 1);
		if (!image_data) {
			return false; // OutOfMemory
		}

		// Load for match ktx-like layout
		for (u32 layer = 0; layer < array_layers; ++layer) {
			for (u32 face = 0; face < face_count; ++face) {
				for (u32 mip = 0; mip < mip_levels; ++mip) {
					u32 mip_width = max(base_width >> mip, 1u);
					u32 mip_height = max(base_height >> mip, 1u);
					u32 mip_depth = max(base_depth >> mip, 1u);

					usize mip_face_size = fromat_info->calculate_size(mip_width, mip_height, mip_depth);

					LevelInfo& level_info = level_infos[mip];
					usize subresource_offset = (layer * face_count + face) * mip_face_size;
					u8* dest_ptr = image_data + level_info.offset + subresource_offset;

					bytes_readed = fread(dest_ptr, 1, mip_face_size, stream.m_ptr);
					if (bytes_readed != mip_face_size) {
						return false; // Unexpected eof/eos
					}
				}
			}
		}

		return true;
	}

	bool TextureSource::from_ktx1_stream(NotNull<const Allocator*> alloc, NotNull<FILE*> stream) noexcept {
		using namespace detail::ktx1;

		Header header;
		usize bytes_readed = fread(&header, 1, header_size, stream.m_ptr);
		if (bytes_readed != header_size) {
			return false; // InvalidHeader or stream end
		}

		bool reversed_endian = header.endianness == KTX_ENDIAN_REF_REV;
		if (reversed_endian) {
			header.gl_type = detail::swap_u32(header.gl_type);
			header.gl_type_size = detail::swap_u32(header.gl_type_size);
			header.gl_format = detail::swap_u32(header.gl_format);
			header.gl_internal_format = detail::swap_u32(header.gl_internal_format);
			header.gl_base_internal_format = detail::swap_u32(header.gl_base_internal_format);
			header.pixel_width = detail::swap_u32(header.pixel_width);
			header.pixel_height = detail::swap_u32(header.pixel_height);
			header.pixel_depth = detail::swap_u32(header.pixel_depth);
			header.number_of_array_elements = detail::swap_u32(header.number_of_array_elements);
			header.number_of_faces = detail::swap_u32(header.number_of_faces);
			header.number_of_mipmap_levels = detail::swap_u32(header.number_of_mipmap_levels);
			header.bytes_of_key_value_data = detail::swap_u32(header.bytes_of_key_value_data);
		}

		fromat_info = detail::find_format_entry_by_gl(header.gl_internal_format);
		if (!fromat_info) {
			return false; // InvalidFormat / UnsupportedFormat
		}

		base_width = header.pixel_width;
		base_height = max(header.pixel_height, 1u);
		base_depth = max(header.pixel_depth, 1u);
		mip_levels = max(header.number_of_mipmap_levels, 1u);
		array_layers = max(header.number_of_array_elements, 1u);
		face_count = max(header.number_of_faces, 1u);
		
		for (u32 mip = 0; mip < mip_levels; ++mip) {
			LevelInfo& level_info = level_infos[mip];
		
			u32 mip_width = max(base_width >> mip, 1u);
			u32 mip_height = max(base_height >> mip, 1u);
			u32 mip_depth = max(base_depth >> mip, 1u);
		
			usize level_size = fromat_info->calculate_size(mip_width, mip_height, mip_depth);
			level_info.offset = data_size;
			level_info.size = level_size * array_layers * face_count;
		
			data_size += level_info.size;
		}

		image_data = (u8*)alloc->malloc(data_size, 1);
		if (!image_data) {
			return false; // OutOfMemory
		}

		// Skip metadata for now
		fseek(stream.m_ptr, header.bytes_of_key_value_data, SEEK_CUR);

		for (u32 mip = 0; mip < mip_levels; ++mip) {
			u32 level_data_size;
			bytes_readed = fread(&level_data_size, 1, sizeof(u32), stream.m_ptr);
			if (bytes_readed != sizeof(u32)) {
				return false; // Unexpected eof
			}

			if (reversed_endian) {
				level_data_size = detail::swap_u32(level_data_size);
			}

			LevelInfo& level_info = level_infos[mip];

			if (level_data_size != level_info.size) {
				return false; // InvalidHeader
			}

			bytes_readed = fread(image_data + level_info.offset, 1, level_data_size, stream.m_ptr);
			if (bytes_readed != level_data_size) {
				return false; // Unexpected eof/eos
			}

			usize cur_offset = ftell(stream.m_ptr);
			fseek(stream.m_ptr, (cur_offset + 3) & ~3, SEEK_SET);
		}

		return true;
	}

	bool TextureSource::from_ktx2_stream(NotNull<const Allocator*> alloc, NotNull<FILE*> stream) noexcept {
		return false;
	}

	void TextureSource::destroy(NotNull<const Allocator*> alloc) noexcept {
		if (image_data) {
			alloc->free(image_data);
		}
	}

	TextureSource::SubresourceInfo TextureSource::get_subresource_data_ptr(u32 level, u32 layer, u32 face) noexcept {
		if (level >= mip_levels || layer >= array_layers || face >= face_count) {
			return {};
		}
		
		LevelInfo& level_info = level_infos[level];
		
		usize face_size = level_info.size / (array_layers * face_count);
		usize subresource_offset = (layer * face_count + face) * face_size;
		
		return {
			.data = image_data + level_info.offset + subresource_offset,
			.size = face_size
		};
	}
}