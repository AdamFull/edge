#include "dds_image.hpp"
#include "ktx_image.hpp"
#include "internal_image.hpp"

#include <math.hpp>

namespace edge {
	namespace detail {
		constexpr usize max_ident_size = max(ktx1::ident_size, max(dds::ident_size, internal::ident_size));
	}

	void ImageInfo::init(const ImageFormatDesc* desc, u32 width, u32 height, u32 depth, u32 mip_count, u32 layer_count, u32 face_count) {
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

			whole_size += desc->comp_size(mip_width, mip_height, mip_depth) * array_layers;
		}
	}

	IImageReader* open_image_reader(NotNull<const Allocator*> alloc, const char* path) {
		FILE* stream = fopen(path, "rb");
		if (!stream) {
			// TODO: Error file not found
			return nullptr;
		}

		return open_image_reader(alloc, stream);
	}

	IImageReader* open_image_reader(NotNull<const Allocator*> alloc, FILE* stream) {
		u8 buffer[detail::max_ident_size];

		if (fread(buffer, 1, detail::max_ident_size, stream) != detail::max_ident_size) {
			// TODO: Result::InvalidHeader
			return nullptr;
		}

		if (memcmp(buffer, detail::dds::IDENTIFIER, detail::dds::ident_size) == 0) {
			fseek(stream, detail::dds::ident_size, SEEK_SET);
			DDSReader* reader = alloc->allocate<DDSReader>(stream);
			auto result = reader->read_header();
			if (result != IImageReader::Result::Success) {
				alloc->deallocate(reader);
				reader = nullptr;
			}
			return reader;
		}
		else if (memcmp(buffer, detail::ktx1::IDENTIFIER, detail::ktx1::ident_size) == 0) {
			fseek(stream, detail::ktx1::ident_size, SEEK_SET);

			KTX10Reader* reader = alloc->allocate<KTX10Reader>(stream);

			auto result = reader->read_header();
			if (result != IImageReader::Result::Success) {
				alloc->deallocate(reader);
				reader = nullptr;
			}
			return reader;
		}
		else if (memcmp(buffer, detail::internal::IDENTIFIER, detail::internal::ident_size) == 0) {
			fseek(stream, detail::internal::ident_size, SEEK_SET);

			InternalReader* reader = alloc->allocate<InternalReader>(alloc, stream);

			auto result = reader->read_header();
			if (result != IImageReader::Result::Success) {
				alloc->deallocate(reader);
				reader = nullptr;
			}
			return reader;
		}

		return nullptr; //Result::UnsupportedFileFormat;
	}
}