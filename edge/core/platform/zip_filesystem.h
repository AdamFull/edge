#pragma once

#include "platform.h"

typedef struct zip zip_t;
typedef struct zip_file zip_file_t;
typedef struct zip_stat zip_stat_t;

namespace edge::platform {
    class ZipDirectoryIterator final : public IPlatformDirectoryIterator {
    public:
        explicit ZipDirectoryIterator(zip_t* zip, std::u8string_view path, bool recursive);
        ~ZipDirectoryIterator() override = default;

        [[nodiscard]] auto end() const noexcept -> bool override;
        auto next() -> void override;
        auto value() const noexcept -> const DirEntry & override;
    private:
        bool matches_path(std::u8string_view entry_name);

        zip_t* zip_handle_{ nullptr };

        mi::U8String root_{};
        bool recursive_{ false };

        int32_t current_index_{ 0 };
        int32_t entry_count_{ -1 };

        DirEntry entry_{};

        std::mutex mutex_{};
    };

    class ZipFile final : public IPlatformFile {
    public:
        explicit ZipFile(zip_t* zip, zip_file_t* file_handle, zip_stat_t* stat);
        ~ZipFile() override;

        [[nodiscard]] auto is_open() -> bool override;
        auto close() -> void override;
        [[nodiscard]] auto size() -> uint64_t override;

        auto seek(uint64_t offset, std::ios_base::seekdir origin) -> int64_t override;
        [[nodiscard]] auto tell() -> int64_t override;

        auto read(void* buffer, uint64_t size) -> int64_t override;
        auto write(const void* buffer, uint64_t size) -> int64_t override;
    private:
        zip_t* zip_handle_{ nullptr };
        zip_file_t* file_handle_{ nullptr };
        zip_stat_t* file_stat_{ nullptr };

        std::mutex mutex_;
    };

    class ZipFilesystem final : public IPlatformFilesystem {
    public:
        ZipFilesystem(std::u8string_view root, std::u8string_view password = {});
        ~ZipFilesystem() override;

        [[nodiscard]] auto open_file(std::u8string_view path, std::ios_base::openmode mode) -> Shared<IPlatformFile> override;

        auto create_directory(std::u8string_view path) -> bool override;
        auto remove(std::u8string_view path) -> bool override;
        auto exists(std::u8string_view path) -> bool override;

        auto is_directory(std::u8string_view path) -> bool override;
        auto is_file(std::u8string_view path) -> bool override;

        [[nodiscard]] auto walk(std::u8string_view path, bool recursive = false) -> Shared<IPlatformDirectoryIterator> override;
    private:
        zip_t* zip_handle_{ nullptr };

        std::mutex mutex_{};
    };
}