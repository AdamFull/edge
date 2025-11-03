#pragma once

#include <streambuf>
#include <istream>
#include <ostream>

#include "../platform/platform.h"

namespace edge::fs {
	class FileStreamBuf final : public NonCopyable, public std::streambuf {
	public:
		FileStreamBuf(size_t input_buffer_size = 1024, size_t output_buffer_size = 1024) 
			: input_buffer_(input_buffer_size)
			, output_buffer_(output_buffer_size) {

			auto* out_begin = output_buffer_.data();
			setp(out_begin, out_begin + output_buffer_.size());
		}

		explicit FileStreamBuf(Shared<platform::IPlatformFile>&& file, size_t input_buffer_size = 1024, size_t output_buffer_size = 1024)
			: file_(std::move(file))
			, input_buffer_(input_buffer_size)
			, output_buffer_(output_buffer_size) {

			auto* out_begin = output_buffer_.data();
			setp(out_begin, out_begin + output_buffer_.size());
		}

		explicit FileStreamBuf(std::u8string_view path, std::ios_base::openmode mode, size_t input_buffer_size = 1024, size_t output_buffer_size = 1024);

		~FileStreamBuf() {
			sync();
		}

		FileStreamBuf(FileStreamBuf&& other) noexcept
			: std::streambuf(std::move(other))
			, file_(std::move(other.file_))
			, input_buffer_(std::move(other.input_buffer_))
			, output_buffer_(std::move(other.output_buffer_)) {

		}

		FileStreamBuf& operator=(FileStreamBuf&& other) noexcept {
			if (this != &other) {
				std::streambuf::operator=(std::move(other));
				file_ = std::move(other.file_);
				input_buffer_ = std::move(other.input_buffer_);
				output_buffer_ = std::move(other.output_buffer_);
			}
			return *this;
		}

		auto open(std::u8string_view path, std::ios_base::openmode mode) -> bool;

		auto is_open() const noexcept -> bool;
		auto close() noexcept -> void;
	protected:
		int_type underflow() override;
		int_type overflow(int_type ch) override;
		int sync() override;
		pos_type seekoff(off_type offset, std::ios_base::seekdir dir, std::ios_base::openmode which) override;
		pos_type seekpos(pos_type pos, std::ios_base::openmode which) override;
		std::streamsize showmanyc() override;
	private:
		Shared<platform::IPlatformFile> file_;
		mi::Vector<char> input_buffer_;
		mi::Vector<char> output_buffer_;
	};

	template<typename T, std::ios_base::openmode BaseOpenMode, size_t BufIn = 1024, size_t BufOut = 1024>
	class FileStreamBase final : public NonCopyable, public T {
	public:
		FileStreamBase()
			: T(nullptr)
			, file_buf_{ BufIn, BufOut }{
			T::rdbuf(&file_buf_);
		}

		explicit FileStreamBase(Shared<platform::IPlatformFile>&& file)
			: T(nullptr)
			, file_buf_(std::move(file), BufIn, BufOut) {
			T::rdbuf(&file_buf_);
		}

		explicit FileStreamBase(std::u8string_view path, std::ios_base::openmode mode)
			: T(nullptr)
			, file_buf_{ path, mode | BaseOpenMode, BufIn, BufOut }{
			T::rdbuf(&file_buf_);
		}

		explicit FileStreamBase(std::string_view path, std::ios_base::openmode mode)
			: T(nullptr)
			, file_buf_{ unicode::make_utf8_string(path), mode | BaseOpenMode, BufIn, BufOut }{
			T::rdbuf(&file_buf_);
		}

		explicit FileStreamBase(std::u16string_view path, std::ios_base::openmode mode)
			: T(nullptr)
			, file_buf_{ unicode::make_utf8_string(path), mode | BaseOpenMode, BufIn, BufOut }{
			T::rdbuf(&file_buf_);
		}

		explicit FileStreamBase(std::u32string_view path, std::ios_base::openmode mode)
			: T(nullptr)
			, file_buf_{ unicode::make_utf8_string(path), mode | BaseOpenMode, BufIn, BufOut }{
			T::rdbuf(&file_buf_);
		}

		~FileStreamBase() = default;

		FileStreamBase(FileStreamBase&& other) noexcept
			: T(std::move(other))
			, file_buf_(std::move(other.file_buf_)) {
			T::rdbuf(&file_buf_);
		}

