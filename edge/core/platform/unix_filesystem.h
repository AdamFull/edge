#pragma once

#include "platform.h"

typedef struct DIR DIR;

namespace edge::platform {
    class NativeDirectoryIterator final : public IPlatformDirectoryIterator {
    public:
        explicit NativeDirectoryIterator(std::u8string_view path, bool recursive);
        ~NativeDirectoryIterator() override;

        [[nodiscard]] auto end() const noexcept -> bool override;
        auto next() -> void override;
        [[nodiscard]] auto value() const noexcept -> const DirEntry& override;
    private:
        struct DirectoryState {
            DIR* dir_handle = nullptr;
            mi::U8String current_dir;    // Full path
            mi::U8String relative_path;  // Relative path from base
        };

        auto open_directory(std::u8string_view dir_path, std::u8string_view relative_path) -> bool;
        auto get_file_info(std::u8string_view full_path, bool& is_dir, uint64_t& size) -> bool;
        auto advance_to_valid_entry() -> bool;

        mi::U8String base_path_;
        bool recursive_;
        mi::Vector<DirectoryState> dir_stack_;
        DirEntry current_entry_;
        bool at_end_ = false;
    };

    class NativeFile final : public IPlatformFile {
    public:
        explicit NativeFile(FILE* file);
        ~NativeFile() override;

        [[nodiscard]] auto is_open() -> bool override;
        auto close() -> void override;
        [[nodiscard]] auto size() -> uint64_t override;

        auto seek(uint64_t offset, std::ios_base::seekdir origin) -> int64_t override;

        [[nodiscard]] auto tell() -> int64_t override;

        auto read(void* buffer, uint64_t size) -> int64_t override;
        auto write(const void* buffer, uint64_t size) -> int64_t override;
    private:
        FILE* file_;
        uint64_t size_;
    };

    class NativeFilesystem : public IPlatformFilesystem {
    public:
        explicit NativeFilesystem(std::u8string_view root);

        [[nodiscard]] auto exists(std::u8string_view path) -> bool override;

        [[nodiscard]] auto is_file(std::u8string_view path) -> bool override;
        [[nodiscard]] auto is_directory(std::u8string_view path) -> bool override;

        [[nodiscard]] auto open_file(std::u8string_view path, std::ios_base::openmode mode) -> Shared<IPlatformFile> override;

        auto create_directory(std::u8string_view path) -> bool override;
        auto remove(std::u8string_view path) -> bool override;
        [[nodiscard]] auto walk(std::u8string_view path, bool recursive = false) -> Shared<IPlatformDirectoryIterator> override;
    private:
        auto to_native_path(std::u8string_view vfs_path) -> mi::U8String;

        mi::U8String root_path_;
    };
}