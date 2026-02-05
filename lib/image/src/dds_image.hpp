#ifndef EDGE_DDS_IMAGE_H
#define EDGE_DDS_IMAGE_H

#include "image.hpp"
#include "image_format.hpp"

#include <math.hpp>

#define MAKE_FOURCC(a, b, c, d)                                                \
  ((u32)(a) | ((u32)(b) << 8) | ((u32)(c) << 16) | ((u32)(d) << 24))

namespace edge {
namespace detail::dds {
constexpr u8 IDENTIFIER[] = {0x44, 0x44, 0x53, 0x20};

static constexpr usize ident_size = sizeof(IDENTIFIER);

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

enum HeaderFlagBits : u32 {
  DDS_HEADER_CAPS_FLAG_BIT = 0x1,
  DDS_HEADER_HEIGHT_FLAG_BIT = 0x2,
  DDS_HEADER_WIDTH_FLAG_BIT = 0x4,
  DDS_HEADER_PITCH_FLAG_BIT = 0x8,
  DDS_HEADER_PIXEL_FORMAT_FLAG_BIT = 0x1000,
  DDS_HEADER_MIP_MAP_COUNT_FLAG_BIT = 0x20000,
  DDS_HEADER_LINEAR_SIZE_FLAG_BIT = 0x80000,
  DDS_HEADER_DEPTH_FLAG_BIT = 0x800000
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
      switch (fourcc) {
      case FOURCC_DXT1:
        return DXGI_FORMAT_BC1_UNORM;
      case FOURCC_DXT2:
      case FOURCC_DXT3:
        return DXGI_FORMAT_BC2_UNORM;
      case FOURCC_DXT4:
      case FOURCC_DXT5:
        return DXGI_FORMAT_BC3_UNORM;
      case FOURCC_BC4U:
        return DXGI_FORMAT_BC4_UNORM;
      case FOURCC_BC4S:
        return DXGI_FORMAT_BC4_SNORM;
      case FOURCC_ATI2:
      case FOURCC_BC5U:
        return DXGI_FORMAT_BC5_UNORM;
      case FOURCC_BC5S:
        return DXGI_FORMAT_BC5_SNORM;
      case FOURCC_RGBG:
        return DXGI_FORMAT_R8G8_B8G8_UNORM;
      case FOURCC_GRGB:
        return DXGI_FORMAT_G8R8_G8B8_UNORM;
      case 36:
        return DXGI_FORMAT_R16G16B16A16_UNORM;
      case 110:
        return DXGI_FORMAT_R16G16B16A16_SNORM;
      case 111:
        return DXGI_FORMAT_R16_FLOAT;
      case 112:
        return DXGI_FORMAT_R16G16_FLOAT;
      case 113:
        return DXGI_FORMAT_R16G16B16A16_FLOAT;
      case 114:
        return DXGI_FORMAT_R32_FLOAT;
      case 115:
        return DXGI_FORMAT_R32G32_FLOAT;
      case 116:
        return DXGI_FORMAT_R32G32B32A32_FLOAT;
      default:
        return DXGI_FORMAT_UNKNOWN;
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
        if (r_bit_mask == 0x0000FFFF && g_bit_mask == 0xFFFF0000 &&
            b_bit_mask == 0x00000000) {
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
        if (r_bit_mask == 0xFF0000 && g_bit_mask == 0x00FF00 &&
            b_bit_mask == 0x0000FF) {
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
} // namespace detail::dds

struct DDSReader final : IImageReader {
  FILE *stream = nullptr;
  ImageInfo info = {};

  usize current_layer = 0;
  usize current_mip = 0;

  DDSReader(NotNull<FILE *> fstream) : stream{fstream.m_ptr} {}

  Result create(NotNull<const Allocator *> alloc) override {
    using namespace detail::dds;

    Header header;
    if (read_bytes(&header, header_size) != header_size) {
      return Result::InvalidHeader;
    }

    bool fourcc_pixel_format =
        header.ddspf.flags & DDS_PIXEL_FORMAT_FOUR_CC_FLAG_BIT;

    const ImageFormatDesc *format_desc = nullptr;
    u32 layer_count = 1, face_count = 1;
    if (fourcc_pixel_format && (header.ddspf.fourcc == FOURCC_DX10)) {
      HeaderDXT10 header_dxt10;
      if (read_bytes(&header_dxt10, header_dxt10_size) != header_dxt10_size) {
        return Result::InvalidHeader;
      }

      format_desc = detail::find_format_entry_by_dxgi(header_dxt10.dxgi_format);
      if (!format_desc) {
        return Result::InvalidPixelFormat;
      }

      if ((header_dxt10.misc_flag & DDS_MISC_TEXTURE_CUBE_FLAG_BIT) ==
          DDS_MISC_TEXTURE_CUBE_FLAG_BIT) {
        face_count *= 6;
      }
    } else {
      auto dxgi_format = header.ddspf.get_format();

      format_desc = detail::find_format_entry_by_dxgi(dxgi_format);
      if (!format_desc) {
        return Result::InvalidPixelFormat;
      }

      if (header.caps2 & DDS_CAPS2_CUBEMAP_FLAG_BIT) {
        face_count *= 6;
      }
    }

    info.init(format_desc, header.width, header.height, header.depth,
              header.mip_map_count, layer_count, face_count);

    return Result::Success;
  }

  void destroy(NotNull<const Allocator *> alloc) override {}

  Result read_next_block(void *dst_memory, usize &dst_offset,
                         ImageBlockInfo &block_info) override {
    if (current_layer >= static_cast<usize>(info.array_layers)) {
      return Result::EndOfStream;
    }

    block_info.write_offset = dst_offset;
    block_info.mip_level = static_cast<u32>(current_mip++);
    block_info.array_layer = static_cast<u32>(current_layer);
    block_info.layer_count = 1;

    block_info.block_width = max(info.base_width >> block_info.mip_level, 1u);
    block_info.block_height = max(info.base_height >> block_info.mip_level, 1u);
    block_info.block_depth = max(info.base_depth >> block_info.mip_level, 1u);

    usize copy_size = info.format_desc->comp_size(block_info.block_width,
                                                  block_info.block_height,
                                                  block_info.block_depth) *
                      block_info.layer_count;
    usize bytes_readed =
        fread((u8 *)dst_memory + dst_offset, 1, copy_size, stream);
    if (bytes_readed != copy_size) {
      return Result::EndOfStream;
    }

    dst_offset += copy_size;

    if (current_mip >= static_cast<usize>(info.mip_levels)) {
      current_mip = 0;
      ++current_layer;
    }

    return Result::Success;
  }

  const ImageInfo &get_info() const override { return info; }

  ImageContainerType get_container_type() const override {
    return ImageContainerType::DDS;
  }

private:
  usize read_bytes(void *buffer, usize count) const {
    return fread(buffer, 1, count, stream);
  }
};

struct DDSWriter final : IImageWriter {
  FILE *stream = nullptr;
  ImageInfo info = {};

  DDSWriter(NotNull<FILE *> fstream) : stream{fstream.m_ptr} {}

  Result create(NotNull<const Allocator *> alloc,
                const ImageInfo &image_info) override {
    using namespace detail::dds;

    if (!image_info.format_desc ||
        image_info.format_desc->dxgi_format == DXGI_FORMAT_UNKNOWN) {
      return Result::InvalidPixelFormat;
    }

    info = image_info;

    if (write_bytes(IDENTIFIER, ident_size) != ident_size) {
      return Result::BadStream;
    }

    Header header = {
        .size = header_size,
        .flags = DDS_HEADER_CAPS_FLAG_BIT | DDS_HEADER_HEIGHT_FLAG_BIT |
                 DDS_HEADER_WIDTH_FLAG_BIT | DDS_HEADER_PIXEL_FORMAT_FLAG_BIT,
        .height = info.base_height,
        .width = info.base_width};

    const u32 base_depth = max(info.base_depth, 1u);
    if (base_depth > 1) {
      header.flags |= DDS_HEADER_DEPTH_FLAG_BIT;
      header.depth = base_depth;
    }

    if (info.mip_levels > 1) {
      header.flags |= DDS_HEADER_MIP_MAP_COUNT_FLAG_BIT;
      header.mip_map_count = info.mip_levels;
      header.caps |= DDS_CAPS_MIP_MAP_FLAG_BIT;
    }

    header.caps |= DDS_CAPS_TEXTURE_FLAG_BIT;

    const usize base_size = info.format_desc->comp_size(
        info.base_width, info.base_height, base_depth);
    if (info.format_desc->compressed) {
      header.flags |= DDS_HEADER_LINEAR_SIZE_FLAG_BIT;
      header.pitch_or_linear_size = static_cast<u32>(base_size);
    } else {
      header.flags |= DDS_HEADER_PITCH_FLAG_BIT;
      header.pitch_or_linear_size =
          static_cast<u32>(info.base_width * info.format_desc->block_size);
    }

    // TODO: Since DXT10 header is used, it is not necessary to fill it, but for
    // full support it is recommended...
    header.ddspf = {.size = sizeof(PixelFormat),
                    .flags = DDS_PIXEL_FORMAT_FOUR_CC_FLAG_BIT,
                    .fourcc = FOURCC_DX10,
                    .rgb_bit_count = 0,
                    .r_bit_mask = 0,
                    .g_bit_mask = 0,
                    .b_bit_mask = 0,
                    .a_bit_mask = 0};

    if (info.type == ImageType::ImageCube) {
      header.caps2 |= DDS_CAPS2_CUBEMAP_FLAG_BIT |
                      DDS_CAPS2_CUBEMAP_POSITIVE_X_FLAG_BIT |
                      DDS_CAPS2_CUBEMAP_NEGATIVE_X_FLAG_BIT |
                      DDS_CAPS2_CUBEMAP_POSITIVE_Y_FLAG_BIT |
                      DDS_CAPS2_CUBEMAP_NEGATIVE_Y_FLAG_BIT |
                      DDS_CAPS2_CUBEMAP_POSITIVE_Z_FLAG_BIT |
                      DDS_CAPS2_CUBEMAP_NEGATIVE_Z_FLAG_BIT;
    } else if (info.type == ImageType::Image3D) {
      header.caps2 |= DDS_CAPS2_VOLUME_FLAG_BIT;
    }

    if (write_bytes(&header, header_size) != header_size) {
      return Result::BadStream;
    }

    HeaderDXT10 header_dxt10 = {
        .dxgi_format = static_cast<DXGI_FORMAT>(info.format_desc->dxgi_format),
        .resource_dimension = DDS_RESOURCE_DIMENSION_TEXTURE_2D,
        .misc_flag = DDS_MISC_FLAG_NONE,
        .array_size = info.array_layers};

    if (info.type == ImageType::Image1D) {
      header_dxt10.resource_dimension = DDS_RESOURCE_DIMENSION_TEXTURE_1D;
    } else if (info.type == ImageType::Image3D) {
      header_dxt10.resource_dimension = DDS_RESOURCE_DIMENSION_TEXTURE_3D;
    }

    if (info.type == ImageType::ImageCube) {
      header_dxt10.misc_flag |= DDS_MISC_TEXTURE_CUBE_FLAG_BIT;
      header_dxt10.array_size = max(info.array_layers / 6u, 1u);
    }

    if (write_bytes(&header_dxt10, header_dxt10_size) != header_dxt10_size) {
      return Result::BadStream;
    }

    return Result::Success;
  }

  void destroy(NotNull<const Allocator *> alloc) override {
    if (stream) {
      fclose(stream);
    }
  }

  Result write_next_block(const void *src_memory,
                          const ImageBlockInfo &block_info) override {
    using namespace detail::dds;

    if (block_info.mip_level >= info.mip_levels) {
      return Result::EndOfStream;
    }

    const u32 block_width = block_info.block_width;
    const u32 block_height = block_info.block_height;
    const u32 block_depth = block_info.block_depth;
    const usize layer_block_size =
        info.format_desc->comp_size(block_width, block_height, block_depth);

    auto mip_comp_size = [this](u32 mip_level) {
      const u32 mip_width = max(info.base_width >> mip_level, 1u);
      const u32 mip_height = max(info.base_height >> mip_level, 1u);
      const u32 mip_depth = max(info.base_depth >> mip_level, 1u);
      return info.format_desc->comp_size(mip_width, mip_height, mip_depth);
    };

    usize per_layer_size = 0;
    for (u32 mip = 0; mip < info.mip_levels; ++mip) {
      per_layer_size += mip_comp_size(mip);
    }

    usize mip_offset = 0;
    for (u32 mip = 0; mip < block_info.mip_level; ++mip) {
      mip_offset += mip_comp_size(mip);
    }

    const usize data_start_offset =
        ident_size + header_size + header_dxt10_size;

    const u8 *src_bytes = static_cast<const u8 *>(src_memory);
    for (u32 layer_index = 0; layer_index < block_info.layer_count;
         ++layer_index) {
      const u32 layer = block_info.array_layer + layer_index;
      const usize dst_offset = data_start_offset +
                               static_cast<usize>(layer) * per_layer_size +
                               mip_offset;
      if (fseek(stream, static_cast<long>(dst_offset), SEEK_SET) != 0) {
        return Result::BadStream;
      }

      const usize src_block_offset =
          block_info.write_offset +
          static_cast<usize>(layer_index) * layer_block_size;
      if (write_bytes(src_bytes + src_block_offset, layer_block_size) !=
          layer_block_size) {
        return Result::BadStream;
      }
    }

    return Result::Success;
  }

  const ImageInfo &get_info() const override { return info; }

  ImageContainerType get_container_type() const override {
    return ImageContainerType::DDS;
  }

private:
  usize write_bytes(const void *buffer, usize count) const {
    return fwrite(buffer, 1, count, stream);
  }
};
} // namespace edge

#undef MAKE_FOURCC

#endif