#ifndef EDGE_FILESYSTEM_LIST_H
#define EDGE_FILESYSTEM_LIST_H

#include "string_view.hpp"

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
} // namespace edge::filesystem

#endif