		FileStreamBase& operator=(FileStreamBase&& other) noexcept {
			if (this != &other) {
				T::operator=(std::move(other));
				file_buf_ = std::move(other.file_buf_);
				T::rdbuf(&file_buf_);
			}
			return *this;
		}

		auto open(std::u8string_view path, std::ios_base::openmode mode) -> bool {
			return file_buf_.open(path, mode | BaseOpenMode);
		}

		auto open(std::string_view path, std::ios_base::openmode mode) -> bool {
			return file_buf_.open(unicode::make_utf8_string(path), mode | BaseOpenMode);
		}

		auto open(std::wstring_view path, std::ios_base::openmode mode) -> bool {
			return file_buf_.open(unicode::make_utf8_string(path), mode | BaseOpenMode);
		}

		auto open(std::u16string_view path, std::ios_base::openmode mode) -> bool {
			return file_buf_.open(unicode::make_utf8_string(path), mode | BaseOpenMode);
		}

		auto open(std::u32string_view path, std::ios_base::openmode mode) -> bool {
			return file_buf_.open(unicode::make_utf8_string(path), mode | BaseOpenMode);
		}

		[[nodiscard]] auto is_open() const -> bool {
			return file_buf_.is_open();
		}

		auto close() -> void {
			if (is_open()) {
				file_buf_.pubsync();
				file_buf_.close();
			}
		}
	private:
		FileStreamBuf file_buf_;
	};

	template<size_t BufferSize = 1024>
	using InputFileStream = FileStreamBase<std::istream, std::ios_base::in, BufferSize, 0ull>;
	template<size_t BufferSize = 1024>
	using OutputFileStream = FileStreamBase<std::ostream, std::ios_base::out, 0ull, BufferSize>;
	template<size_t InBufferSize = 1024, size_t OutBufferSize = 1024>
	using FileStream = FileStreamBase<std::iostream, std::ios_base::in | std::ios_base::out, InBufferSize, OutBufferSize>;

	class DirectoryIterator {
	public:
		DirectoryIterator(Shared<platform::IPlatformDirectoryIterator>&& iterator_impl) noexcept :
			impl_{ std::move(iterator_impl) } {
		}

		DirectoryIterator(const DirectoryIterator& other) :
			impl_{ other.impl_ } {
		}

		[[nodiscard]] auto begin() -> DirectoryIterator& {
			return *this;
		}

		[[nodiscard]] auto end() -> DirectoryIterator& {
			static DirectoryIterator end_{ nullptr };
			return end_;
		}

		auto operator++() -> DirectoryIterator& {
			impl_->next();
			return *this;
		}

		auto operator*() const noexcept -> const platform::DirEntry& {
			return impl_->value();
		}

		auto operator!=(const DirectoryIterator& other) -> bool const {
			//if (!other.impl_)
			//	return !impl_->End();
			//
			//return impl_ != other.impl_;
			return impl_ && !impl_->end();
		}
	private:
		Shared<platform::IPlatformDirectoryIterator> impl_;
	};

	struct MountPoint {
		mi::U8String path;
		Shared<platform::IPlatformFilesystem> filesystem;
	};

	auto initialize_filesystem() -> void;
	auto shutdown_filesystem() -> void;

	auto create_native_filesystem(std::u8string_view root_path) -> Shared<platform::IPlatformFilesystem>;

	auto mount_filesystem(std::u8string_view mount_point, Shared<platform::IPlatformFilesystem>&& filesystem) -> void;
	auto unmount_filesystem(std::u8string_view mount_point) -> bool;

	auto exists(std::u8string_view path) -> bool;
	auto is_directory(std::u8string_view path) -> bool;
	auto is_file(std::u8string_view path) -> bool;

	auto create_directory(std::u8string_view path) -> bool;
	auto create_directories(std::u8string_view path) -> bool;
	auto remove(std::u8string_view path) -> bool;

	auto get_work_directory_path() -> std::u8string_view;
	auto get_temp_directory_path() -> std::u8string_view;
	auto get_cache_directory_path() -> std::u8string_view;

	[[nodiscard]] auto walk_directory(std::u8string_view path, bool recursive = false) -> DirectoryIterator;

	// Helper functions
	auto read_whole_file(std::u8string_view path, std::ios_base::openmode mode, mi::Vector<uint8_t>& out) -> bool;
}