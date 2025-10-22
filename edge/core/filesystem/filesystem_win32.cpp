#include "filesystem.h"

#include <windows.h>
#include <shlobj.h>
#include <direct.h>

namespace edge::fs {
    auto get_system_cwd() -> mi::U8String {
        wchar_t buffer[MAX_PATH];
        DWORD length = GetCurrentDirectoryW(MAX_PATH, buffer);
        return unicode::make_utf8_string({buffer, length});
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

    class NativeDirectoryIterator final : public IDirectoryIterator {
    public:
        explicit NativeDirectoryIterator(std::u8string_view path, bool recursive)
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

        ~NativeDirectoryIterator() {
            while (!dir_stack_.empty()) {
                if (dir_stack_.back().handle != INVALID_HANDLE_VALUE) {
                    FindClose(dir_stack_.back().handle);
                }
                dir_stack_.pop_back();
            }
        }

        [[nodiscard]] auto end() const noexcept -> bool override {
            return at_end_;
        }

        auto next() -> void override {
            if (at_end_) {
                return;
            }

            if (!advance_to_valid_entry()) {
                at_end_ = true;
            }
        }

        auto value() const noexcept -> const DirEntry & override {
            return current_entry_;
        }
    private:
        struct DirectoryState {
            mi::WString current_dir;
            mi::U8String relative_path;
            HANDLE handle = INVALID_HANDLE_VALUE;
            WIN32_FIND_DATAW find_data{};
            bool first = true;
        };

        auto open_directory(std::u8string_view dir_path) -> bool {
            auto search_path = path::append(dir_path, u8"*", u8'\\');
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

        auto open_subdirectory(std::u8string_view relative_path, std::u8string_view name) -> bool {
            DirectoryState state;
            state.relative_path = relative_path.empty() ? name : path::append(relative_path, name, u8'\\');
            state.first = true;

            auto full_path = path::append(base_path_, state.relative_path, u8'\\');
            auto search_path = path::append(full_path, u8"*", u8'\\');
            auto wsearch_path = unicode::make_wide_string(search_path);

            state.handle = FindFirstFileW(wsearch_path.c_str(), &state.find_data);

            if (state.handle == INVALID_HANDLE_VALUE) {
                return false;
            }

            state.current_dir = unicode::make_wide_string(full_path);
            
            dir_stack_.push_back(std::move(state));
            return true;
        }

        auto advance_to_valid_entry() -> bool {
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

                current_entry_.path = state.relative_path.empty() ? filename : path::append(state.relative_path, filename, u8'/');
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

        mi::U8String base_path_;
        bool recursive_;
        mi::Vector<DirectoryState> dir_stack_;
        DirEntry current_entry_;
        bool at_end_ = false;
    };

    class NativeFile final : public IFile {
    public:
        explicit NativeFile(HANDLE handle) 
            : handle_(handle)
            , size_(0) {
            if (handle_ != INVALID_HANDLE_VALUE) {
                LARGE_INTEGER li;
                if (GetFileSizeEx(handle_, &li)) {
                    size_ = static_cast<uint64_t>(li.QuadPart);
                }
            }
        }

        ~NativeFile() override {
            close();
        }

        [[nodiscard]] auto is_open() -> bool override {
            return handle_ != INVALID_HANDLE_VALUE;
        }

        auto close() -> void override {
            if (handle_ != INVALID_HANDLE_VALUE) {
                CloseHandle(handle_);
                handle_ = INVALID_HANDLE_VALUE;
            }
        }

        [[nodiscard]] auto size() -> uint64_t override {
            return size_;
        }

        auto seek(uint64_t offset, std::ios_base::seekdir origin) -> int64_t override {
            if (!is_open()) return -1;

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

        [[nodiscard]] auto tell() -> int64_t override {
            if (!is_open()) return -1;

            LARGE_INTEGER li;
            li.QuadPart = 0;
            li.LowPart = SetFilePointer(handle_, 0, &li.HighPart, FILE_CURRENT);

            if (li.LowPart == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR) {
                return -1;
            }

            return li.QuadPart;
        }

        auto read(void* buffer, uint64_t size) -> int64_t override {
            if (!is_open()) return -1;

            DWORD bytes_read = 0;
            if (!ReadFile(handle_, buffer, static_cast<DWORD>(size), &bytes_read, nullptr)) {
                return -1;
            }

            return bytes_read;
        }

        auto write(const void* buffer, uint64_t size) -> int64_t override {
            if (!is_open()) return -1;

            DWORD bytes_written = 0;
            if (!WriteFile(handle_, buffer, static_cast<DWORD>(size), &bytes_written, nullptr)) {
                return -1;
            }

            return bytes_written;
        }
    private:
        HANDLE handle_;
        uint64_t size_;
    };

    class NativeFilesystem final : public IFilesystem {
    public:
        NativeFilesystem(std::u8string_view root)
            : root_path_{ root } {
        }

        [[nodiscard]] auto open_file(std::u8string_view path, std::ios_base::openmode mode) -> Shared<IFile> override {
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

        auto create_directory(std::u8string_view path) -> bool override {
            auto native_path = to_native_path(path);
            if (!CreateDirectoryW(native_path.c_str(), nullptr)) {
                DWORD error = GetLastError();
                if (error != ERROR_ALREADY_EXISTS) {
                    return false;
                }
            }
            return true;
        }

        auto remove(std::u8string_view path) -> bool override {
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

        auto exists(std::u8string_view path) -> bool override {
            auto native_path = to_native_path(path);
            return GetFileAttributesW(native_path.c_str()) != INVALID_FILE_ATTRIBUTES;
        }

        auto is_directory(std::u8string_view path) -> bool override {
            auto native_path = to_native_path(path);
            DWORD attrs = GetFileAttributesW(native_path.c_str());
            if (attrs == INVALID_FILE_ATTRIBUTES) {
                return false;
            }
            return (attrs & FILE_ATTRIBUTE_DIRECTORY);
        }

        auto is_file(std::u8string_view path) -> bool override {
            auto native_path = to_native_path(path);
            DWORD attrs = GetFileAttributesW(native_path.c_str());
            if (attrs == INVALID_FILE_ATTRIBUTES) {
                return false;
            }
            return !(attrs & FILE_ATTRIBUTE_DIRECTORY);
        }

        [[nodiscard]] auto walk(std::u8string_view path, bool recursive = false) -> Shared<IDirectoryIterator> override {
            auto native_path = to_native_path(path);
            auto utf8_path = unicode::make_utf8_string(native_path);
            return std::make_shared<NativeDirectoryIterator>(utf8_path, recursive);
        }
    private:
        auto to_native_path(std::u8string_view path) -> mi::WString {
            auto new_path = path::to_windows(path::append(root_path_, path));
            return unicode::make_wide_string(new_path);
        }

        mi::U8String root_path_;
    };

    auto create_native_filesystem(std::u8string_view root_path) -> Shared<IFilesystem> {
        return std::make_shared<NativeFilesystem>(root_path);
    }
}