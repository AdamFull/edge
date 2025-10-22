#include "filesystem.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <locale>
#include <codecvt>

namespace edge::fs {
    auto get_system_cwd() -> Path {
        // TODO: filesystem
        return {};
    }

    auto get_system_temp_dir() -> Path {
        // TODO: filesystem
        return {};
    }

    auto get_system_cache_dir() -> Path {
        // TODO: filesystem
        return {};
    }

    class NativeDirectoryIterator final : public IDirectoryIterator {
    public:
        explicit NativeDirectoryIterator(const Path& path, bool recursive)
            : is_end_(false), recursive_(recursive) {
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

        [[nodiscard]] auto value() const noexcept -> const DirEntry & override {
            return current_;
        }
    private:
        auto advance() -> void {
            while (!stack_.empty()) {
                Path wpath = stack_.back();
                stack_.pop_back();

                std::string path = wstring_to_string(wpath.wstring());
                DIR* dir = opendir(path.c_str());

                if (!dir) continue;

                struct dirent* entry;
                while ((entry = readdir(dir)) != nullptr) {
                    std::string name = entry->d_name;
                    if (name != "." && name != "..") {
                        std::string full_path = path;
                        if (full_path.back() != '/') {
                            full_path += '/';
                        }
                        full_path += name;

                        struct stat st;
                        bool is_dir = false;
                        uint64_t size = 0;

                        if (stat(full_path.c_str(), &st) == 0) {
                            is_dir = S_ISDIR(st.st_mode);
                            size = st.st_size;
                        }

                        if (is_dir && recursive_) {
                            stack_.push_back(Path(string_to_wstring(full_path)));
                        }

                        current_.name = Path(string_to_wstring(full_path));
                        current_.is_directory = is_dir;
                        current_.size = size;

                        closedir(dir);
                        return;
                    }
                }

                closedir(dir);
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
        explicit NativeFile(FILE* file) : file_(file), size_(0) {
            if (file_) {
                fseeko(file_, 0, SEEK_END);
                size_ = ftello(file_);
                fseeko(file_, 0, SEEK_SET);
            }
        }

        ~NativeFile() override {
            close();
        }

        [[nodiscard]] auto is_open() -> bool override {
            return file_ != nullptr;
        }

        auto close() -> void override {
            if (file_) {
                fclose(file_);
                file_ = nullptr;
            }
        }

        [[nodiscard]] auto size() -> uint64_t override {
            return size_;
        }

        auto seek(uint64_t offset, std::ios_base::seekdir origin) -> int64_t override {
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

        [[nodiscard]] auto tell() -> int64_t override {
            if (!is_open()) return -1;
            return ftello(file_);
        }

        auto read(void* buffer, uint64_t size) -> int64_t override {
            if (!is_open()) return -1;
            return fread(buffer, 1, size, file_);
        }

        auto write(const void* buffer, uint64_t size) -> int64_t override {
            if (!is_open()) return -1;
            return fwrite(buffer, 1, size, file_);
        }
	private:
		FILE* file_;
		uint64_t size_;
	};

    class NativeFilesystem : public IFilesystem {
    public:
    private:
        auto to_native_path(const Path& vfs_path) -> std::string {
            Path native = root_path_ / vfs_path;
            return wstring_to_string(native.string());
        }

        Path root_path_;
    };
}