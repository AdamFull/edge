#pragma once

#include <streambuf>
#include <istream>
#include <ostream>

#include "../foundation/foundation.h"

namespace edge::fs {
	class IFile {
	public:
		virtual ~IFile() = default;

		[[nodiscard]] virtual auto is_open() -> bool = 0;
		virtual auto close() -> void = 0;

		[[nodiscard]] virtual auto size() -> uint64_t = 0;
		virtual auto seek(uint64_t offset, std::ios_base::seekdir origin) -> int64_t = 0;
		[[nodiscard]] virtual auto tell() -> int64_t = 0;
		virtual auto read(void* buffer, uint64_t size) -> int64_t = 0;
		virtual auto write(const void* buffer, uint64_t size) -> int64_t = 0;
	};

	class FileStreamBuf final : public NonCopyable, public std::streambuf {
	public:
		FileStreamBuf(size_t input_buffer_size = 1024, size_t output_buffer_size = 1024) 
			: input_buffer_(input_buffer_size)
			, output_buffer_(output_buffer_size) {

			auto* out_begin = output_buffer_.data();
			setp(out_begin, out_begin + output_buffer_.size());
		}

		explicit FileStreamBuf(Shared<IFile>&& file, size_t input_buffer_size = 1024, size_t output_buffer_size = 1024)
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
		Shared<IFile> file_;
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

		explicit FileStreamBase(Shared<IFile>&& file)
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

	struct DirEntry {
		mi::U8String path;
		bool is_directory;
		uint64_t size;
	};

	class IDirectoryIterator {
	public:
		virtual ~IDirectoryIterator() = default;

		virtual auto end() const noexcept -> bool = 0;
		virtual auto next() -> void = 0;
		virtual auto value() const noexcept -> const DirEntry& = 0;
	};

	class IFilesystem {
	public:
		virtual ~IFilesystem() = default;

		[[nodiscard]] virtual auto open_file(std::u8string_view path, std::ios_base::openmode mode) -> Shared<IFile> = 0;
		virtual auto create_directory(std::u8string_view path) -> bool = 0;
		virtual auto remove(std::u8string_view path) -> bool = 0;

		virtual auto exists(std::u8string_view path) -> bool = 0;
		virtual auto is_directory(std::u8string_view path) -> bool = 0;
		virtual auto is_file(std::u8string_view path) -> bool = 0;

		[[nodiscard]] virtual auto walk(std::u8string_view path, bool recursive = false) -> Shared<IDirectoryIterator> = 0;
	};

	class DirectoryIterator {
	public:
		DirectoryIterator(Shared<IDirectoryIterator>&& iterator_impl) noexcept :
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

		auto operator*() const noexcept -> const DirEntry& {
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
		Shared<IDirectoryIterator> impl_;
	};

	struct MountPoint {
		mi::U8String path;
		Shared<IFilesystem> filesystem;
	};

	auto initialize_filesystem() -> void;
	auto shutdown_filesystem() -> void;

	auto create_native_filesystem(std::u8string_view root_path) -> Shared<IFilesystem>;

	auto mount_filesystem(std::u8string_view mount_point, Shared<IFilesystem>&& filesystem) -> void;
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
}