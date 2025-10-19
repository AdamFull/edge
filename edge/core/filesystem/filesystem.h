#pragma once

#include <algorithm>
#include <streambuf>
#include <istream>
#include <ostream>

#include "../foundation/foundation.h"

namespace edge::fs {
	using Path = mi::WString;
	using PathView = std::wstring_view;

	inline auto normalize_path(PathView path) -> Path {
		Path result = Path{ path };
		std::replace(result.begin(), result.end(), L'\\', L'/');
		if (!result.empty() && result.back() == L'/') {
			result.pop_back();
		}
		if (result.empty()) {
			result = L"/";
		}
		return result;
	}

	inline auto split_path(PathView path) -> mi::Vector<Path> {
		mi::Vector<Path> parts;
		Path current;

		for (wchar_t ch : path) {
			if (ch == L'/') {
				if (!current.empty()) {
					parts.push_back(current);
					current.clear();
				}
			}
			else {
				current += ch;
			}
		}

		if (!current.empty()) {
			parts.push_back(current);
		}

		return parts;
	}

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

	class FileInputBuf : public std::streambuf {
	public:
		explicit FileInputBuf(Shared<IFile>&& file) 
			: file_(std::move(file)) {
		}

	protected:
		int_type underflow() override;
		pos_type seekoff(off_type off, std::ios_base::seekdir origin, std::ios_base::openmode which = std::ios_base::in) override;
		pos_type seekpos(pos_type pos, std::ios_base::openmode which = std::ios_base::in) override;

	private:
		Shared<IFile> file_;
		char buffer_;
	};

	class FileInputStream : public std::istream {
	public:
		explicit FileInputStream(Shared<IFile>&& file)
			: std::istream(&buf_), buf_(std::move(file)) {}

	private:
		FileInputBuf buf_;
	};

	class FileOutputBuf : public std::streambuf {
	public:
		explicit FileOutputBuf(Shared<IFile>&& file) 
			: file_(std::move(file)) {
		}

	protected:
		int_type overflow(int_type c) override;
		std::streamsize xsputn(const char* s, std::streamsize n) override;
		pos_type seekoff(off_type off, std::ios_base::seekdir origin, std::ios_base::openmode which = std::ios_base::out) override;
		pos_type seekpos(pos_type pos, std::ios_base::openmode which = std::ios_base::out) override;

	private:
		Shared<IFile> file_;
	};

	class FileOutputStream : public std::ostream {
	public:
		explicit FileOutputStream(Shared<IFile>&& file)
			: std::ostream(&buf_), buf_(std::move(file)) {}

	private:
		FileOutputBuf buf_;
	};

	struct DirEntry {
		Path path;
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

		[[nodiscard]] virtual auto open_file(PathView path, std::ios_base::openmode mode) -> Shared<IFile> = 0;
		virtual auto create_directory(PathView path) -> bool = 0;
		virtual auto remove(PathView path) -> bool = 0;

		virtual auto exists(PathView path) -> bool = 0;
		virtual auto is_directory(PathView path) -> bool = 0;
		virtual auto is_file(PathView path) -> bool = 0;

		[[nodiscard]] virtual auto walk(PathView path, bool recursive = false) -> Shared<IDirectoryIterator> = 0;
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
		Path path;
		Shared<IFilesystem> filesystem;
	};

	auto initialize_filesystem() -> void;
	auto shutdown_filesystem() -> void;

	auto create_native_filesystem(PathView root_path) -> Shared<IFilesystem>;

	auto mount_filesystem(PathView mount_point, Shared<IFilesystem>&& filesystem) -> void;
	auto unmount_filesystem(PathView mount_point) -> bool;

	auto exists(PathView path) -> bool;
	auto is_directory(PathView path) -> bool;
	auto is_file(PathView path) -> bool;

	auto create_directory(PathView path) -> bool;
	auto create_directories(PathView path) -> bool;
	auto remove(PathView path) -> bool;

	auto get_work_directory_path() -> Path const&;
	auto get_temp_directory_path() -> Path const&;
	auto get_cache_directory_path() -> Path const&;

	[[nodiscard]] auto open_input_stream(PathView path, std::ios_base::openmode mode = 0) -> FileInputStream;
	[[nodiscard]] auto open_output_stream(PathView path, std::ios_base::openmode mode = 0) -> FileOutputStream;

	[[nodiscard]] auto walk_directory(PathView path, bool recursive = false) -> DirectoryIterator;
}