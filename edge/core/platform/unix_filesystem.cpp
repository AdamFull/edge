#include "unix_filesystem.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <locale>
#include <codecvt>

namespace edge::platform {
    auto get_system_cwd() -> mi::U8String {
        // TODO: filesystem
        return {};
    }

    auto get_system_temp_dir() -> mi::U8String {
        // TODO: filesystem
        return {};
    }

    auto get_system_cache_dir() -> mi::U8String {
        // TODO: filesystem
        return {};
    }

    NativeDirectoryIterator::NativeDirectoryIterator(std::u8string_view path, bool recursive)
        : base_path_(path)
        , recursive_(recursive)
    {
        if (!open_directory(base_path_, u8"")) {
            at_end_ = true;
            return;
        }

        if (!advance_to_valid_entry()) {
            at_end_ = true;
        }
    }

    NativeDirectoryIterator::~NativeDirectoryIterator() {
        while (!dir_stack_.empty()) {
            if (dir_stack_.back().dir_handle) {
                closedir(dir_stack_.back().dir_handle);
            }
            dir_stack_.pop_back();
        }
    }

    auto NativeDirectoryIterator::end() const noexcept -> bool {
        return at_end_;
    }

    auto NativeDirectoryIterator::next() -> void {
        if (at_end_) {
            return;
        }

        if (!advance_to_valid_entry()) {
            at_end_ = true;
        }
    }

    auto NativeDirectoryIterator::value() const noexcept -> const DirEntry& {
        return current_entry_;
    }

    auto NativeDirectoryIterator::open_directory(std::u8string_view dir_path, std::u8string_view relative_path) -> bool {
        DIR* dir = opendir(reinterpret_cast<const char*>(dir_path.data()));

        if (!dir) {
            return false;
        }

        DirectoryState state;
        state.dir_handle = dir;
        state.current_dir = dir_path;
        state.relative_path = relative_path;

        dir_stack_.push_back(std::move(state));
        return true;
    }

    auto NativeDirectoryIterator::get_file_info(std::u8string_view full_path, bool& is_dir, uint64_t& size) -> bool {
        struct stat st;

        if (stat(reinterpret_cast<const char*>(full_path.data()), &st) != 0) {
            return false;
        }

        is_dir = S_ISDIR(st.st_mode);
        size = static_cast<uint64_t>(st.st_size);
        return true;
    }

    auto NativeDirectoryIterator::advance_to_valid_entry() -> bool {
        while (!dir_stack_.empty()) {
            auto& state = dir_stack_.back();

            struct dirent* entry = readdir(state.dir_handle);
            if (!entry) {
                closedir(state.dir_handle);
                dir_stack_.back();
                continue;
            }

            auto filename = unicode::make_utf8_string(entry->d_name);
            if (filename == u8"." || filename == u8"..") {
                continue;
            }

            bool is_dir;
            uint64_t size;

            auto full_path = fs::path::append(state.current_dir, filename);
            if (!get_file_info(full_path, is_dir, size)) {
                // Skip entries we can't stat
                continue;
            }

            current_entry_.path = state.relative_path.empty() ? filename : fs::path::append(state.relative_path, filename);
            current_entry_.is_directory = is_dir;
            current_entry_.size = size;

            // If recursive and this is a directory, queue it for later
            if (recursive_ && is_dir) {
                auto subdir_path = full_path + u8'/';
                open_directory(subdir_path, current_entry_.path);
            }

            return true;
        }

        return false;
    }

    NativeFile::NativeFile(FILE* file) : file_(file), size_(0) {
        if (file_) {
            fseeko(file_, 0, SEEK_END);
            size_ = ftello(file_);
            fseeko(file_, 0, SEEK_SET);
        }
    }

    NativeFile::~NativeFile() {
        close();
    }

    auto NativeFile::is_open() -> bool {
        return file_ != nullptr;
    }

    auto NativeFile::close() -> void {
        if (file_) {
            fclose(file_);
            file_ = nullptr;
        }
    }

    auto NativeFile::size() -> uint64_t {
        return size_;
    }

