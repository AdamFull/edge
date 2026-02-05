#ifndef EDGE_KTX_IMAGE_H
#define EDGE_KTX_IMAGE_H

#include "image.hpp"
#include "image_format.hpp"

namespace edge {
namespace detail::ktx1 {
[[maybe_unused]] constexpr u32 KTX_ENDIAN_REF = 0x04030201;
constexpr u32 KTX_ENDIAN_REF_REV = 0x01020304;

constexpr u8 IDENTIFIER[] = {0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31,
                             0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A};

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

static u32 swap_u32(const u32 val) {
  return ((val & 0xFF000000) >> 24) | ((val & 0x00FF0000) >> 8) |
         ((val & 0x0000FF00) << 8) | ((val & 0x000000FF) << 24);
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
} // namespace detail::ktx1

struct KTX10Reader final : IImageReader {
  FILE *stream = nullptr;
  ImageInfo info = {};
  u32 endianness = 0;

  usize current_mip = 0;

  KTX10Reader(NotNull<FILE *> fstream) : stream{fstream.m_ptr} {}

  Result create(NotNull<const Allocator *> alloc) override {
    using namespace detail::ktx1;

    Header header{};
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
      header.number_of_array_elements =
          swap_u32(header.number_of_array_elements);
      header.number_of_faces = swap_u32(header.number_of_faces);
      header.number_of_mipmap_levels = swap_u32(header.number_of_mipmap_levels);
      header.bytes_of_key_value_data = swap_u32(header.bytes_of_key_value_data);
    }

    const ImageFormatDesc *format_desc =
        detail::find_format_entry_by_gl(header.gl_internal_format);
    if (!format_desc) {
      return Result::InvalidPixelFormat;
    }

    info.init(format_desc, header.pixel_width, header.pixel_height,
              header.pixel_depth, header.number_of_mipmap_levels,
              header.number_of_array_elements, header.number_of_faces);

    // Skip metadata
    fseek(stream, static_cast<long>(header.bytes_of_key_value_data), SEEK_CUR);

    return Result::Success;
  }

  void destroy(NotNull<const Allocator *> alloc) override {}

  Result read_next_block(void *dst_memory, usize &dst_offset,
                         ImageBlockInfo &block_info) override {
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
      // EDGE_LOG_ERROR();
      return Result::EndOfStream;
    }

    if (endianness == KTX_ENDIAN_REF_REV) {
      next_block_size = swap_u32(next_block_size);
    }

    usize calculated_block_size =
        info.format_desc->comp_size(block_info.block_width,
                                    block_info.block_height,
                                    block_info.block_depth) *
        block_info.layer_count;
    if (calculated_block_size != static_cast<usize>(next_block_size)) {
      // Invalid image data
      return Result::EndOfStream;
    }

    const usize bytes_readed =
        read_bytes(
        static_cast<u8 *>(dst_memory) + dst_offset, calculated_block_size);
    if (bytes_readed != calculated_block_size) {
      return Result::EndOfStream;
    }

    dst_offset += calculated_block_size;

    const usize current_stream_offset = ftell(stream);
    fseek(stream, (current_stream_offset + 3) & ~3, SEEK_SET);

    return Result::Success;
  }

  [[nodiscard]] const ImageInfo &get_info() const override { return info; }

  [[nodiscard]] ImageContainerType get_container_type() const override {
    return ImageContainerType::KTX_1_0;
  }

private:
  usize read_bytes(void *buffer, const usize count) const {
    return fread(buffer, 1, count, stream);
  }
};

struct KTX10Writer final : IImageWriter {
  FILE *stream = nullptr;
  ImageInfo info = {};

  KTX10Writer(const NotNull<FILE *> fstream) : stream{fstream.m_ptr} {}

