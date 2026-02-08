#ifndef EDGE_FILESYSTEM_H
#define EDGE_FILESYSTEM_H

#include "array.hpp"
#include "enumerator.hpp"
#include "string_view.hpp"

namespace edge::filesystem {
enum class AccessMode : u32 {
  Read = 1u << 0,
  Write = 1u << 1,
  Append = 1u << 2,
  Create = 1u << 3,
  Truncate = 1u << 4,
};
using AccessModeFlags = Flags<AccessMode>;

enum class EntryFlag : u32 {
  File = 1u << 0,
  Directory = 1u << 1,
};
using EntryFlags = Flags<EntryFlag>;

enum struct StreamOrigin : u32 {
  Begin,
  Current,
  End,
};
} // namespace edge::filesystem

EDGE_ENUM_FLAGS(filesystem::AccessMode)
EDGE_ENUM_FLAGS(filesystem::EntryFlag)

namespace edge::filesystem {
using Path = String;

constexpr bool is_alpha(const char8_t c) {
  return (c >= u8'A' && c <= u8'Z') || (c >= u8'a' && c <= u8'z');
}

constexpr bool is_separator(const char8_t c) {
  return c == u8'\\' || c == u8'/';
}

constexpr usize find_last_separator(const StringView<char8_t> path) {
  for (usize i = path.size(); i > 0; --i) {
    if (is_separator(path[i - 1])) {
      return i - 1;
    }
  }
  return SIZE_MAX;
}

constexpr usize find_first_separator(const StringView<char8_t> path) {
  for (usize i = 0; i < path.size(); ++i) {
    if (is_separator(path[i])) {
      return i;
    }
  }
  return SIZE_MAX;
}

constexpr bool is_absolute(const StringView<char8_t> path) {
  if (path.empty()) {
    return false;
  }

  if (is_separator(path[0])) {
    return true;
  }

  if (path.size() >= 3 && is_alpha(path[0]) && path[1] == u8':' &&
      is_separator(path[2])) {
    return true;
  }

  return false;
}

constexpr StringView<char8_t> filename(StringView<char8_t> path) {
  if (path.empty()) {
    return path;
  }

  while (!path.empty() && is_separator(path.back())) {
    path.remove_suffix(1);
  }

  if (path.empty()) {
    return u8"/";
  }

  const auto pos = find_last_separator(path);
  if (pos == SIZE_MAX) {
    return path;
  }

  return path.substr(pos + 1);
}

constexpr StringView<char8_t> extension(const StringView<char8_t> path) {
  const StringView<char8_t> file_name = filename(path);
  if (file_name.empty() || file_name == StringView<char8_t>{u8"."} ||
      file_name == StringView<char8_t>{u8".."}) {
    return StringView<char8_t>{};
  }

  const usize pos = file_name.rfind(u8'.');
  if (pos == SIZE_MAX || pos == 0) {
    return StringView<char8_t>{};
  }

  return file_name.substr(pos);
}

constexpr StringView<char8_t> stem(const StringView<char8_t> path) {
  const StringView<char8_t> file_name = filename(path);
  if (file_name.empty() || file_name == StringView<char8_t>{u8"."} ||
      file_name == StringView<char8_t>{u8".."}) {
    return file_name;
  }

  const usize pos = file_name.rfind(u8'.');
  if (pos == SIZE_MAX || pos == 0) {
    return file_name;
  }

  return file_name.substr(0, pos);
}

constexpr StringView<char8_t> parent_path(StringView<char8_t> path) {
  if (path.empty()) {
    return path;
  }

  while (!path.empty() && is_separator(path.back())) {
    path.remove_suffix(1);
  }

  if (path.empty()) {
    return StringView<char8_t>{};
  }

  const auto pos = find_last_separator(path);
  if (pos == SIZE_MAX) {
    return StringView<char8_t>{};
  }

  if (pos == 0) {
    return path.substr(0, 1);
  }

  if (pos == 2 && path.size() >= 3 && path[1] == u8':') {
    return path.substr(0, 3);
  }

  return path.substr(0, pos);
}

inline Path append(const NotNull<const Allocator *> alloc,
                   const StringView<char8_t> base,
                   const StringView<char8_t> component,
                   const char8_t separator = u8'/') {
  Path result = {};

  if (base.empty()) {
    return result.from_utf8(alloc, component.data(), component.length())
               ? result
               : Path{};
  }

  if (component.empty()) {
    return result.from_utf8(alloc, base.data(), base.length()) ? result
                                                               : Path{};
  }

  if (!result.from_utf8(alloc, base.data(), base.length())) {
    return {};
  }

  if (!is_separator(result.back()) && !is_separator(component.front())) {
    result.append(alloc, separator);
  }
  result.append(alloc, component.data(), component.length());

  return result;
}

class IFile {
public:
  virtual ~IFile() = default;

  virtual bool open(StringView<char8_t> path, AccessModeFlags flags) = 0;
  virtual void close() = 0;

  [[nodiscard]] virtual bool is_open() const = 0;

  [[nodiscard]] virtual usize seek(isize offset, StreamOrigin origin) = 0;
  [[nodiscard]] virtual usize tell() = 0;
  virtual usize read(void *buffer_out, usize element_size,
                     usize element_count) const = 0;
  virtual usize write(const void *buffer_in, usize element_size,
                      usize element_count) const = 0;

  virtual bool flush() = 0;
};

class IFilesystem {
public:
  virtual ~IFilesystem() = default;

  virtual bool create(NotNull<const Allocator *> alloc) = 0;
  virtual void destroy(NotNull<const Allocator *> alloc) = 0;

  virtual bool create_directory(StringView<char8_t> path) = 0;
  virtual bool remove(StringView<char8_t> path) = 0;

  virtual EntryFlags get_entry_flags(StringView<char8_t> path) = 0;
};

struct MountPoint {
  String path;
  IFilesystem *filesystem;
};

class Filesystem {
public:
  static void set_instance(Filesystem *instance);
  static Filesystem *get_instance();

  bool create(NotNull<const Allocator *> alloc);
  void destroy(NotNull<const Allocator *> alloc);

  void mount(NotNull<const Allocator *> alloc, StringView<char8_t> mount_point, IFilesystem* filesystem);
  void unmount(NotNull<const Allocator *> alloc,
               StringView<char8_t> mount_point);

  bool exists(StringView<char8_t> path) const;
  bool is_file(StringView<char8_t> path) const;
  bool is_directory(StringView<char8_t> path) const;

  bool create_directory(StringView<char8_t> path) const;
  bool create_directories(StringView<char8_t> path);
  bool remove(StringView<char8_t> path) const;

private:
  static Filesystem *s_instance;

  String m_cwd_path;
  String m_temp_path;
  String m_cached_path;
  Array<MountPoint> m_mount_points;

  MountPoint resolve_path(StringView<char8_t> path) const;
};

bool file_exists(StringView<char8_t> path);
bool directory_exists(StringView<char8_t> path);
i64 file_size(StringView<char8_t> path);
bool create_directory(StringView<char8_t> path);
bool create_directories(StringView<char8_t> path);
bool remove_file(StringView<char8_t> path);
bool remove_directory(StringView<char8_t> path);
bool rename_path(StringView<char8_t> from, StringView<char8_t> to);
bool copy_file(StringView<char8_t> from, StringView<char8_t> to);

} // namespace edge::filesystem
#endif