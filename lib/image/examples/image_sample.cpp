#include <image.hpp>

static edge::Allocator allocator = {};

using namespace edge;

int main() {
	allocator = Allocator::create_tracking();

	FILE* read_image_stream = fopen("D:\\GitHub\\edge\\assets\\images\\texture_cube_with_mips.dds", "rb");
	auto image_reader_result = open_image_reader(&allocator, read_image_stream);
	if (!image_reader_result) {
		return -1;
	}

	IImageReader* image_reader = image_reader_result.value();
	image_reader->create(&allocator);
	const ImageInfo& image_info = image_reader->get_info();

	FILE* write_image_stream = fopen("D:\\GitHub\\edge\\assets\\images\\texture_cube_with_mips.ktx", "wb");
	auto image_writer_result = open_image_writer(&allocator, write_image_stream, ImageContainerType::KTX_1_0);
	if (!image_writer_result) {
		return -1;
	}

	IImageWriter* image_writer = image_writer_result.value();
	image_writer->create(&allocator, image_info);
	
	u8* temp_buffer = (u8*)allocator.malloc(image_info.whole_size, 1);

	usize dst_offset = 0;
	ImageBlockInfo block_info = {};
	while (image_reader->read_next_block(temp_buffer, dst_offset, block_info) != IImageReader::Result::EndOfStream) {
		image_writer->write_next_block(temp_buffer, block_info);
	}

	allocator.free(temp_buffer);

	image_writer->destroy(&allocator);
	allocator.free(image_writer);
	fclose(write_image_stream);

	image_reader->destroy(&allocator);
	allocator.free(image_reader);
	fclose(read_image_stream);

	usize net_allocated = allocator.get_net();
	assert(net_allocated == 0 && "Memory leaks detected.");

	return 0;
}