  Result create(NotNull<const Allocator *> alloc,
                const ImageInfo &image_info) override {
    using namespace detail::ktx1;

    if (!image_info.format_desc ||
        image_info.format_desc->gl_internal_format == 0) {
      return Result::InvalidPixelFormat;
    }

    info = image_info;

    if (write_bytes(IDENTIFIER, ident_size) != ident_size) {
      return Result::BadStream;
    }

    Header header = {.endianness = KTX_ENDIAN_REF,
                     .gl_type = info.format_desc->gl_type,
                     .gl_format = info.format_desc->gl_format,
                     .gl_internal_format = info.format_desc->gl_internal_format,
                     .gl_base_internal_format =
                         info.format_desc->gl_format != 0
                             ? info.format_desc->gl_format
                             : info.format_desc->gl_internal_format,
                     .pixel_width = info.base_width,
                     .pixel_height = info.base_height,
                     .pixel_depth = info.base_depth,
                     .number_of_array_elements = info.array_layers,
                     .number_of_faces = 1,
                     .number_of_mipmap_levels = info.mip_levels,
                     .bytes_of_key_value_data = 0};

    if (info.type == ImageType::ImageCube) {
      header.number_of_faces = 6;
      header.number_of_array_elements = max(info.array_layers / 6u, 1u);
    }

    if (info.format_desc->compressed) {
      header.gl_type = 0;
      header.gl_format = 0;
      header.gl_type_size = 1;
    } else {
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

  void destroy(NotNull<const Allocator *> alloc) override {}

  Result write_next_block(const void *src_memory,
                          const ImageBlockInfo &block_info) override {
    using namespace detail::ktx1;

    if (block_info.mip_level >= info.mip_levels) {
      return Result::EndOfStream;
    }

    const u32 block_width = block_info.block_width;
    const u32 block_height = block_info.block_height;
    const u32 block_depth = block_info.block_depth;
    const usize layer_block_size =
        info.format_desc->comp_size(block_width, block_height, block_depth);

    if (block_info.array_layer + block_info.layer_count > info.array_layers) {
      return Result::EndOfStream;
    }

    auto mip_comp_size = [this](const u32 mip_level) {
      const u32 mip_width = max(info.base_width >> mip_level, 1u);
      const u32 mip_height = max(info.base_height >> mip_level, 1u);
      const u32 mip_depth = max(info.base_depth >> mip_level, 1u);
      return info.format_desc->comp_size(mip_width, mip_height, mip_depth);
    };

    usize mip_block_offset = ident_size + header_size;
    for (u32 mip = 0; mip < block_info.mip_level; ++mip) {
      const usize mip_size = mip_comp_size(mip) * info.array_layers;
      const usize mip_block_size = 4 + mip_size;
      mip_block_offset += (mip_block_size + 3) & ~static_cast<usize>(3);
    }

    const usize mip_block_size =
        mip_comp_size(block_info.mip_level) * info.array_layers;
    const u32 image_size = static_cast<u32>(mip_block_size);

    if (fseek(stream, static_cast<long>(mip_block_offset), SEEK_SET) != 0) {
      return Result::BadStream;
    }

    if (write_bytes(&image_size, 4) != 4) {
      return Result::BadStream;
    }

    const usize mip_data_offset = mip_block_offset + 4;
    const u8 *src_bytes = static_cast<const u8 *>(src_memory);
    for (u32 layer_index = 0; layer_index < block_info.layer_count;
         ++layer_index) {
      const u32 layer = block_info.array_layer + layer_index;
      const usize dst_offset =
          mip_data_offset + static_cast<usize>(layer) * layer_block_size;
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

  [[nodiscard]] const ImageInfo &get_info() const override { return info; }

  [[nodiscard]] ImageContainerType get_container_type() const override {
    return ImageContainerType::KTX_1_0;
  }

private:
  usize write_bytes(const void *buffer, const usize count) const {
    return fwrite(buffer, 1, count, stream);
  }
};
} // namespace edge

namespace edge::detail::ktx2 {
// TODO: NOT IMPLEMENTED
constexpr u8 IDENTIFIER[] = {0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32,
                             0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A};

static constexpr usize ident_size = sizeof(IDENTIFIER);
} // namespace edge::detail::ktx2

#endif