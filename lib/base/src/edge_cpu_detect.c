#include "edge_cpu_detect.h"
#include "edge_platform_detect.h"

#include <stdint.h>
#include <string.h>
#include <stdio.h>


#if defined(EDGE_PLATFORM_WINDOWS)
#include <windows.h>
#include <intrin.h>
#elif defined(EDGE_PLATFORM_POSIX)
#include <unistd.h>
#if defined(EDGE_PLATFORM_LINUX)
#include <sys/auxv.h>
#endif
#endif

#if defined(EDGE_ARCH_AARCH64)
#ifndef HWCAP_FP
#define HWCAP_FP (1 << 0)
#endif
#ifndef HWCAP_ASIMD
#define HWCAP_ASIMD (1 << 1)
#endif
#ifndef HWCAP_AES
#define HWCAP_AES (1 << 3)
#endif
#ifndef HWCAP_PMULL
#define HWCAP_PMULL (1 << 4)
#endif
#ifndef HWCAP_SHA1
#define HWCAP_SHA1 (1 << 5)
#endif
#ifndef HWCAP_SHA2
#define HWCAP_SHA2 (1 << 6)
#endif
#ifndef HWCAP_CRC32
#define HWCAP_CRC32 (1 << 7)
#endif
#ifndef HWCAP_ATOMICS
#define HWCAP_ATOMICS (1 << 8)
#endif
#ifndef HWCAP_FPHP
#define HWCAP_FPHP (1 << 9)
#endif
#ifndef HWCAP_ASIMDHP
#define HWCAP_ASIMDHP (1 << 10)
#endif
#ifndef HWCAP_ASIMDDP
#define HWCAP_ASIMDDP (1 << 20)
#endif
#ifndef HWCAP_SVE
#define HWCAP_SVE (1 << 22)
#endif
#ifndef HWCAP2_SVE2
#define HWCAP2_SVE2 (1 << 1)
#endif
#ifndef HWCAP2_SVEI8MM
#define HWCAP2_SVEI8MM (1 << 9)
#endif
#ifndef HWCAP2_SVEBF16
#define HWCAP2_SVEBF16 (1 << 12)
#endif
#ifndef HWCAP2_I8MM
#define HWCAP2_I8MM (1 << 13)
#endif
#ifndef HWCAP2_BF16
#define HWCAP2_BF16 (1 << 14)
#endif
#elif defined(EDGE_ARCH_ARM)
#ifndef HWCAP_VFP
#define HWCAP_VFP (1 << 6)
#endif
#ifndef HWCAP_NEON
#define HWCAP_NEON (1 << 12)
#endif
#ifndef HWCAP_VFPv3
#define HWCAP_VFPv3 (1 << 13)
#endif
#ifndef HWCAP_VFPv4
#define HWCAP_VFPv4 (1 << 16)
#endif
#ifndef HWCAP_IDIVA
#define HWCAP_IDIVA (1 << 17)
#endif
#ifndef HWCAP_IDIVT
#define HWCAP_IDIVT (1 << 18)
#endif
#ifndef HWCAP2_AES
#define HWCAP2_AES (1 << 0)
#endif
#ifndef HWCAP2_PMULL
#define HWCAP2_PMULL (1 << 1)
#endif
#ifndef HWCAP2_SHA1
#define HWCAP2_SHA1 (1 << 2)
#endif
#ifndef HWCAP2_SHA2
#define HWCAP2_SHA2 (1 << 3)
#endif
#ifndef HWCAP2_CRC32
#define HWCAP2_CRC32 (1 << 4)
#endif
#endif

#if defined(EDGE_ARCH_X64) || defined(EDGE_ARCH_X86)
static void edge_cpuid(uint32_t leaf, uint32_t subleaf, uint32_t* eax, uint32_t* ebx, uint32_t* ecx, uint32_t* edx) {
#if defined(EDGE_COMPILER_MSVC)
	int regs[4];
	__cpuidex(regs, (int)leaf, (int)subleaf);
	*eax = (uint32_t)regs[0];
	*ebx = (uint32_t)regs[1];
	*ecx = (uint32_t)regs[2];
	*edx = (uint32_t)regs[3];
#elif defined(EDGE_COMPILER_GCC) || defined(EDGE_COMPILER_CLANG)
	__asm__ __volatile__(
		"cpuid"
		: "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
		: "a"(leaf), "c"(subleaf)
	);
#endif
}
#endif

