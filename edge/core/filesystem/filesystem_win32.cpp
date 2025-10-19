#include "filesystem.h"

#include <windows.h>
#include <shlobj.h>
#include <direct.h>

namespace edge::fs {
    auto get_system_cwd() -> Path {
        wchar_t buffer[MAX_PATH];
        DWORD length = GetCurrentDirectoryW(MAX_PATH, buffer);
        return Path{ buffer, length };
    }

    auto get_system_temp_dir() -> Path {
        wchar_t buffer[MAX_PATH];
        DWORD length = GetTempPathW(MAX_PATH, buffer);
        return Path{ buffer, length };
    }

    auto get_system_cache_dir() -> Path {
        wchar_t buffer[MAX_PATH];
        HRESULT result = SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, buffer);
        return Path{ buffer };
    }

    class NativeDirectoryIterator final : public IDirectoryIterator {
    public:
        explicit NativeDirectoryIterator(const Path& path, bool recursive)
            : is_end_(false)
            , recursive_(recursive) {
            stack_.push_back(path);
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
        auto advance() -> void {
            while (!stack_.empty()) {
                auto const& path = stack_.back();
                stack_.pop_back();

                Path search_path = path + L"\\*";
                WIN32_FIND_DATAW find_data;
                HANDLE hFind = FindFirstFileW(search_path.c_str(), &find_data);

                if (hFind == INVALID_HANDLE_VALUE) continue;

                do {
                    Path name = find_data.cFileName;
                    if (name != L"." && name != L"..") {
                        Path full_path = path;
                        if (full_path.back() != L'\\' && full_path.back() != L'/') {
                            full_path += L'\\';
                        }
                        full_path += name;

                        bool is_dir = (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

                        if (is_dir && recursive_) {
                            stack_.push_back(full_path);
                        }

                        current_.path = full_path;
                        current_.is_directory = is_dir;
                        current_.size = (static_cast<uint64_t>(find_data.nFileSizeHigh) << 32) | find_data.nFileSizeLow;

                        FindClose(hFind);
                        return;
                    }
                } while (FindNextFileW(hFind, &find_data));

                FindClose(hFind);
            }

            is_end_ = true;
        }

        mi::Vector<Path> stack_;
        DirEntry current_;
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
        NativeFilesystem(PathView root)
            : root_path_{ root } {
        }

        [[nodiscard]] auto open_file(PathView path, std::ios_base::openmode mode) -> Shared<IFile> override {
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
                to_native_path(std::wstring(path)).c_str(),
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

        auto create_directory(PathView path) -> bool override {
            auto native_path = to_native_path(path);
            if (!CreateDirectoryW(native_path.c_str(), nullptr)) {
                DWORD error = GetLastError();
                if (error != ERROR_ALREADY_EXISTS) {
                    return false;
                }
            }
            return true;
        }

        auto remove(PathView path) -> bool override {
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

        auto exists(PathView path) -> bool override {
            auto native_path = to_native_path(path);
            return GetFileAttributesW(native_path.c_str()) != INVALID_FILE_ATTRIBUTES;
        }

        auto is_directory(PathView path) -> bool override {
            auto native_path = to_native_path(path);
            DWORD attrs = GetFileAttributesW(native_path.c_str());
            if (attrs == INVALID_FILE_ATTRIBUTES) {
                return false;
            }
            return (attrs & FILE_ATTRIBUTE_DIRECTORY);
        }

        auto is_file(PathView path) -> bool override {
            auto native_path = to_native_path(path);
            DWORD attrs = GetFileAttributesW(native_path.c_str());
            if (attrs == INVALID_FILE_ATTRIBUTES) {
                return false;
            }
            return !(attrs & FILE_ATTRIBUTE_DIRECTORY);
        }

        [[nodiscard]] auto walk(PathView path, bool recursive = false) -> Shared<IDirectoryIterator> override {
            auto native_path = to_native_path(path);
            return std::make_shared<NativeDirectoryIterator>(native_path, recursive);
        }
    private:
        auto to_native_path(PathView path) -> Path {
            auto native = root_path_;
            if (!native.empty() && native.back() != '\\' && native.back() != '/') {
                native += '\\';
            }
            Path normalized = Path(path);
            std::replace(normalized.begin(), normalized.end(), L'/', L'\\');
            if (!normalized.empty() && normalized[0] == '\\') {
                normalized = normalized.substr(1);
            }
            native += normalized;
            return native;
        }

        Path root_path_;
    };

    auto create_native_filesystem(PathView root_path) -> Shared<IFilesystem> {
        return std::make_shared<NativeFilesystem>(root_path);
    }
}