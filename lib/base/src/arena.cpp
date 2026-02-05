#include "arena.hpp"
#include "math.hpp"

namespace edge {
namespace detail {
static void *ptr_add(void *p, usize bytes) {
  return reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(p) + bytes);
}

static bool arena_ensure_committed_locked(Arena *arena, usize required_bytes) {
  if (required_bytes <= arena->m_committed) {
    return true;
  }

  usize max_size = arena->m_reserved;
  if (required_bytes > max_size) {
    return false;
  }

  usize need = required_bytes - arena->m_committed;
  usize commit_size = align_up(need, ARENA_COMMIT_CHUNK_SIZE);
  if (arena->m_committed + commit_size > max_size) {
    commit_size = max_size - arena->m_committed;
  }

  void *commit_addr = ptr_add(arena->m_base, arena->m_committed);

  if (!vmem_commit(commit_addr, commit_size)) {
    return false;
  }

  arena->m_committed += commit_size;
  return true;
}
} // namespace detail

bool Arena::create(usize size) {
  if (size == 0) {
    size = ARENA_MAX_SIZE;
  }

  usize page_size = vmem_page_size();
  size = align_up(size, page_size);

  void *base = nullptr;
  if (!vmem_reserve(&base, size)) {
    return false;
  }

  m_base = base;
  m_reserved = size;
  m_committed = 0;
  m_offset = 0;
  m_page_size = page_size;

  return true;
}

void Arena::destroy() {
  if (m_base) {
    vmem_release(m_base, m_reserved);
    m_base = nullptr;
  }
}

bool Arena::protect(void *addr, usize size, VMemProt prot) {
  if (!m_base) {
    return false;
  }

  uintptr_t base = reinterpret_cast<uintptr_t>(m_base);
  uintptr_t astart = reinterpret_cast<uintptr_t>(addr);
  if (astart < base) {
    return false;
  }
  if (astart + size > base + m_reserved) {
    return false;
  }

  uintptr_t page_mask = ~static_cast<uintptr_t>(m_page_size - 1);
  uintptr_t page_addr = astart & page_mask;
  usize page_off = astart - page_addr;
  usize total = align_up(size + page_off, m_page_size);
  return vmem_protect(reinterpret_cast<void *>(page_addr), total, prot);
}

void *Arena::alloc_ex(usize size, usize alignment) {
  if (!m_base || size == 0) {
    return nullptr;
  }

  if (alignment == 0) {
    alignment = alignof(max_align_t);
  }

  if ((alignment & (alignment - 1)) != 0) {
    return nullptr;
  }

  // TODO: Update after threads port
  // std::lock_guard<std::recursive_mutex> lock(arena->m_mutex);

  usize offset = m_offset;
  usize aligned_offset = align_up(offset, alignment);

  if (aligned_offset > SIZE_MAX - size) {
    return nullptr;
  }

  usize max_size = m_reserved;
  usize new_offset = aligned_offset + size;
  if (new_offset > max_size) {
    return nullptr;
  }

  if (!detail::arena_ensure_committed_locked(this, new_offset)) {
    return nullptr;
  }

  void *result = detail::ptr_add(m_base, aligned_offset);
  m_offset = new_offset;

  return result;
}

void Arena::reset(bool zero_memory) {
  if (zero_memory && m_committed > 0) {
    memset(m_base, 0, m_committed);
  }
  m_offset = 0;
}
} // namespace edge