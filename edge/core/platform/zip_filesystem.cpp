#include "zip_filesystem.h"

#include <zip.h>

namespace edge::platform {
    inline auto get_zip_compression_method_name(zip_uint16_t comp_method) -> const char* {
        switch (comp_method) {
        case ZIP_CM_DEFAULT:		return "default";
        case ZIP_CM_STORE:			return "store";
        case ZIP_CM_SHRINK:			return "shrink";
        case ZIP_CM_REDUCE_1:		return "reduce_1";
        case ZIP_CM_REDUCE_2:		return "reduce_2";
        case ZIP_CM_REDUCE_3:		return "reduce_3";
        case ZIP_CM_REDUCE_4:		return "reduce_4";
        case ZIP_CM_IMPLODE:		return "implode";
        case ZIP_CM_DEFLATE:		return "deflate";
        case ZIP_CM_DEFLATE64:		return "deflate64";
        case ZIP_CM_PKWARE_IMPLODE:	return "pkware";
        case ZIP_CM_BZIP2:			return "bzip2";
        case ZIP_CM_LZMA:			return "lzma";
        case ZIP_CM_TERSE:			return "terse";
        case ZIP_CM_LZ77:			return "lz77";
        case ZIP_CM_LZMA2:			return "lzma2";
        case ZIP_CM_ZSTD:			return "zstd";
        case ZIP_CM_XZ:				return "xz";
        case ZIP_CM_JPEG:			return "jpeg";
        case ZIP_CM_WAVPACK:		return "wavpach";
        case ZIP_CM_PPMD:			return "ppmd";
        default:					return "unknown";
        }
    }

    inline auto get_zip_encryption_method_name(zip_uint16_t encryption_method) -> const char* {
        switch (encryption_method) {
        case ZIP_EM_NONE:			return "none";
        case ZIP_EM_TRAD_PKWARE:	return "pkware";
        case ZIP_EM_AES_128:		return "aes128";
        case ZIP_EM_AES_192:		return "aes192";
        case ZIP_EM_AES_256:		return "aes256";
        default:					return "unknown";
        }
    }

    ZipDirectoryIterator::ZipDirectoryIterator(zip_t* zip, std::u8string_view path, bool recursive)
        : zip_handle_{ zip }
        , root_{ path }
        , recursive_{ recursive } {

        if (!root_.empty() && root_.back() != '/') {
            root_ += '/';
        }

        entry_count_ = static_cast<int32_t>(zip_get_num_entries(zip_handle_, 0));
        next();
    }

    auto ZipDirectoryIterator::end() const noexcept -> bool {
        return current_index_ >= entry_count_;
    }

    auto ZipDirectoryIterator::next() -> void {
        // TODO: Improve threading
        std::lock_guard<std::mutex> lock(mutex_);
        ++current_index_;

        while (current_index_ < entry_count_) {
            zip_stat_t st;
            if (zip_stat_index(zip_handle_, current_index_, 0, &st) == 0 && matches_path(reinterpret_cast<const char8_t*>(st.name)))
            {
                entry_.path = reinterpret_cast<const char8_t*>(st.name);
                entry_.is_directory = (st.valid & ZIP_STAT_NAME) && (st.name[strlen(st.name) - 1] == '/');
                break;
            }
            ++current_index_;
        }
    }

    auto ZipDirectoryIterator::value() const noexcept -> const DirEntry& {
        return entry_;
    }

    bool ZipDirectoryIterator::matches_path(std::u8string_view entry_name) {
        if (!entry_name.starts_with(root_)) {
            return false;
        }

        if (recursive_) {
            return true;
        }

        // Non-recursive: exclude entries deeper than immediate children
        auto subpath = entry_name.substr(root_.size());
        return subpath.find('/') == std::u8string_view::npos || subpath.find('/') == subpath.size() - 1;
    }


    ZipFile::ZipFile(zip_t* zip, zip_file_t* file_handle, zip_stat_t* stat)
        : zip_handle_{ zip }
        , file_handle_{ file_handle }
        , file_stat_{ stat } {

    }

    ZipFile::~ZipFile() {
        if (file_stat_) {
            delete file_stat_;
            file_stat_ = nullptr;
        }

        close();
    }

    auto ZipFile::is_open() -> bool {
        return (file_handle_);
    }

    auto ZipFile::close() -> void {
        std::lock_guard<std::mutex> lock(mutex_);
        if (file_handle_) {
            zip_fclose(file_handle_);
            file_handle_ = nullptr;
        }
    }

    auto ZipFile::size() -> uint64_t {
        return file_stat_ ? file_stat_->size : 0ull;
    }

    auto ZipFile::seek(uint64_t offset, std::ios_base::seekdir origin) -> int64_t {
        std::lock_guard<std::mutex> lock(mutex_);
        if (zip_fseek(file_handle_, static_cast<long>(offset), static_cast<int>(origin)) != 0) {
            return -1;
        }
        return zip_ftell(file_handle_);
    }

    auto ZipFile::tell() -> int64_t {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!file_handle_) {
            return -1;
        }

