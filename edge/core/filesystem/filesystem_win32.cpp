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
            : base_path_{ path }
            , is_end_(false)
            , recursive_(recursive)
            , current_handle_(INVALID_HANDLE_VALUE) {
            stack_.push_back(mi::U8String{ path });
            advance();
        }

        [[nodiscard]] auto end() const noexcept -> bool override {
            return is_end_;
        }

        auto next() -> void override {
            if (!is_end_) {
                advance();
            }
        }

        [[nodiscard]] auto value() const noexcept -> const DirEntry& override {
            return current_;
        }
    private:
        auto close_current_handle() -> void {
            if (current_handle_ != INVALID_HANDLE_VALUE) {
                FindClose(current_handle_);
                current_handle_ = INVALID_HANDLE_VALUE;
            }
        }

        auto advance() -> void {
            while (true) {
                // Try to get next entry from current directory
                if (current_handle_ != INVALID_HANDLE_VALUE) {
                    WIN32_FIND_DATAW find_data;

                    while (FindNextFileW(current_handle_, &find_data)) {
                        auto name = unicode::make_utf8_string(find_data.cFileName);
                        if (name == u8"." || name == u8"..") {
                            continue;
                        }

                        auto full_path = path::append(current_directory_, name, u8'\\');
                        bool is_dir = (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

                        if (is_dir && recursive_) {
                            stack_.push_back(full_path);
                        }

                        current_.path = path::to_posix(full_path.substr(base_path_.size()));
                        current_.is_directory = is_dir;
                        current_.size = (static_cast<uint64_t>(find_data.nFileSizeHigh) << 32) | find_data.nFileSizeLow;

                        return;
                    }

                    // No more entries in current directory, close handle
                    close_current_handle();
                }

                // Need to open a new directory
                if (stack_.empty()) {
                    is_end_ = true;
                    return;
                }

                current_directory_ = stack_.back();
                stack_.pop_back();

                auto search_path = path::append(current_directory_, u8"*", u8'\\');
                auto search_path_utf16 = unicode::make_wide_string(search_path);

                WIN32_FIND_DATAW find_data;
                current_handle_ = FindFirstFileW(search_path_utf16.c_str(), &find_data);

                if (current_handle_ == INVALID_HANDLE_VALUE) {
                    continue; // Try next directory in stack
                }

                // Process first entry
                auto name = unicode::make_utf8_string(find_data.cFileName);
                if (name == u8"." || name == u8"..") {
                    continue; // Will loop back and call FindNextFileW
                }

                auto full_path = path::append(current_directory_, name, u8'\\');
                bool is_dir = (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

                if (is_dir && recursive_) {
                    stack_.push_back(full_path);
                }

                current_.path = path::to_posix(full_path.substr(base_path_.size()));
                current_.is_directory = is_dir;
                current_.size = (static_cast<uint64_t>(find_data.nFileSizeHigh) << 32) | find_data.nFileSizeLow;

                return;
            }
        }

        mi::U8String base_path_;
        mi::U8String current_directory_;
        mi::Vector<mi::U8String> stack_;
        DirEntry current_;
        HANDLE current_handle_;
        bool is_end_;
        bool recursive_;
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