    auto NativeFile::seek(uint64_t offset, std::ios_base::seekdir origin) -> int64_t {
        if (!is_open()) return -1;

        int whence;
        switch (origin) {
        case std::ios_base::beg: whence = SEEK_SET; break;
        case std::ios_base::cur: whence = SEEK_CUR; break;
        case std::ios_base::end: whence = SEEK_END; break;
        default: return -1;
        }

        if (fseeko(file_, offset, whence) != 0) {
            return -1;
        }

        return ftello(file_);
    }

    auto NativeFile::tell() -> int64_t {
        if (!is_open()) return -1;
        return ftello(file_);
    }

    auto NativeFile::read(void* buffer, uint64_t size) -> int64_t {
        if (!is_open()) return -1;
        return fread(buffer, 1, size, file_);
    }

    auto NativeFile::write(const void* buffer, uint64_t size) -> int64_t {
        if (!is_open()) return -1;
        return fwrite(buffer, 1, size, file_);
    }

    NativeFilesystem::NativeFilesystem(std::u8string_view root)
        : root_path_(root) {

    }

    auto NativeFilesystem::exists(std::u8string_view path) -> bool {
        auto native = to_native_path(path);

        struct stat st;
        return stat(reinterpret_cast<const char*>(native.c_str()), &st) == 0;
    }

    auto NativeFilesystem::is_file(std::u8string_view path) -> bool {
        auto native = to_native_path(path);

        struct stat st;
        if (stat(reinterpret_cast<const char*>(native.c_str()), &st) != 0) {
            return false;
        }
        return S_ISREG(st.st_mode);
    }

    auto NativeFilesystem::is_directory(std::u8string_view path) -> bool {
        auto native = to_native_path(path);

        struct stat st;
        if (stat(reinterpret_cast<const char*>(native.c_str()), &st) != 0) {
            return false;
        }
        return S_ISDIR(st.st_mode);
    }

    auto NativeFilesystem::open_file(std::u8string_view path, std::ios_base::openmode mode) -> Shared<IPlatformFile> {
        const char* mode_str = "rb";

        if ((mode & std::ios_base::in) && (mode & std::ios_base::out)) {
            if (mode & std::ios_base::trunc) {
                mode_str = (mode & std::ios_base::binary) ? "w+b" : "w+";
            }
            else if (mode & std::ios_base::app) {
                mode_str = (mode & std::ios_base::binary) ? "a+b" : "a+";
            }
            else {
                mode_str = (mode & std::ios_base::binary) ? "r+b" : "r+";
            }
        }
        else if (mode & std::ios_base::out) {
            if (mode & std::ios_base::app) {
                mode_str = (mode & std::ios_base::binary) ? "ab" : "a";
            }
            else if (mode & std::ios_base::trunc) {
                mode_str = (mode & std::ios_base::binary) ? "wb" : "w";
            }
            else {
                mode_str = (mode & std::ios_base::binary) ? "wb" : "w";
            }
        }
        else if (mode & std::ios_base::in) {
            mode_str = (mode & std::ios_base::binary) ? "rb" : "r";
        }

        auto native = to_native_path(path);
        FILE* file = fopen(reinterpret_cast<const char*>(native.c_str()), mode_str);
        if (!file) {
            return nullptr;
        }
        return std::make_shared<NativeFile>(file);
    }

    auto NativeFilesystem::create_directory(std::u8string_view path) -> bool {
        auto native = to_native_path(path);
        if (mkdir(reinterpret_cast<const char*>(native.c_str()), 0755) != 0) {
            if (errno != EEXIST) {
                return false;
            }
        }
        return true;
    }

    auto NativeFilesystem::remove(std::u8string_view path) -> bool {
        auto native = to_native_path(path);
        auto native_char = reinterpret_cast<const char*>(native.c_str());

        struct stat st;
        if (stat(native_char, &st) != 0) {
            return false;
        }

        if (S_ISDIR(st.st_mode)) {
            if (rmdir(native_char) != 0) {
                return false;
            }
        }
        else {
            if (unlink(native_char) != 0) {
                return false;
            }
        }
        return true;
    }

    auto NativeFilesystem::walk(std::u8string_view path, bool recursive) -> Shared<IPlatformDirectoryIterator> {
        auto native = to_native_path(path);
        return std::make_shared<NativeDirectoryIterator>(native, recursive);
    }

    auto NativeFilesystem::to_native_path(std::u8string_view vfs_path) -> mi::U8String {
        return fs::path::append(root_path_, vfs_path);
    }
}