        return zip_ftell(file_handle_);
    }

    auto ZipFile::read(void* buffer, uint64_t size) -> int64_t {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!file_handle_) {
            return -1;
        }

        return zip_fread(file_handle_, buffer, size);
    }

    auto ZipFile::write(const void* buffer, uint64_t size) -> int64_t {
        assert(false && "NOT IMPLEMENTED");
        return -1;
    }

#define EDGE_LOGGER_SCOPE "platform::ZipFilesystem"

    ZipFilesystem::ZipFilesystem(std::u8string_view root, std::u8string_view password) {
        int32_t error_code = 0;
        zip_handle_ = zip_open(reinterpret_cast<const char*>(root.data()), ZIP_RDONLY, &error_code);

        if (!zip_handle_) {
            zip_error_t err{};
            zip_error_strerror(&err);

            EDGE_SLOGE("Failed to open archive: {}.", err.str);
            return;
        }

        if (!password.empty()) {
            zip_set_default_password(zip_handle_, reinterpret_cast<const char*>(password.data()));
        }
    }

    ZipFilesystem::~ZipFilesystem() {
        if (zip_handle_) {
            zip_close(zip_handle_);
        }
    }

    auto ZipFilesystem::open_file(std::u8string_view path, std::ios_base::openmode mode) -> Shared<IPlatformFile> {
        constexpr const zip_flags_t flags{ ZIP_FL_ENC_GUESS | ZIP_FL_NOCASE };

        auto file_index = zip_name_locate(zip_handle_, reinterpret_cast<const char*>(path.data()), flags);
        if (file_index < 0) {
            EDGE_SLOGE("Zip error: {}. File \"{}\"", zip_strerror(zip_handle_), reinterpret_cast<const char*>(path.data()));
            return nullptr;
        }

        zip_stat_t* file_stat = new zip_stat_t;
        if (zip_stat_index(zip_handle_, file_index, flags, file_stat) != 0) {
            EDGE_SLOGE("Failed to read \"{}\" file metadata.", reinterpret_cast<const char*>(path.data()));
            return nullptr;
        }

        zip_file_t* file_handle = zip_fopen_index(zip_handle_, file_index, 0);

        auto* zip_err = zip_get_error(zip_handle_);
        if (zip_err && zip_err->zip_err != 0) {
            char buffer[256];
            int buffer_offset = zip_error_to_str(buffer, sizeof(buffer), zip_err->zip_err, errno);

            buffer_offset = snprintf(buffer + buffer_offset, sizeof(buffer) - buffer_offset,
                "\nName: %s\nSize: %llu"
                "\nCompressed size: %llu\nModification time: %s"
                "\nCRC: %lu"
                "\nCompression method: %s"
                "\nEncryption method: %s",
                file_stat->name, file_stat->size,
                file_stat->comp_size, ctime(&file_stat->mtime),
                file_stat->crc,
                get_zip_compression_method_name(file_stat->comp_method),
                get_zip_encryption_method_name(file_stat->encryption_method)
            );

            EDGE_SLOGE("Failed to open file \"{}\". Error: {}", reinterpret_cast<const char*>(path.data()), buffer);
            return nullptr;
        }

        return std::make_unique<ZipFile>(zip_handle_, file_handle, file_stat);
    }

    auto ZipFilesystem::create_directory(std::u8string_view path) -> bool {
        assert(false && "NT IMPLEMENTED IN READ ONLY FS");
        return false;
    }

    auto ZipFilesystem::remove(std::u8string_view path) -> bool {
        assert(false && "NT IMPLEMENTED IN READ ONLY FS");
        return false;
    }

    auto ZipFilesystem::exists(std::u8string_view path) -> bool {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!zip_handle_) {
            return false;
        }

        return zip_name_locate(zip_handle_, reinterpret_cast<const char*>(path.data()), ZIP_FL_ENC_GUESS | ZIP_FL_NOCASE) >= 0;
    }

    auto ZipFilesystem::is_directory(std::u8string_view path) -> bool {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!zip_handle_) {
            return false;
        }

        zip_stat_t st;
        if (zip_stat(zip_handle_, reinterpret_cast<const char*>(path.data()), ZIP_FL_ENC_GUESS | ZIP_FL_NOCASE, &st) == 0) {
            return (st.valid & ZIP_STAT_NAME) && (st.name[strlen(st.name) - 1] == '/');
        }

        return false;
    }

    auto ZipFilesystem::is_file(std::u8string_view path) -> bool {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!zip_handle_) {
            return false;
        }

        zip_stat_t st;
        if (zip_stat(zip_handle_, reinterpret_cast<const char*>(path.data()), ZIP_FL_ENC_GUESS | ZIP_FL_NOCASE, &st) == 0) {
            return (st.valid & ZIP_STAT_NAME) && (st.name[strlen(st.name) - 1] != '/');
        }

        return false;
    }

    auto ZipFilesystem::walk(std::u8string_view path, bool recursive) -> Shared<IPlatformDirectoryIterator> {
        return std::make_unique<ZipDirectoryIterator>(zip_handle_, path, recursive);
    }

#undef EDGE_LOGGER_SCOPE
}