#include "filesystem.h"

#if EDGE_PLATFORM_ANDROID
#include "../platform/unix_filesystem.h"
#include "../platform/zip_filesystem.h"
#elif EDGE_PLATFORM_WINDOWS
#include "../platform/windows_filesystem.h"
#include "../platform/zip_filesystem.h"
#endif

namespace edge::platform {
	extern auto get_system_cwd() -> mi::U8String;
	extern auto get_system_temp_dir() -> mi::U8String;
	extern auto get_system_cache_dir() -> mi::U8String;
}

namespace edge::fs {
	mi::Vector<MountPoint> mounts_;
	mi::U8String current_workdir_;
	mi::U8String temp_directory_;
	mi::U8String cache_directory_;

	static auto resolve_path(std::u8string_view path) -> std::tuple<Shared<platform::IPlatformFilesystem>, std::u8string_view> {
		size_t best_index{ ~0ull };
		size_t best_match_length{ 0ull };

		for (int32_t i = 0; i < static_cast<int32_t>(mounts_.size()); ++i) {
			auto const& mount = mounts_[i];
			if (path.size() >= mount.path.size() && path.substr(0, mount.path.size()) == std::u8string_view(mount.path)) {
				// Found a match, check if it's longer than current best
				if (mount.path.size() > best_match_length) {
					best_index = i;
					best_match_length = mount.path.size();
				}
			}
		}

		if (best_index != ~0ull) {
			return { mounts_[best_index].filesystem, path.substr(best_match_length)};
		}

		return { nullptr, path };
	}

	auto initialize_filesystem() -> void {
		mounts_ = {};
		current_workdir_ = platform::get_system_cwd();
		temp_directory_ = platform::get_system_temp_dir();
		cache_directory_ = platform::get_system_cache_dir();
		mount_filesystem(u8"/", create_native_filesystem(current_workdir_));
	}

	auto shutdown_filesystem() -> void {
		mounts_.clear();
	}

	auto create_native_filesystem(std::u8string_view root_path) -> Shared<platform::IPlatformFilesystem> {
		return std::make_unique<platform::NativeFilesystem>(root_path);
	}

	auto mount_filesystem(std::u8string_view mount_point, Shared<platform::IPlatformFilesystem>&& filesystem) -> void {
		if (!filesystem) {
			return;
		}

		MountPoint mp;
		// TODO: normalize path
		mp.path = mount_point;
		mp.filesystem = std::move(filesystem);
		mounts_.push_back(mp);
	}

	auto unmount_filesystem(std::u8string_view mount_point) -> bool {
		for (auto it = mounts_.begin(); it != mounts_.end(); ++it) {
			if (mount_point.compare(it->path) == 0) {
				mounts_.erase(it);
				return true;
			}
		}

		return false;
	}

	auto exists(std::u8string_view path) -> bool {
		auto [fs, rel_path] = resolve_path(path);
		if (!fs) {
			return false;
		}
		return fs->exists(rel_path);
	}

	auto is_directory(std::u8string_view path) -> bool {
		auto [fs, rel_path] = resolve_path(path);
		if (!fs) {
			return false;
		}
		return fs->is_directory(rel_path);
	}

	auto is_file(std::u8string_view path) -> bool {
		auto [fs, rel_path] = resolve_path(path);
		if (!fs) {
			return false;
		}
		return fs->is_file(rel_path);
	}

	auto create_directory(std::u8string_view path) -> bool {
		auto [fs, rel_path] = resolve_path(path);
		if (!fs) {
			return false;
		}
		return fs->create_directory(rel_path);
	}

	auto create_directories(std::u8string_view path) -> bool {
		auto parts = path::split_components(path);

		mi::U8String current = u8"/";
		for (const auto& part : parts) {
			current = path::append(current, part);

			auto exists_result = exists(current);
			if (exists_result) {
				if (!create_directory(current)) {
					return false;
				}
			}

			current = path::append(current, u8"/");
		}

		return true;
	}

	auto remove(std::u8string_view path) -> bool {
		auto [fs, rel_path] = resolve_path(path);
		if (!fs) {
			return false;
		}
		return fs->remove(rel_path);
	}

	auto get_work_directory_path() -> std::u8string_view {
		return current_workdir_;
	}

	auto get_temp_directory_path() -> std::u8string_view {
		return temp_directory_;
	}

	auto get_cache_directory_path() -> std::u8string_view {
		return cache_directory_;
	}

