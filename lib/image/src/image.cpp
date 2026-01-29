#include "dds_image.hpp"
#include "ktx_image.hpp"

#include <math.hpp>

namespace edge {
	namespace detail {
		constexpr usize max_ident_size = max(ktx1::ident_size, dds::ident_size);
	}

	void ImageInfo::init(const ImageFormatDesc* desc, u32 width, u32 height, u32 depth, u32 mip_count, u32 layer_count, u32 face_count) {
		format_desc = desc;

		base_width = width;
		base_height = max(height, 1u);
		base_depth = max(depth, 1u);
		mip_levels = max(mip_count, 1u);
		array_layers = max(layer_count, 1u) * face_count;

		if (face_count == 6) {
			type = ImageType::ImageCube;
		}
		else if (base_depth > 1) {
			type = ImageType::Image3D;
		}
		else if (base_height > 1) {
			type = ImageType::Image2D;
		}
		else if (base_width >= 1) {
			type = ImageType::Image2D;
		}

		for (usize i = 0; i < static_cast<usize>(mip_levels); ++i) {
			u32 mip_width = max(base_width >> static_cast<u32>(i), 1u);
			u32 mip_height = max(base_height >> static_cast<u32>(i), 1u);
			u32 mip_depth = max(base_depth >> static_cast<u32>(i), 1u);

			whole_size += format_desc->comp_size(mip_width, mip_height, mip_depth) * array_layers;
		}
	}

	Result<IImageReader*, IImageReader::Result> open_image_reader(NotNull<const Allocator*> alloc, NotNull<FILE*> stream) {
		u8 buffer[detail::max_ident_size];

		if (fread(buffer, 1, detail::max_ident_size, stream.m_ptr) != detail::max_ident_size) {
			return IImageReader::Result::InvalidHeader;
		}

		IImageReader* reader = nullptr;
		if (memcmp(buffer, detail::dds::IDENTIFIER, detail::dds::ident_size) == 0) {
			fseek(stream.m_ptr, detail::dds::ident_size, SEEK_SET);
			reader = alloc->allocate<DDSReader>(stream);
		}
		else if (memcmp(buffer, detail::ktx1::IDENTIFIER, detail::ktx1::ident_size) == 0) {
			fseek(stream.m_ptr, detail::ktx1::ident_size, SEEK_SET);
			reader = alloc->allocate<KTX10Reader>(stream);
		}
		else {
			return IImageReader::Result::InvalidHeader;
		}

		return reader;
	}

	Result<IImageWriter*, IImageWriter::Result> open_image_writer(NotNull<const Allocator*> alloc, NotNull<FILE*> stream, ImageContainerType type) {
		IImageWriter* writer = nullptr;

		switch (type) {
		case ImageContainerType::DDS:
			writer = alloc->allocate<DDSWriter>(stream);
			break;
		case ImageContainerType::KTX_1_0:
			writer = alloc->allocate<KTX10Writer>(stream);
			break;
		default:
			return IImageWriter::Result::InvalidHeader;
		}

		return writer;
	}
}