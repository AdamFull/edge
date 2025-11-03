#pragma once

#include "platform.h"

namespace edge::platform {
    class NativeDirectoryIterator final : public IPlatformDirectoryIterator {
    public:
        explicit NativeDirectoryIterator(std::u8string_view path, bool recursive);
        ~NativeDirectoryIterator();

        [[nodiscard]] auto end() const noexcept -> bool override;
        auto next() -> void override;
        auto value() const noexcept -> const DirEntry& override;
    private:
        struct DirectoryState {
            mi::WString current_dir;
            mi::U8String relative_path;
            HANDLE handle = INVALID_HANDLE_VALUE;
            WIN32_FIND_DATAW find_data{};
            bool first = true;
        };

        auto open_directory(std::u8string_view dir_path) -> bool;
        auto open_subdirectory(std::u8string_view relative_path, std::u8string_view name) -> bool;
        auto advance_to_valid_entry() -> bool;

        mi::U8String base_path_;
        bool recursive_;
        mi::Vector<DirectoryState> dir_stack_;
        DirEntry current_entry_;
        bool at_end_ = false;
    };

    class NativeFile final : public IPlatformFile {
    public:
        explicit NativeFile(HANDLE handle);
        ~NativeFile() override;

        [[nodiscard]] auto is_open() -> bool override;
        auto close() -> void override;
        [[nodiscard]] auto size() -> uint64_t override;

        auto seek(uint64_t offset, std::ios_base::seekdir origin) -> int64_t override;
        [[nodiscard]] auto tell() -> int64_t override;

        auto read(void* buffer, uint64_t size) -> int64_t override;
        auto write(const void* buffer, uint64_t size) -> int64_t override;
    private:
        HANDLE handle_;
        uint64_t size_;
    };

    class NativeFilesystem final : public IPlatformFilesystem {
    public:
        NativeFilesystem(std::u8string_view root);

        [[nodiscard]] auto open_file(std::u8string_view path, std::ios_base::openmode mode) -> Shared<IPlatformFile> override;

        auto create_directory(std::u8string_view path) -> bool override;
        auto remove(std::u8string_view path) -> bool override;
        auto exists(std::u8string_view path) -> bool override;

        auto is_directory(std::u8string_view path) -> bool override;
        auto is_file(std::u8string_view path) -> bool override;

        [[nodiscard]] auto walk(std::u8string_view path, bool recursive = false) -> Shared<IPlatformDirectoryIterator> override;
    private:
        auto to_native_path(std::u8string_view path) -> mi::WString;

        mi::U8String root_path_;
    };
}