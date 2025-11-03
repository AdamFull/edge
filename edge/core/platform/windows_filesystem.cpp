#include "windows_filesystem.h"

#include <windows.h>
#include <shlobj.h>
#include <direct.h>

namespace edge::platform {
    auto get_system_cwd() -> mi::U8String {
        wchar_t buffer[MAX_PATH];
        DWORD length = GetCurrentDirectoryW(MAX_PATH, buffer);
        return unicode::make_utf8_string({ buffer, length });
    }

    auto get_system_temp_dir() -> mi::U8String {
        wchar_t buffer[MAX_PATH];
        DWORD length = GetTempPathW(MAX_PATH, buffer);
        return unicode::make_utf8_string({ buffer, length });
    }

    auto get_system_cache_dir() -> mi::U8String {
        wchar_t buffer[MAX_PATH];
        HRESULT result = SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, buffer);
        return unicode::make_utf8_string({ buffer });
    }

    NativeDirectoryIterator::NativeDirectoryIterator(std::u8string_view path, bool recursive)
        : base_path_(path)
        , recursive_(recursive)
    {
        if (!open_directory(base_path_)) {
            at_end_ = true;
            return;
        }

        if (!advance_to_valid_entry()) {
            at_end_ = true;
        }
    }

    NativeDirectoryIterator::~NativeDirectoryIterator() {
        while (!dir_stack_.empty()) {
            if (dir_stack_.back().handle != INVALID_HANDLE_VALUE) {
                FindClose(dir_stack_.back().handle);
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

    auto NativeDirectoryIterator::open_directory(std::u8string_view dir_path) -> bool {
        auto search_path = fs::path::append(dir_path, u8"*", u8'\\');
        auto wsearch_path = unicode::make_wide_string(search_path);

        WIN32_FIND_DATAW find_data{};
        HANDLE handle = FindFirstFileW(wsearch_path.c_str(), &find_data);

        if (handle == INVALID_HANDLE_VALUE) {
            return false;
        }

        DirectoryState state;
        state.current_dir = unicode::make_wide_string(dir_path);
        state.relative_path = u8"";
        state.handle = handle;
        state.find_data = find_data;
        state.first = true;

        dir_stack_.push_back(std::move(state));
        return true;
    }

    auto NativeDirectoryIterator::open_subdirectory(std::u8string_view relative_path, std::u8string_view name) -> bool {
        DirectoryState state;
        state.relative_path = relative_path.empty() ? name : fs::path::append(relative_path, name, u8'\\');
        state.first = true;

        auto full_path = fs::path::append(base_path_, state.relative_path, u8'\\');
        auto search_path = fs::path::append(full_path, u8"*", u8'\\');
        auto wsearch_path = unicode::make_wide_string(search_path);

        state.handle = FindFirstFileW(wsearch_path.c_str(), &state.find_data);

        if (state.handle == INVALID_HANDLE_VALUE) {
            return false;
        }

        state.current_dir = unicode::make_wide_string(full_path);

        dir_stack_.push_back(std::move(state));
        return true;
    }

    auto NativeDirectoryIterator::advance_to_valid_entry() -> bool {
        while (!dir_stack_.empty()) {
            auto& state = dir_stack_.back();

            if (state.first) {
                state.first = false;
            }
            else {
                if (!FindNextFileW(state.handle, &state.find_data)) {
                    FindClose(state.handle);
                    dir_stack_.pop_back();
                    continue;
                }
            }

            auto filename = unicode::make_utf8_string(state.find_data.cFileName);

            // Skip . and ..
            if (filename == u8"." || filename == u8"..") {
                continue;
            }

            // Build current entry
            bool is_dir = (state.find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            uint64_t size = (static_cast<uint64_t>(state.find_data.nFileSizeHigh) << 32) |
                state.find_data.nFileSizeLow;

            current_entry_.path = state.relative_path.empty() ? filename : fs::path::append(state.relative_path, filename, u8'/');
            current_entry_.is_directory = is_dir;
            current_entry_.size = size;

            // If recursive and this is a directory, queue it for later
            if (recursive_ && is_dir) {
                open_subdirectory(state.relative_path, filename);
            }

            return true;
        }

        return false;
    }


    NativeFile::NativeFile(HANDLE handle)
        : handle_(handle)
        , size_(0) {
        if (handle_ != INVALID_HANDLE_VALUE) {
            LARGE_INTEGER li;
            if (GetFileSizeEx(handle_, &li)) {
                size_ = static_cast<uint64_t>(li.QuadPart);
            }
        }
    }

    NativeFile::~NativeFile() {
        close();
    }

    auto NativeFile::is_open() -> bool {
        return handle_ != INVALID_HANDLE_VALUE;
    }

    auto NativeFile::close() -> void {
        if (handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(handle_);
            handle_ = INVALID_HANDLE_VALUE;
        }
    }

    auto NativeFile::size() -> uint64_t {
        return size_;
    }

    auto NativeFile::seek(uint64_t offset, std::ios_base::seekdir origin) -> int64_t {
        if (!is_open()) {
            return -1;
        }

        DWORD move_method;
        switch (origin) {
        case std::ios_base::beg: move_method = FILE_BEGIN; break;
        case std::ios_base::cur: move_method = FILE_CURRENT; break;
        case std::ios_base::end: move_method = FILE_END; break;
        default: return -1;
        }

        LARGE_INTEGER li;
        li.QuadPart = offset;
        li.LowPart = SetFilePointer(handle_, li.LowPart, &li.HighPart, move_method);

        if (li.LowPart == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR) {
            return -1;
        }

        return li.QuadPart;
    }

    auto NativeFile::tell() -> int64_t {
        if (!is_open()) {
            return -1;
        }

        LARGE_INTEGER li;
        li.QuadPart = 0;
        li.LowPart = SetFilePointer(handle_, 0, &li.HighPart, FILE_CURRENT);

        if (li.LowPart == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR) {
            return -1;
        }

        return li.QuadPart;
    }

    auto NativeFile::read(void* buffer, uint64_t size) -> int64_t {
        if (!is_open()) {
            return -1;
        }

        DWORD bytes_read = 0;
        if (!ReadFile(handle_, buffer, static_cast<DWORD>(size), &bytes_read, nullptr)) {
            return -1;
        }

        return bytes_read;
    }

    auto NativeFile::write(const void* buffer, uint64_t size) -> int64_t {
        if (!is_open()) return -1;

        DWORD bytes_written = 0;
        if (!WriteFile(handle_, buffer, static_cast<DWORD>(size), &bytes_written, nullptr)) {
            return -1;
        }

        return bytes_written;
    }

    NativeFilesystem::NativeFilesystem(std::u8string_view root)
        : root_path_{ root } {
    }

    auto NativeFilesystem::open_file(std::u8string_view path, std::ios_base::openmode mode) -> Shared<IPlatformFile> {
        auto native_path = to_native_path(path);

        DWORD access = 0;
        DWORD creation = OPEN_EXISTING;
        DWORD share = FILE_SHARE_READ;

        if (mode & std::ios_base::in) {
            access |= GENERIC_READ;
        }
        if (mode & std::ios_base::out) {
            access |= GENERIC_WRITE;
            share = 0;

            if (mode & std::ios_base::app) {
                creation = OPEN_ALWAYS;
            }
            else if (mode & std::ios_base::trunc) {
                creation = CREATE_ALWAYS;
            }
            else {
                creation = CREATE_ALWAYS;
            }
        }
        if ((mode & std::ios_base::in) && !(mode & std::ios_base::out)) {
            creation = OPEN_EXISTING;
        }

        HANDLE handle = CreateFileW(
            native_path.c_str(),
            access,
            share,
            nullptr,
            creation,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );

        auto file = std::make_shared<NativeFile>(handle);

        if (mode & std::ios_base::app) {
            file->seek(0, std::ios_base::end);
        }

        return file;
    }

    auto NativeFilesystem::create_directory(std::u8string_view path) -> bool {
        auto native_path = to_native_path(path);
        if (!CreateDirectoryW(native_path.c_str(), nullptr)) {
            DWORD error = GetLastError();
            if (error != ERROR_ALREADY_EXISTS) {
                return false;
            }
        }
        return true;
    }

    auto NativeFilesystem::remove(std::u8string_view path) -> bool {
        auto native_path = to_native_path(path);

        DWORD attrs = GetFileAttributesW(native_path.c_str());
        if (attrs == INVALID_FILE_ATTRIBUTES) {
            return false;
        }

        if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
            if (!RemoveDirectoryW(native_path.c_str())) {
                return false;
            }
        }
        else {
            if (!DeleteFileW(native_path.c_str())) {
                return false;
            }
        }
        return true;
    }

    auto NativeFilesystem::exists(std::u8string_view path) -> bool {
        auto native_path = to_native_path(path);
        return GetFileAttributesW(native_path.c_str()) != INVALID_FILE_ATTRIBUTES;
    }

    auto NativeFilesystem::is_directory(std::u8string_view path) -> bool {
        auto native_path = to_native_path(path);
        DWORD attrs = GetFileAttributesW(native_path.c_str());
        if (attrs == INVALID_FILE_ATTRIBUTES) {
            return false;
        }
        return (attrs & FILE_ATTRIBUTE_DIRECTORY);
    }

    auto NativeFilesystem::is_file(std::u8string_view path) -> bool {
        auto native_path = to_native_path(path);
        DWORD attrs = GetFileAttributesW(native_path.c_str());
        if (attrs == INVALID_FILE_ATTRIBUTES) {
            return false;
        }
        return !(attrs & FILE_ATTRIBUTE_DIRECTORY);
    }

    auto NativeFilesystem::walk(std::u8string_view path, bool recursive) -> Shared<IPlatformDirectoryIterator> {
        auto native_path = to_native_path(path);
        auto utf8_path = unicode::make_utf8_string(native_path);
        return std::make_shared<NativeDirectoryIterator>(utf8_path, recursive);
    }
    
    auto NativeFilesystem::to_native_path(std::u8string_view path) -> mi::WString {
        auto new_path = fs::path::to_windows(fs::path::append(root_path_, path));
        return unicode::make_wide_string(new_path);
    }
}