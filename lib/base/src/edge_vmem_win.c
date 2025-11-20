#include "edge_vmem.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

size_t edge_vmem_page_size(void) {
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	return (size_t)si.dwPageSize;
}

bool edge_vmem_reserve(void** out_base, size_t reserve_bytes) {
	void* base = VirtualAlloc(NULL, reserve_bytes, MEM_RESERVE, PAGE_NOACCESS);
	if (!base) {
		return false;
	}
	*out_base = base;
	return true;
}

bool edge_vmem_release(void* base, size_t reserve_bytes) {
	(void)reserve_bytes;
	return VirtualFree(base, 0, MEM_RELEASE) != 0;
}

bool edge_vmem_commit(void* addr, size_t size) {
	return VirtualAlloc(addr, size, MEM_COMMIT, PAGE_READWRITE) != NULL;
}

static DWORD translate_protection_flags(edge_vmem_prot_t p) {
	if (p == EDGE_VMEM_PROT_NONE) {
		return PAGE_NOACCESS;
	}

	if ((p & EDGE_VMEM_PROT_WRITE) != 0) {
		if ((p & EDGE_VMEM_PROT_EXEC) != 0) {
			return PAGE_EXECUTE_READWRITE;
		}
		return PAGE_READWRITE;
	}
	// no write
	if ((p & EDGE_VMEM_PROT_EXEC) != 0) {
		return PAGE_EXECUTE_READ;
	}
	return PAGE_READONLY;
}

bool edge_vmem_protect(void* addr, size_t size, edge_vmem_prot_t prot) {
	DWORD old;
	DWORD newf = translate_protection_flags(prot);
	if (!VirtualProtect(addr, size, newf, &old)) {
		return false;
	}
	return true;
}