void edge_cpu_features_get(edge_cpu_features_t* features) {
	memset(features, 0, sizeof(edge_cpu_features_t));

#if defined(EDGE_ARCH_X64) || defined(EDGE_ARCH_X86)
	uint32_t eax, ebx, ecx, edx;
	uint32_t max_leaf, max_ext_leaf;

	edge_cpuid(0, 0, &max_leaf, &ebx, &ecx, &edx);
	memcpy(features->vendor + 0, &ebx, 4);
	memcpy(features->vendor + 4, &edx, 4);
	memcpy(features->vendor + 8, &ecx, 4);
	features->vendor[12] = '\0';

	if (max_leaf >= 1) {
		edge_cpuid(1, 0, &eax, &ebx, &ecx, &edx);

		features->stepping = eax & 0xF;
		features->model = (eax >> 4) & 0xF;
		features->family = (eax >> 8) & 0xF;

		if (features->family == 0xF) {
			features->family += (eax >> 20) & 0xFF;
		}
		if (features->family == 0x6 || features->family == 0xF) {
			features->model += ((eax >> 16) & 0xF) << 4;
		}

		features->has_sse3 = (ecx & (1 << 0)) != 0;
		features->has_pclmul = (ecx & (1 << 1)) != 0;
		features->has_ssse3 = (ecx & (1 << 9)) != 0;
		features->has_fma = (ecx & (1 << 12)) != 0;
		features->has_sse4_1 = (ecx & (1 << 19)) != 0;
		features->has_sse4_2 = (ecx & (1 << 20)) != 0;
		features->has_movbe = (ecx & (1 << 22)) != 0;
		features->has_popcnt = (ecx & (1 << 23)) != 0;
		features->has_aes = (ecx & (1 << 25)) != 0;
		features->has_avx = (ecx & (1 << 28)) != 0;
		features->has_f16c = (ecx & (1 << 29)) != 0;

		features->has_sse = (edx & (1 << 25)) != 0;
		features->has_sse2 = (edx & (1 << 26)) != 0;
	}

	if (max_leaf >= 7) {
		edge_cpuid(7, 0, &eax, &ebx, &ecx, &edx);

		features->has_bmi1 = (ebx & (1 << 3)) != 0;
		features->has_avx2 = (ebx & (1 << 5)) != 0;
		features->has_bmi2 = (ebx & (1 << 8)) != 0;
		features->has_avx512f = (ebx & (1 << 16)) != 0;
		features->has_avx512dq = (ebx & (1 << 17)) != 0;
		features->has_avx512cd = (ebx & (1 << 28)) != 0;
		features->has_avx512bw = (ebx & (1 << 30)) != 0;
		features->has_avx512vl = (ebx & (1 << 31)) != 0;

		features->has_avx512vnni = (ecx & (1 << 11)) != 0;
		features->has_avx512bf16 = (eax & (1 << 5)) != 0;

		features->has_sha = (ebx & (1 << 29)) != 0;

		features->has_avx512fp16 = (edx & (1 << 23)) != 0;
	}

	edge_cpuid(0x80000000, 0, &max_ext_leaf, &ebx, &ecx, &edx);

	if (max_ext_leaf >= 0x80000001) {
		edge_cpuid(0x80000001, 0, &eax, &ebx, &ecx, &edx);
		features->has_lzcnt = (ecx & (1 << 5)) != 0;
	}

	if (max_ext_leaf >= 0x80000004) {
		uint32_t* brand_ptr = (uint32_t*)features->brand;
		edge_cpuid(0x80000002, 0, &brand_ptr[0], &brand_ptr[1], &brand_ptr[2], &brand_ptr[3]);
		edge_cpuid(0x80000003, 0, &brand_ptr[4], &brand_ptr[5], &brand_ptr[6], &brand_ptr[7]);
		edge_cpuid(0x80000004, 0, &brand_ptr[8], &brand_ptr[9], &brand_ptr[10], &brand_ptr[11]);
		features->brand[48] = '\0';

		char* start = features->brand;
		while (*start == ' ') start++;
		if (start != features->brand) {
			memmove(features->brand, start, strlen(start) + 1);
		}
	}
#elif defined(EDGE_ARCH_AARCH64) || defined(EDGE_ARCH_ARM)
	unsigned long hwcap = getauxval(AT_HWCAP);

#if defined(EDGE_ARCH_AARCH64)
	features->has_neon = 1;

	features->has_neon_fma = (hwcap & HWCAP_ASIMD) != 0;
	features->has_neon_crypto = ((hwcap & HWCAP_AES) && (hwcap & HWCAP_PMULL) &&
		(hwcap & HWCAP_SHA1) && (hwcap & HWCAP_SHA2)) != 0;
	features->has_arm_crc32 = (hwcap & HWCAP_CRC32) != 0;
	features->has_neon_fp16 = (hwcap & HWCAP_ASIMDHP) != 0;
	features->has_neon_dotprod = (hwcap & HWCAP_ASIMDDP) != 0;
	features->has_sve = (hwcap & HWCAP_SVE) != 0;

	unsigned long hwcap2 = getauxval(AT_HWCAP2);

	features->has_sve2 = (hwcap2 & HWCAP2_SVE2) != 0;
	features->has_arm_bf16 = (hwcap2 & HWCAP2_BF16) != 0;
	features->has_arm_i8mm = (hwcap2 & HWCAP2_I8MM) != 0;

	strcpy(features->vendor, "ARM");

	FILE* cpuinfo = fopen("/proc/cpuinfo", "r");
	if (cpuinfo) {
		char line[256];
		while (fgets(line, sizeof(line), cpuinfo)) {
			if (strncmp(line, "CPU implementer", 15) == 0) {
				unsigned int implementer;
				if (sscanf(line, "CPU implementer : 0x%x", &implementer) == 1) {
					features->family = implementer;
				}
			}
			else if (strncmp(line, "CPU variant", 11) == 0) {
				unsigned int variant;
				if (sscanf(line, "CPU variant : 0x%x", &variant) == 1) {
					features->model = variant;
				}
			}
			else if (strncmp(line, "CPU revision", 12) == 0) {
				unsigned int revision;
				if (sscanf(line, "CPU revision : %u", &revision) == 1) {
					features->stepping = revision;
				}
			}
			else if (strncmp(line, "Hardware", 8) == 0 ||
				strncmp(line, "model name", 10) == 0) {
				char* colon = strchr(line, ':');
				if (colon) {
					colon++;
					while (*colon == ' ' || *colon == '\t') colon++;

					size_t len = strlen(colon);
					if (len > 0 && colon[len - 1] == '\n') {
						colon[len - 1] = '\0';
					}

					strncpy(features->brand, colon, 48);
					features->brand[48] = '\0';
					break;
				}
			}
		}
		fclose(cpuinfo);
	}

	if (features->brand[0] == '\0') {
		strcpy(features->brand, "ARM AArch64 Processor");
	}
#elif defined(EDGE_ARCH_ARM)
	features->has_neon = (hwcap & HWCAP_NEON) != 0;
	features->has_neon_fma = (hwcap & HWCAP_VFPv4) != 0;

	unsigned long hwcap2 = getauxval(AT_HWCAP2);

	features->has_neon_crypto = ((hwcap2 & HWCAP2_AES) && (hwcap2 & HWCAP2_PMULL) &&
		(hwcap2 & HWCAP2_SHA1) && (hwcap2 & HWCAP2_SHA2)) != 0;
	features->has_arm_crc32 = (hwcap2 & HWCAP2_CRC32) != 0;

	strcpy(features->vendor, "ARM");

	FILE* cpuinfo = fopen("/proc/cpuinfo", "r");
	if (cpuinfo) {
		char line[256];
		while (fgets(line, sizeof(line), cpuinfo)) {
			if (strncmp(line, "CPU implementer", 15) == 0) {
				unsigned int implementer;
				if (sscanf(line, "CPU implementer : 0x%x", &implementer) == 1) {
					features->family = implementer;
				}
			}
			else if (strncmp(line, "CPU variant", 11) == 0) {
				unsigned int variant;
				if (sscanf(line, "CPU variant : 0x%x", &variant) == 1) {
					features->model = variant;
				}
			}
			else if (strncmp(line, "CPU revision", 12) == 0) {
				unsigned int revision;
				if (sscanf(line, "CPU revision : %u", &revision) == 1) {
					features->stepping = revision;
				}
			}
			else if (strncmp(line, "Hardware", 8) == 0 ||
				strncmp(line, "Processor", 9) == 0) {
				char* colon = strchr(line, ':');
				if (colon) {
					colon++;
					while (*colon == ' ' || *colon == '\t') colon++;

					size_t len = strlen(colon);
					if (len > 0 && colon[len - 1] == '\n') {
						colon[len - 1] = '\0';
					}

					strncpy(features->brand, colon, 48);
					features->brand[48] = '\0';
					break;
				}
			}
		}
		fclose(cpuinfo);
	}

	if (features->brand[0] == '\0') {
		strcpy(features->brand, "ARM Processor");
	}
#endif

#endif
}