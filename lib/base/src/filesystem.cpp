#include "filesystem.hpp"

#include <winternl.h>

namespace edge::filesystem {
Filesystem *Filesystem::s_instance = nullptr;

String get_system_cwd(NotNull<const Allocator*> alloc);
String get_system_temp_path(NotNull<const Allocator*> alloc);
String get_system_cached_path(NotNull<const Allocator*> alloc);

void Filesystem::set_instance(Filesystem *instance) { s_instance = instance; }
Filesystem *Filesystem::get_instance() { return s_instance; }

bool Filesystem::create(const NotNull<const Allocator *> alloc) {
  m_cwd_path = get_system_cwd(alloc);
  m_temp_path = get_system_temp_path(alloc);
  m_cached_path = get_system_cached_path(alloc);
  return true;
}

void Filesystem::destroy(const NotNull<const Allocator *> alloc) {
  for (auto &[path, filesystem] : m_mount_points) {
    path.destroy(alloc);
    filesystem->destroy(alloc);
    alloc->deallocate(filesystem);
  }

  m_cwd_path.destroy(alloc);
  m_temp_path.destroy(alloc);
  m_cached_path.destroy(alloc);
}

void Filesystem::mount(const NotNull<const Allocator *> alloc,
                       const StringView<char8_t> mount_point,
                       IFilesystem *filesystem) {
  m_mount_points.emplace_back(
      alloc, String{alloc, mount_point.data(), mount_point.length()},
      filesystem);
}

void Filesystem::unmount(const NotNull<const Allocator *> alloc,
                         const StringView<char8_t> mount_point) {
  for (usize i = 0; i < m_mount_points.size(); ++i) {
    if (const StringView<char8_t> mp = m_mount_points[i].path;
        mp == mount_point) {
      MountPoint mount_point = {};
      m_mount_points.remove(i, &mount_point);
      mount_point.path.destroy(alloc);
      mount_point.filesystem->destroy(alloc);
      alloc->deallocate(mount_point.filesystem);
      return;
    }
  }
}

bool Filesystem::exists(const StringView<char8_t> path) const {
  auto [relative_path, filesystem] = resolve_path(path);
  if (!filesystem) {
    return false;
  }
  return filesystem->get_entry_flags(relative_path).any();
}

bool Filesystem::is_file(const StringView<char8_t> path) const {
  auto [relative_path, filesystem] = resolve_path(path);
  if (!filesystem) {
    return false;
  }
  return filesystem->get_entry_flags(relative_path).has(EntryFlag::File);
}

bool Filesystem::is_directory(const StringView<char8_t> path) const {
  auto [relative_path, filesystem] = resolve_path(path);
  if (!filesystem) {
    return false;
  }
  return filesystem->get_entry_flags(relative_path).has(EntryFlag::Directory);
}

bool Filesystem::create_directory(const StringView<char8_t> path) const {
  auto [relative_path, filesystem] = resolve_path(path);
  if (!filesystem) {
    return false;
  }
  return filesystem->create_directory(relative_path);
}

bool Filesystem::create_directories(const StringView<char8_t> path) {
  auto [relative_path, filesystem] = resolve_path(path);
  if (!filesystem) {
    return false;
  }

  // TODO: NOT IMPLEMENTED

  return true;
}

bool Filesystem::remove(const StringView<char8_t> path) const {
  auto [relative_path, filesystem] = resolve_path(path);
  if (!filesystem) {
    return false;
  }
  return filesystem->remove(relative_path);
}

MountPoint Filesystem::resolve_path(const StringView<char8_t> path) const {
  return {};
}
} // namespace edge::filesystem