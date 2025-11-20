#ifndef EDGE_VMEM_H
#define EDGE_VMEM_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

	typedef enum edge_vmem_prot {
		EDGE_VMEM_PROT_NONE = 0,
		EDGE_VMEM_PROT_READ = 0x01,
		EDGE_VMEM_PROT_WRITE = 0x02,
		EDGE_VMEM_PROT_EXEC = 0x04,
	} edge_vmem_prot_t;

	size_t edge_vmem_page_size(void);

	bool edge_vmem_reserve(void** out_base, size_t reserve_bytes);

	bool edge_vmem_release(void* base, size_t reserve_bytes);

	bool edge_vmem_commit(void* addr, size_t size);

	bool edge_vmem_protect(void* addr, size_t size, edge_vmem_prot_t prot);

#ifdef __cplusplus
}
#endif

#endif // EDGE_VMEM_H
