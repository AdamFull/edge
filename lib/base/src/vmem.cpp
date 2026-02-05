#include "vmem.hpp"

#if EDGE_HAS_WINDOWS_API
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace edge {
usize vmem_page_size() {
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  return static_cast<usize>(si.dwPageSize);
}

usize vmem_allocation_granularity() {
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  return static_cast<usize>(si.dwAllocationGranularity);
}

bool vmem_reserve(void **out_base, usize reserve_bytes) {
  if (!out_base) {
    return false;
  }

  void *base = VirtualAlloc(nullptr, reserve_bytes, MEM_RESERVE, PAGE_NOACCESS);
  if (!base) {
    return false;
  }
  *out_base = base;
  return true;
}

bool vmem_release(void *base, usize reserve_bytes) {
  if (!base) {
    return false;
  }

  (void)reserve_bytes;
  return VirtualFree(base, 0, MEM_RELEASE) != 0;
}

bool vmem_commit(void *addr, usize size) {
  if (!addr) {
    return false;
  }

  return VirtualAlloc(addr, size, MEM_COMMIT, PAGE_READWRITE) != nullptr;
}

namespace detail {
inline DWORD translate_protection_flags(VMemProt p) {
  if (p == VMemProt::None) {
    return PAGE_NOACCESS;
  }

  u32 flags = static_cast<u32>(p);
  bool has_write = (flags & static_cast<u32>(VMemProt::Write)) != 0;
  bool has_exec = (flags & static_cast<u32>(VMemProt::Exec)) != 0;

  if (has_write) {
    if (has_exec) {
      return PAGE_EXECUTE_READWRITE;
    }
    return PAGE_READWRITE;
  }

  // No write
  if (has_exec) {
    return PAGE_EXECUTE_READ;
  }
  return PAGE_READONLY;
}
} // namespace detail

bool vmem_protect(void *addr, usize size, VMemProt prot) {
  if (!addr) {
    return false;
  }

  DWORD old;
  DWORD new_prot = detail::translate_protection_flags(prot);
  if (!VirtualProtect(addr, size, new_prot, &old)) {
    return false;
  }
  return true;
}
} // namespace edge
#elif EDGE_PLATFORM_POSIX
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

namespace edge {
usize vmem_page_size() {
  long page_size = sysconf(_SC_PAGESIZE);
  if (page_size <= 0) {
    return 4096;
  }
  return static_cast<usize>(page_size);
}

usize vmem_allocation_granularity() { return vmem_page_size(); }

bool vmem_reserve(void **out_base, usize reserve_bytes) {
  if (!out_base) {
    return false;
  }

  void *base = mmap(nullptr, reserve_bytes, PROT_NONE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (base == MAP_FAILED) {
    return false;
  }
  *out_base = base;
  return true;
}

bool vmem_release(void *base, usize reserve_bytes) {
  if (!base) {
    return false;
  }
  return munmap(base, reserve_bytes) == 0;
}

bool vmem_commit(void *addr, usize size) {
  if (!addr) {
    return false;
  }

  i32 prot = PROT_READ | PROT_WRITE;
  if (mprotect(addr, size, prot) != 0) {
    return false;
  }

  return true;
}

namespace detail {
inline i32 translate_protection_flags(VMemProt p) {
  if (p == VMemProt::None) {
    return PROT_NONE;
  }

  i32 flags = 0;
  if ((static_cast<u32>(p) & static_cast<u32>(VMemProt::Read)) != 0) {
    flags |= PROT_READ;
  }
  if ((static_cast<u32>(p) & static_cast<u32>(VMemProt::Write)) != 0) {
    flags |= PROT_WRITE;
  }
  if ((static_cast<u32>(p) & static_cast<u32>(VMemProt::Exec)) != 0) {
    flags |= PROT_EXEC;
  }
  return flags;
}
} // namespace detail

bool vmem_protect(void *addr, usize size, VMemProt prot) {
  if (!addr) {
    return false;
  }

  i32 flags = detail::translate_protection_flags(prot);
  if (mprotect(addr, size, flags) != 0) {
    return false;
  }
  return true;
}
} // namespace edge
#else
#error "Unsupported platform for virtual memory operations"
#endif
