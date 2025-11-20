#include "edge_vmem.h"

#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>

size_t edge_vmem_page_size(void) {
	long page_size = sysconf(_SC_PAGESIZE);
	if (page_size <= 0) {
		return 4096;
	}
	return (size_t)page_size;
}

bool edge_vmem_reserve(void** out_base, size_t reserve_bytes) {
	void* base = mmap(NULL, reserve_bytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (base == MAP_FAILED) {
		return false;
	}
	*out_base = base;
	return true;
}

bool edge_vmem_release(void* base, size_t reserve_bytes) {
	return munmap(base, reserve_bytes) == 0;
}

bool edge_vmem_commit(void* addr, size_t size) {
	int prot = PROT_READ | PROT_WRITE;
	if (mprotect(addr, size, prot) != 0) {
		return false;
	}

	return true;
}

static int translate_protection_flags(edge_vmem_prot_t p) {
	int flags = 0;
	if (p == EDGE_VMEM_PROT_NONE) {
		return PROT_NONE;
	}
	if ((p & EDGE_VMEM_PROT_READ) != 0) {
		flags |= PROT_READ;
	}
	if ((p & EDGE_VMEM_PROT_WRITE) != 0) {
		flags |= PROT_WRITE;
	}
	if ((p & EDGE_VMEM_PROT_EXEC) != 0) {
		flags |= PROT_EXEC;
	}
	return flags;
}

bool edge_vmem_protect(void* addr, size_t size, edge_vmem_prot_t prot) {
	int flags = translate_protection_flags(prot);
	if (mprotect(addr, size, flags) != 0) {
		return false;
	}
    return true;
}
