#include "filesystem.hpp"

#include "allocator.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <ShlObj.h>
#include <windows.h>

namespace edge::filesystem {
static usize utf8_to_wide(const StringView<char8_t> sv, wchar_t *out,
                          const usize out_cap) {
  if (sv.empty() || out_cap == 0) {
    return 0;
  }
  const int len = MultiByteToWideChar(
      CP_UTF8, 0, reinterpret_cast<const char *>(sv.data()),
      static_cast<int>(sv.size()), out, static_cast<int>(out_cap - 1));
  if (len <= 0) {
    return 0;
  }
  out[len] = L'\0';
  return static_cast<usize>(len);
}

static usize wide_to_utf8(const wchar_t *wide, char8_t *out,
                          const usize out_cap) {
  if (!wide || !out || out_cap == 0) {
    return 0;
  }
  const int len =
      WideCharToMultiByte(CP_UTF8, 0, wide, -1, reinterpret_cast<char *>(out),
                          static_cast<int>(out_cap), nullptr, nullptr);

  if (len <= 0) {
    return 0;
  }

  for (usize i = 0; i < static_cast<usize>(len); i++) {
    if (is_separator(out[i])) {
      out[i] = u8'/';
    }
  }

  // len includes the null terminator.
  return static_cast<usize>(len - 1);
}

String get_system_cwd(const NotNull<const Allocator *> alloc) {
  wchar_t wchar_buffer[MAX_PATH];
  DWORD wchar_length = GetCurrentDirectoryW(MAX_PATH, wchar_buffer);

  char8_t utf8_buffer[1024];
  const usize utf_len = wide_to_utf8(wchar_buffer, utf8_buffer, 1024);

  return String{alloc, utf8_buffer, utf_len};
}

String get_system_temp_path(const NotNull<const Allocator *> alloc) {
  wchar_t wchar_buffer[MAX_PATH];
  DWORD wchar_length = GetTempPathW(MAX_PATH, wchar_buffer);

  char8_t utf8_buffer[1024];
  const usize utf_len = wide_to_utf8(wchar_buffer, utf8_buffer, 1024);

  return String{alloc, utf8_buffer, utf_len};
}

String get_system_cached_path(const NotNull<const Allocator *> alloc) {
  wchar_t wchar_buffer[MAX_PATH];
  HRESULT wchar_length =
      SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, wchar_buffer);

  char8_t utf8_buffer[1024];
  const usize utf_len = wide_to_utf8(wchar_buffer, utf8_buffer, 1024);

  return String{alloc, utf8_buffer, utf_len};
}