	// TODO: Maybe add filesystem cache for entry to be able to not search for it again?
	auto walk_directory(std::u8string_view path, bool recursive) -> DirectoryIterator {
		auto [fs, rel_path] = resolve_path(path);
		if (!fs) {
			return DirectoryIterator(nullptr);
		}
		return DirectoryIterator(fs->walk(rel_path, recursive));
	}

	FileStreamBuf::FileStreamBuf(std::u8string_view path, std::ios_base::openmode mode, size_t input_buffer_size, size_t output_buffer_size)
		: input_buffer_(input_buffer_size)
		, output_buffer_(output_buffer_size){

		auto* out_begin = output_buffer_.data();
		setp(out_begin, out_begin + output_buffer_.size());

		open(path, mode);
	}

	auto FileStreamBuf::open(std::u8string_view path, std::ios_base::openmode mode) -> bool {
		if (file_ && file_->is_open()) {
			file_->close();
		}

		auto [fs, fs_path] = resolve_path(path);
		if (!fs) {
			return false;
		}

		file_ = fs->open_file(fs_path, mode);
		return file_ != nullptr;
	}

	auto FileStreamBuf::is_open() const noexcept -> bool {
		return file_ && file_->is_open();
	}

	auto FileStreamBuf::close() noexcept -> void {
		if (file_) {
			file_->close();
		}
	}

	std::streambuf::int_type FileStreamBuf::underflow() {
		if (gptr() < egptr()) {
			return traits_type::to_int_type(*gptr());
		}

		if (!file_ || !file_->is_open()) {
			return traits_type::eof();
		}

		auto bytes_read = file_->read(input_buffer_.data(), input_buffer_.size());
		if (bytes_read <= 0) {
			return traits_type::eof();
		}

		setg(input_buffer_.data(), input_buffer_.data(), input_buffer_.data() + bytes_read);

		return traits_type::to_int_type(*gptr());
	}

	std::streambuf::int_type FileStreamBuf::overflow(int_type ch) {
		if (!file_ || !file_->is_open()) {
			return traits_type::eof();
		}

		if (sync() == -1) {
			return traits_type::eof();
		}

		if (!traits_type::eq_int_type(ch, traits_type::eof())) {
			*pptr() = traits_type::to_char_type(ch);
			pbump(1);
		}

		return traits_type::not_eof(ch);
	}

	int FileStreamBuf::sync() {
		if (!file_ || !file_->is_open()) {
			return -1;
		}

		std::ptrdiff_t num_bytes = pptr() - pbase();
		if (num_bytes > 0) {
			auto written = file_->write(pbase(), static_cast<uint64_t>(num_bytes));
			if (written != num_bytes) {
				return -1;
			}

			// Reset output buffer
			pbump(-static_cast<int>(num_bytes));
		}

		// Reset input buffer to force re-read after write
		setg(input_buffer_.data(), input_buffer_.data(), input_buffer_.data());

		return 0;
	}

	std::streambuf::pos_type FileStreamBuf::seekoff(off_type offset, std::ios_base::seekdir dir, std::ios_base::openmode which) {
		if (!file_ || !file_->is_open()) {
			return pos_type(off_type(-1));
		}

		// Flush output buffer before seeking
		if (which & std::ios_base::out) {
			if (sync() == -1) {
				return pos_type(off_type(-1));
			}
		}

		// Invalidate input buffer
		setg(input_buffer_.data(), input_buffer_.data(), input_buffer_.data());

		auto new_pos = file_->seek(static_cast<uint64_t>(offset), dir);
		if (new_pos < 0) {
			return pos_type(off_type(-1));
		}

		return pos_type(new_pos);
	}

	std::streambuf::pos_type FileStreamBuf::seekpos(pos_type pos, std::ios_base::openmode which) {
		return seekoff(pos, std::ios_base::beg, which);
	}

	std::streamsize FileStreamBuf::showmanyc() {
		if (!file_ || !file_->is_open()) {
			return -1;
		}

		return egptr() - gptr();
	}




	auto read_whole_file(std::u8string_view path, std::ios_base::openmode mode, mi::Vector<uint8_t>& out) -> bool {
		fs::InputFileStream file(path, mode);
		if (!file.is_open()) {
			return false;
		}

		file.seekg(0, std::ios::end);
		auto file_size = file.tellg();
		file.seekg(0, std::ios::beg);

		out.resize(file_size);

		file.read(reinterpret_cast<char*>(out.data()), file_size);
		return true;
	}
}