#ifndef EDGE_CPU_DETECT_H
#define EDGE_CPU_DETECT_H

#include "edge_base.h"
#include "edge_platform_detect.h"

#ifdef __cplusplus
extern "C" {
#endif

	typedef struct {
#if EDGE_ARCH_X64 || EDGE_ARCH_X86
		uint32_t has_sse : 1;
		uint32_t has_sse2 : 1;
		uint32_t has_sse3 : 1;
		uint32_t has_ssse3 : 1;
		uint32_t has_sse4_1 : 1;
		uint32_t has_sse4_2 : 1;
		uint32_t has_avx : 1;
		uint32_t has_avx2 : 1;
		uint32_t has_avx512f : 1;
		uint32_t has_avx512bw : 1;
		uint32_t has_avx512cd : 1;
		uint32_t has_avx512dq : 1;
		uint32_t has_avx512vl : 1;
		uint32_t has_avx512vnni : 1;
		uint32_t has_avx512bf16 : 1;
		uint32_t has_avx512fp16 : 1;
		uint32_t has_fma : 1;
		uint32_t has_f16c : 1;
		uint32_t has_bmi1 : 1;
		uint32_t has_bmi2 : 1;
		uint32_t has_aes : 1;
		uint32_t has_pclmul : 1;
		uint32_t has_popcnt : 1;
		uint32_t has_lzcnt : 1;
		uint32_t has_movbe : 1;
		uint32_t has_sha : 1;

#elif EDGE_ARCH_AARCH64 || EDGE_ARCH_ARM
		uint32_t has_neon : 1;
		uint32_t has_neon_fma : 1;
		uint32_t has_neon_crypto : 1;
		uint32_t has_arm_crc32 : 1;
		uint32_t has_neon_dotprod : 1;
		uint32_t has_neon_fp16 : 1;
		uint32_t has_sve : 1;
		uint32_t has_sve2 : 1;
		uint32_t has_arm_bf16 : 1;
		uint32_t has_arm_i8mm : 1;
#endif

		// CPU info
		uint32_t reserved : 30;

		char vendor[13];
		char brand[49];
		uint32_t family;
		uint32_t model;
		uint32_t stepping;
	} edge_cpu_features_t;

	void edge_cpu_features_get(edge_cpu_features_t* features);

#ifdef __cplusplus
}
#endif

#endif