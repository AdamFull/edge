#ifndef EDGE_INTERNAL_IMAGE_H
#define EDGE_INTERNAL_IMAGE_H

#define LZ4F_STATIC_LINKING_ONLY
#include <lz4frame.h>

namespace edge {
	namespace detail::internal {
		constexpr u8 IDENTIFIER[] = {
				'E', 'D', 'G', 'E', ' ',
				'I', 'N', 'T', 'E', 'R', 'N', 'A', 'L', ' ',
				'I', 'M', 'A', 'G', 'E',
				0x00, 0x00, 0x01, 0x00, 0x00
		};

		static constexpr usize ident_size = sizeof(IDENTIFIER);

		enum class ImageCompressionMethod : u32 {
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
			ImageCompressionMethod compression;
		};

		static constexpr usize header_size = sizeof(Header);
		static constexpr usize chunk_size = 64 * 1024;

#if 0
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
#endif
	}

	struct IImageDecompressor {
		virtual ~IImageDecompressor() = default;

		virtual bool initialize(NotNull<const Allocator*> alloc) = 0;
		virtual void cleanup(NotNull<const Allocator*> alloc) = 0;

		virtual usize decompress(FILE* stream, void* dst_buffer, usize compressed_size, usize original_size) = 0;
	};

	struct LZ4Decompressor final : IImageDecompressor {
		LZ4F_CustomMem custom_mem = {};
		LZ4F_dctx* lz4ktx = nullptr;
		char* io_buffer = nullptr;

		bool initialize(NotNull<const Allocator*> alloc) override {
			using namespace detail::internal;

			custom_mem = {
				.customAlloc = [](void* ud, usize size) -> void* { return static_cast<const Allocator*>(ud)->malloc(size, 1); },
				.customCalloc = nullptr,
				.customFree = [](void* ud, void* address) -> void { static_cast<const Allocator*>(ud)->free(address); },
				.opaqueState = (void*)alloc.m_ptr
			};

			lz4ktx = LZ4F_createDecompressionContext_advanced(custom_mem, LZ4F_VERSION);
			if (!lz4ktx) {
				return false;
			}

			io_buffer = (char*)alloc->malloc(chunk_size * 2, 1);
			if (!io_buffer) {
				cleanup(alloc);
				return false;
			}

			return true;
		}

		void cleanup(NotNull<const Allocator*> alloc) override {
			if (io_buffer) {
				alloc->free(io_buffer);
				io_buffer = nullptr;
			}

			if (lz4ktx) {
				LZ4F_freeDecompressionContext(lz4ktx);
				lz4ktx = nullptr;
			}
		}

		usize decompress(FILE* stream, void* dst_buffer, usize compressed_size, usize original_size) override {
			using namespace detail::internal;

			usize total_decompressed = 0;
			usize compressed_remaining = compressed_size;
			usize ret = 1;

			while (ret != 0 && total_decompressed < original_size && compressed_remaining > 0) {
				usize to_read = (compressed_remaining < chunk_size) ? compressed_remaining : chunk_size;
				usize read_size = fread(io_buffer, 1, to_read, stream);

				if (read_size == 0 || read_size != to_read) {
					return 0;
				}

				compressed_remaining -= read_size;

				const void* src_ptr = io_buffer;
				const void* src_end = (const char*)io_buffer + read_size;

				while (src_ptr < src_end && ret != 0 && total_decompressed < original_size) {
					usize dst_size = chunk_size;
					usize src_size = (const char*)src_end - (const char*)src_ptr;

					ret = LZ4F_decompress(lz4ktx, io_buffer + chunk_size, &dst_size, src_ptr, &src_size, NULL);

					if (LZ4F_isError(ret)) {
						return 0;
					}

					if (dst_size > 0) {
						if (total_decompressed + dst_size > original_size) {
							return 0;
						}

						memcpy((u8*)dst_buffer + total_decompressed, io_buffer + chunk_size, dst_size);
						total_decompressed += dst_size;
					}

					src_ptr = (const char*)src_ptr + src_size;
				}
			}

			return total_decompressed;
		}
	};

	struct InternalReader final : IImageReader {
		FILE* stream = nullptr;
		ImageInfo info = {};
		IImageDecompressor* decompressor = nullptr;

		usize current_mip = 0;

		InternalReader(NotNull<FILE*> fstream)
			: stream{ fstream.m_ptr } {
		}

		Result create(NotNull<const Allocator*> alloc) override {
			using namespace detail::internal;

			Header header;
			if (read_bytes(&header, header_size) != header_size) {
				return Result::InvalidHeader;
			}

			const ImageFormatDesc* format_desc = detail::find_format_entry_by_vk(header.vk_format);
			if (!format_desc) {
				return Result::InvalidPixelFormat;
			}

			info.init(format_desc,
				header.pixel_width, header.pixel_height, header.pixel_depth,
				header.number_of_mipmap_levels, header.number_of_array_elements,
				header.number_of_faces);

			// TODO: Detect decompressor type

			decompressor = alloc->allocate<LZ4Decompressor>();
			if (!decompressor || !decompressor->initialize(alloc)) {
				return Result::OutOfMemory;
			}

			return Result::Success;
		}

		void destroy(NotNull<const Allocator*> alloc) override {
			if (decompressor) {
				decompressor->cleanup(alloc);
				alloc->deallocate(decompressor);
			}

			if (stream) {
				fclose(stream);
			}
		}

		Result read_next_block(void* dst_memory, usize& dst_offset, ImageBlockInfo& block_info) override {
			using namespace detail::internal;

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
				return Result::EndOfStream;
			}

			usize calculated_block_size = info.format_desc->comp_size(block_info.block_width, block_info.block_height, block_info.block_depth) * block_info.layer_count;
			if (decompressor->decompress(stream, (u8*)dst_memory + dst_offset, next_block_size, calculated_block_size) != calculated_block_size) {
				// ERROR: Decompression failed
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
			return ImageContainerType::Internal;
		}

	private:
		usize read_bytes(void* buffer, usize count) const {
			return fread(buffer, 1, count, stream);
		}
	};

#if 0
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
			.compression = ImageCompressionMethod::LZ4
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

		LZ4F_freeCompressionContext(lz4cctx);

		return Result::Success;
	}
#endif
}

#endif