#include "filesystem.h"

namespace edge::fs {
	mi::Vector<MountPoint> mounts_;
	Path current_workdir_;
	Path temp_directory_;
	Path cache_directory_;

	extern auto get_system_cwd() -> Path;
	extern auto get_system_temp_dir() -> Path;
	extern auto get_system_cache_dir() -> Path;

	static auto resolve_path(PathView path) -> std::pair<Shared<IFilesystem>, Path> {
		auto normalized = normalize_path(path);

		Shared<IFilesystem> best_fs = nullptr;
		Path best_relative;
		size_t best_len = 0;

		for (const auto& mount : mounts_) {
			auto mount_path = normalize_path(mount.path);

			bool matches = false;

			// Special case for root mount
			if (mount_path == L"/") {
				matches = (normalized[0] == L'/');
			}
			else if (normalized == mount_path) {
				matches = true;
			}
			else if (normalized.length() > mount_path.length() &&
				normalized.find(mount_path) == 0 &&
				normalized[mount_path.length()] == L'/') {
				matches = true;
			}

			if (matches && mount_path.length() > best_len) {
				best_fs = mount.filesystem;
				best_len = mount_path.length();

				if (normalized == mount_path) {
					best_relative = L"/";
				}
				else if (mount_path == L"/") {
					best_relative = normalized;
				}
				else {
					best_relative = normalized.substr(mount_path.length());
				}
			}
		}

		return { best_fs, best_relative };
	}

	auto initialize_filesystem() -> void {
		mounts_ = {};
		current_workdir_ = get_system_cwd();
		temp_directory_ = get_system_temp_dir();
		cache_directory_ = get_system_cache_dir();
		mount_filesystem(L"/", create_native_filesystem(current_workdir_));
		mount_filesystem(L"/assets", create_native_filesystem(L"assets"));
	}

	auto shutdown_filesystem() -> void {
		mounts_.clear();
	}

	auto mount_filesystem(PathView mount_point, Shared<IFilesystem>&& filesystem) -> void {
		if (!filesystem) {
			return;
		}

		MountPoint mp;
		mp.path = normalize_path(mount_point);
		mp.filesystem = std::move(filesystem);
		mounts_.push_back(mp);
	}

	auto unmount_filesystem(PathView mount_point) -> bool {
		auto normalized = normalize_path(mount_point);

		for (auto it = mounts_.begin(); it != mounts_.end(); ++it) {
			if (normalize_path(it->path) == normalized) {
				mounts_.erase(it);
				return true;
			}
		}

		return false;
	}

	auto exists(PathView path) -> bool {
		auto [fs, rel_path] = resolve_path(path);
		if (!fs) {
			return false;
		}
		return fs->exists(rel_path);
	}

	auto is_directory(PathView path) -> bool {
		auto [fs, rel_path] = resolve_path(path);
		if (!fs) {
			return false;
		}
		return fs->is_directory(rel_path);
	}

	auto is_file(PathView path) -> bool {
		auto [fs, rel_path] = resolve_path(path);
		if (!fs) {
			return false;
		}
		return fs->is_file(rel_path);
	}

	auto create_directory(PathView path) -> bool {
		auto [fs, rel_path] = resolve_path(path);
		if (!fs) {
			return false;
		}
		return fs->create_directory(rel_path);
	}

	auto create_directories(PathView path) -> bool {
		auto normalized = normalize_path(path);
		mi::Vector<Path> parts = split_path(normalized);

		Path current = L"/";
		for (const auto& part : parts) {
			current += part;

			auto exists_result = exists(current);
			if (exists_result) {
				if (!create_directory(current)) {
					return false;
				}
			}

			current += L"/";
		}

		return true;
	}

	auto remove(PathView path) -> bool {
		auto [fs, rel_path] = resolve_path(path);
		if (!fs) {
			return false;
		}
		return fs->remove(rel_path);
	}

	auto get_work_directory_path() -> Path const& {
		return current_workdir_;
	}

	auto get_temp_directory_path() -> Path const& {
		return temp_directory_;
	}

	auto get_cache_directory_path() -> Path const& {
		return cache_directory_;
	}

	auto open_input_stream(PathView path, std::ios_base::openmode mode) -> FileInputStream {
		auto [fs, rel_path] = resolve_path(path);
		// TODO: return error
		//if (!fs) {
		//	return VFSResult<Shared<IFile>>(VFSError::NO_FILESYSTEM);
		//}
		return FileInputStream{ fs->open_file(rel_path, std::ios_base::in | mode) };
	}

	auto open_output_stream(PathView path, std::ios_base::openmode mode) -> FileOutputStream {
		auto [fs, rel_path] = resolve_path(path);
		// TODO: return error
		//if (!fs) {
		//	return VFSResult<Shared<IFile>>(VFSError::NO_FILESYSTEM);
		//}
		return FileOutputStream{ fs->open_file(rel_path, std::ios_base::out | mode) };
	}

	auto walk_directory(PathView path, bool recursive) -> DirectoryIterator {
		auto [fs, rel_path] = resolve_path(path);
		if (!fs) {
			return DirectoryIterator(nullptr);
		}
		return DirectoryIterator(fs->walk(rel_path, recursive));
	}

	std::streambuf::int_type FileInputBuf::underflow() {
		if (gptr() < egptr()) {
			return traits_type::to_int_type(*gptr());
		}

		char c;
		if (file_->read(&c, 1) != 1) {
			return traits_type::eof();
		}

		buffer_ = c;
		setg(&buffer_, &buffer_, &buffer_ + 1);
		return traits_type::to_int_type(c);
	}

	std::streambuf::pos_type FileInputBuf::seekoff(off_type off, std::ios_base::seekdir origin, std::ios_base::openmode which) {
		if (!(which & std::ios_base::in)) {
			return pos_type(off_type(-1));
		}

		auto result = file_->seek(static_cast<uint64_t>(off), origin);
		if (result < 0) {
			return pos_type(off_type(-1));
		}

		setg(nullptr, nullptr, nullptr);
		return pos_type(result);
	}

	std::streambuf::pos_type FileInputBuf::seekpos(std::streambuf::pos_type pos, std::ios_base::openmode which) {
		return seekoff(off_type(pos), std::ios_base::beg, which);
	}

	std::streambuf::int_type FileOutputBuf::overflow(int_type c) {
		if (c != traits_type::eof()) {
			char ch = traits_type::to_char_type(c);
			if (file_->write(&ch, 1) != 1) {
				return traits_type::eof();
			}
		}
		return c;
	}

	std::streamsize FileOutputBuf::xsputn(const char* s, std::streamsize n) {
		return file_->write(s, static_cast<uint64_t>(n));
	}

	std::streambuf::pos_type FileOutputBuf::seekoff(off_type off, std::ios_base::seekdir origin, std::ios_base::openmode which) {
		if (!(which & std::ios_base::out)) {
			return pos_type(off_type(-1));
		}

		int64_t result = file_->seek(static_cast<uint64_t>(off), origin);
		return result < 0 ? pos_type(off_type(-1)) : pos_type(result);
	}

	std::streambuf::pos_type FileOutputBuf::seekpos(pos_type pos, std::ios_base::openmode which) {
		return seekoff(off_type(pos), std::ios_base::beg, which);
	}
}