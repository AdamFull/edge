#ifndef EDGE_IMAGE_H
#define EDGE_IMAGE_H

#include <cstdio>

#include <callable.hpp>

namespace edge {
using ImageReadFn = Callable<bool(u32 mip_level, u32 layer, u32 layer_count)>;

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

enum class ImageContainerType { None, KTX_1_0, DDS, Internal };

enum class ImageType { None, Image1D, Image2D, Image3D, ImageCube };

struct ImageInfo {
  usize whole_size = 0;
  const ImageFormatDesc *format_desc = nullptr;

  u32 base_width = 1;
  u32 base_height = 1;
  u32 base_depth = 1;
  u32 mip_levels = 1;
  u32 array_layers = 1;
  ImageType type = ImageType::None;

  void init(const ImageFormatDesc *desc, u32 width, u32 height, u32 depth,
            u32 mip_count, u32 layer_count, u32 face_count);
};

struct ImageBlockInfo {
  usize write_offset = 0;
  u32 mip_level = 0;
  u32 mip_count = 0;
  u32 array_layer = 0;
  u32 layer_count = 0;
  u32 block_width = 1;
  u32 block_height = 1;
  u32 block_depth = 1;
};

struct IImageReader {
  enum class Result {
    Success = 0,
    IOError,
    InvalidHeader,
    OutOfMemory,
    InvalidPixelFormat,
    EndOfStream
  };

  virtual ~IImageReader() = default;

  virtual Result create(NotNull<const Allocator *> alloc) = 0;
  virtual void destroy(NotNull<const Allocator *> alloc) = 0;

  virtual Result read_next_block(void *dst_memory, usize &dst_offset,
                                 ImageBlockInfo &block_info) = 0;

  virtual const ImageInfo &get_info() const = 0;
  virtual ImageContainerType get_container_type() const = 0;
};

struct IImageWriter {
  enum class Result {
    Success = 0,
    IOError,
    InvalidHeader,
    OutOfMemory,
    InvalidPixelFormat,
    UnsupportedFormat,
    BadStream,
    EndOfStream
  };

  virtual ~IImageWriter() = default;

  virtual Result create(NotNull<const Allocator *> alloc,
                        const ImageInfo &info) = 0;
  virtual void destroy(NotNull<const Allocator *> alloc) = 0;

  virtual Result write_next_block(const void *src_memory,
                                  const ImageBlockInfo &block_info) = 0;

  virtual const ImageInfo &get_info() const = 0;
  virtual ImageContainerType get_container_type() const = 0;
};

Result<IImageReader *, IImageReader::Result>
open_image_reader(NotNull<const Allocator *> alloc, NotNull<FILE *> stream);
Result<IImageWriter *, IImageWriter::Result>
open_image_writer(NotNull<const Allocator *> alloc, NotNull<FILE *> stream,
                  ImageContainerType type);
} // namespace edge

#endif