class NativeFile final : public IFile {
public:
  bool open(const StringView<char8_t> path,
            const AccessModeFlags flags) override {
    if (is_open()) {
      return false;
    }

    wchar_t wpath[1024];
    if (!utf8_to_wide(path, wpath, 1024)) {
      return false;
    }

    DWORD desired_access = 0;
    DWORD creation = OPEN_EXISTING;
    constexpr DWORD share = FILE_SHARE_READ;

    if (flags.has(AccessMode::Read)) {
      desired_access |= GENERIC_READ;
    }
    if (flags.has(AccessMode::Write) || flags.has(AccessMode::Append)) {
      desired_access |= GENERIC_WRITE;
    }

    if (flags.has(AccessMode::Create)) {
      if (flags.has(AccessMode::Truncate)) {
        creation = CREATE_ALWAYS;
      } else {
        creation = OPEN_ALWAYS;
      }
    } else if (flags.has(AccessMode::Truncate)) {
      creation = TRUNCATE_EXISTING;
    }

    m_handle = CreateFileW(wpath, desired_access, share, nullptr, creation,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (!m_handle) {
      return false;
    }

    if (flags.has(AccessMode::Append)) {
      SetFilePointer(m_handle, 0, nullptr, FILE_END);
    }

    return true;
  }

  void close() override {
    if (is_open()) {
      CloseHandle(m_handle);
      m_handle = INVALID_HANDLE_VALUE;
    }
  }

  bool is_open() const override { return m_handle != INVALID_HANDLE_VALUE; }

  usize seek(const isize offset, const StreamOrigin origin) override {
    if (!is_open()) {
      return false;
    }

    DWORD method = FILE_BEGIN;
    switch (origin) {
    case StreamOrigin::Current:
      method = FILE_CURRENT;
      break;
    case StreamOrigin::End:
      method = FILE_END;
      break;
    default:
      break;
    }

    LARGE_INTEGER li;
    li.QuadPart = offset;
    li.LowPart = SetFilePointer(m_handle, li.LowPart, &li.HighPart, method);
    return li.LowPart != INVALID_SET_FILE_POINTER || GetLastError() == NO_ERROR;
  }

  usize tell() override {
    if (!is_open()) {
      return 0;
    }

    LARGE_INTEGER zero;
    zero.QuadPart = 0;
    LARGE_INTEGER pos;
    if (!SetFilePointerEx(m_handle, zero, &pos, FILE_CURRENT)) {
      return 0;
    }
    return pos.QuadPart;
  }

  usize read(void *buffer_out, const usize element_size,
             const usize element_count) const override {
    if (!is_open() || !buffer_out || element_size == 0 || element_count == 0) {
      return 0;
    }

    DWORD bytes_read = 0;
    if (!ReadFile(m_handle, buffer_out,
                  static_cast<DWORD>(element_count * element_size), &bytes_read,
                  nullptr)) {
      return 0;
    }
    return bytes_read;
  }

  usize write(const void *buffer_in, const usize element_size,
              const usize element_count) const override {
    if (!is_open() || !buffer_in || element_size == 0 || element_count == 0) {
      return 0;
    }

    DWORD bytes_written = 0;
    if (!WriteFile(m_handle, buffer_in,
                   static_cast<DWORD>(element_size * element_count),
                   &bytes_written, nullptr)) {
      return 0;
    }
    return bytes_written;
  }

  bool flush() override {
    if (!is_open()) {
      return false;
    }
    return FlushFileBuffers(m_handle) != 0;
  }

private:
  HANDLE m_handle = INVALID_HANDLE_VALUE;
};

bool file_exists(const StringView<char8_t> path) {
  wchar_t wpath[1024];
  if (!utf8_to_wide(path, wpath, 1024)) {
    return false;
  }
  const DWORD attr = GetFileAttributesW(wpath);
  return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

bool directory_exists(const StringView<char8_t> path) {
  wchar_t wpath[1024];
  if (!utf8_to_wide(path, wpath, 1024)) {
    return false;
  }
  const DWORD attr = GetFileAttributesW(wpath);
  return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
}

i64 file_size(const StringView<char8_t> path) {
  wchar_t wpath[1024];
  if (!utf8_to_wide(path, wpath, 1024)) {
    return -1;
  }
  WIN32_FILE_ATTRIBUTE_DATA data;
  if (!GetFileAttributesExW(wpath, GetFileExInfoStandard, &data)) {
    return -1;
  }
  LARGE_INTEGER size;
  size.HighPart = data.nFileSizeHigh;
  size.LowPart = data.nFileSizeLow;
  return size.QuadPart;
}

bool create_directory(const StringView<char8_t> path) {
  wchar_t wpath[1024];
  if (!utf8_to_wide(path, wpath, 1024)) {
    return false;
  }
  return CreateDirectoryW(wpath, nullptr) != 0 ||
         GetLastError() == ERROR_ALREADY_EXISTS;
}

bool create_directories(const StringView<char8_t> path) {
  if (path.empty()) {
    return false;
  }

  const auto scratch_alloc = Allocator::create_default();

  // Walk the path from root to leaf, creating each component.
  Path buf;
  if (!buf.from_utf8(&scratch_alloc, path.data(), path.size())) {
    return false;
  }

  // Normalize separators to backslash for Windows.
  for (usize i = 0; i < buf.m_length; ++i) {
    if (buf.m_data[i] == u8'/') {
      buf.m_data[i] = u8'\\';
    }
  }

  usize start = 0;
  // Skip drive letter or UNC prefix.
  if (buf.m_length >= 3 && is_alpha(buf.m_data[0]) && buf.m_data[1] == u8':' &&
      buf.m_data[2] == u8'\\') {
    start = 3;
  }

  for (usize i = start; i <= buf.m_length; ++i) {
    if (i == buf.m_length || buf.m_data[i] == u8'\\') {
      const char8_t saved = buf.m_data[i];
      buf.m_data[i] = 0;
      if (!create_directory(StringView{buf.m_data, i})) {
        buf.m_data[i] = saved;
        buf.destroy(&scratch_alloc);
        return false;
      }
      buf.m_data[i] = saved;
    }
  }

  buf.destroy(&scratch_alloc);
  return true;
}

bool remove_file(const StringView<char8_t> path) {
  wchar_t wpath[1024];
  if (!utf8_to_wide(path, wpath, 1024)) {
    return false;
  }
  return DeleteFileW(wpath) != 0;
}

bool remove_directory(const StringView<char8_t> path) {
  wchar_t wpath[1024];
  if (!utf8_to_wide(path, wpath, 1024)) {
    return false;
  }
  return RemoveDirectoryW(wpath) != 0;
}

bool rename_path(const StringView<char8_t> from, const StringView<char8_t> to) {
  wchar_t wfrom[1024], wto[1024];
  if (!utf8_to_wide(from, wfrom, 1024) || !utf8_to_wide(to, wto, 1024)) {
    return false;
  }
  return MoveFileW(wfrom, wto) != 0;
}

bool copy_file(const StringView<char8_t> from, const StringView<char8_t> to) {
  wchar_t wfrom[1024], wto[1024];
  if (!utf8_to_wide(from, wfrom, 1024) || !utf8_to_wide(to, wto, 1024)) {
    return false;
  }
  return CopyFileW(wfrom, wto, FALSE) != 0;
}

} // namespace edge::filesystem