#ifndef EDGE_VMEM_H
#define EDGE_VMEM_H

#include "stddef.hpp"

namespace edge {
	enum class VMemProt : u32 {
		None = 0,
		Read = 0x01,
		Write = 0x02,
		Exec = 0x04,
		ReadWrite = Read | Write,
		ReadExec = Read | Exec,
		ReadWriteExec = Read | Write | Exec
	};

	constexpr VMemProt operator|(VMemProt a, VMemProt b) {
		return static_cast<VMemProt>(static_cast<u32>(a) | static_cast<u32>(b));
	}

	constexpr VMemProt operator&(VMemProt a, VMemProt b) {
		return static_cast<VMemProt>(static_cast<u32>(a) & static_cast<u32>(b));
	}

	constexpr VMemProt operator^(VMemProt a, VMemProt b) {
		return static_cast<VMemProt>(static_cast<u32>(a) ^ static_cast<u32>(b));
	}

	constexpr VMemProt operator~(VMemProt a) {
		return static_cast<VMemProt>(~static_cast<u32>(a));
	}

	constexpr VMemProt& operator|=(VMemProt& a, VMemProt b) {
		return a = a | b;
	}

	constexpr VMemProt& operator&=(VMemProt& a, VMemProt b) {
		return a = a & b;
	}

	constexpr VMemProt& operator^=(VMemProt& a, VMemProt b) {
		return a = a ^ b;
	}

	usize vmem_page_size();
	bool vmem_reserve(void** out_base, usize reserve_bytes);
	bool vmem_release(void* base, usize reserve_bytes);
	bool vmem_commit(void* addr, usize size);
	bool vmem_protect(void* addr, usize size, VMemProt prot);
}

#endif