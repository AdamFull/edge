#ifndef EDGE_PLATFORM_DETECT_H
#define EDGE_PLATFORM_DETECT_H

#ifdef __cplusplus
extern "C" {
#endif

// Target platform detection
#if defined(_WIN32) || defined(_WIN64) || defined(__WIN32__) ||                \
    defined(__WINDOWS__)
#define EDGE_PLATFORM_WINDOWS 1
#define EDGE_PLATFORM_NAME "Windows"
#elif defined(__ANDROID__)
#define EDGE_PLATFORM_ANDROID 1
#define EDGE_PLATFORM_NAME "Android"
#elif defined(__linux__) || defined(__gnu_linux__)
#define EDGE_PLATFORM_LINUX 1
#define EDGE_PLATFORM_NAME "Linux"
#elif defined(__APPLE__) && defined(__MACH__)
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
#define EDGE_PLATFORM_IOS 1
#define EDGE_PLATFORM_NAME "iOS"
#elif TARGET_OS_MAC
#define EDGE_PLATFORM_MACOS 1
#define EDGE_PLATFORM_NAME "macOS"
#endif
#else
#define EDGE_PLATFORM_UNKNOWN 1
#define EDGE_PLATFORM_NAME "Unknown"
#warning "Unknown platform detected"
#endif

// Target arch detection
#if defined(__x86_64__) || defined(__x86_64) || defined(__amd64__) ||          \
    defined(__amd64) || defined(_M_X64) || defined(_M_AMD64)
#define EDGE_ARCH_X64 1
#define EDGE_ARCH_NAME "x64"
#define EDGE_ARCH_BITS 64
#elif defined(i386) || defined(__i386) || defined(__i386__) ||                 \
    defined(_M_IX86) || defined(_X86_)
#define EDGE_ARCH_X86 1
#define EDGE_ARCH_NAME "x86"
#define EDGE_ARCH_BITS 32
#elif defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64)
#define EDGE_ARCH_AARCH64 1
#define EDGE_ARCH_NAME "aarch64"
#define EDGE_ARCH_BITS 64
#elif defined(__arm__) || defined(_ARM) || defined(_M_ARM) || defined(__arm)
#define EDGE_ARCH_ARM 1
#define EDGE_ARCH_NAME "arm"
#define EDGE_ARCH_BITS 32
#else
#define EDGE_ARCH_UNKNOWN 1
#define EDGE_ARCH_NAME "Unknown"
#define EDGE_ARCH_BITS 0
#warning "Unknown architecture detected"
#endif

#if EDGE_ARCH_BITS == 64
#define EDGE_64BIT 1
#define EDGE_POINTER_SIZE 8
#elif EDGE_ARCH_BITS == 32
#define EDGE_32BIT 1
#define EDGE_POINTER_SIZE 4
#endif

// Compiler detection
#if defined(_MSC_VER)
#define EDGE_COMPILER_MSVC 1
#define EDGE_COMPILER_NAME "MSVC"
#define EDGE_COMPILER_VERSION _MSC_VER
#elif defined(__clang__)
#define EDGE_COMPILER_CLANG 1
#define EDGE_COMPILER_NAME "Clang"
#define EDGE_COMPILER_VERSION                                                  \
  (__clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__)
#elif defined(__GNUC__)
#define EDGE_COMPILER_GCC 1
#define EDGE_COMPILER_NAME "GCC"
#define EDGE_COMPILER_VERSION                                                  \
  (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#else
#define EDGE_COMPILER_UNKNOWN 1
#define EDGE_COMPILER_NAME "Unknown"
#define EDGE_COMPILER_VERSION 0
#endif

// Build configuration detections
#if defined(DEBUG) || defined(_DEBUG) || !defined(NDEBUG)
#define EDGE_DEBUG 1
#define EDGE_BUILD_CONFIG "Debug"
#else
#define EDGE_RELEASE 1
#define EDGE_BUILD_CONFIG "Release"
#endif

// Endiannes detection
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) &&             \
    defined(__ORDER_BIG_ENDIAN__)
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define EDGE_LITTLE_ENDIAN 1
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define EDGE_BIG_ENDIAN 1
#endif
#elif defined(_WIN32) || defined(_WIN64)
#define EDGE_LITTLE_ENDIAN 1
#elif defined(__LITTLE_ENDIAN__) || defined(__ARMEL__) ||                      \
    defined(__THUMBEL__) || defined(__AARCH64EL__) || defined(_MIPSEL) ||      \
    defined(__MIPSEL) || defined(__MIPSEL__)
#define EDGE_LITTLE_ENDIAN 1
#elif defined(__BIG_ENDIAN__) || defined(__ARMEB__) || defined(__THUMBEB__) || \
    defined(__AARCH64EB__) || defined(_MIPSEB) || defined(__MIPSEB) ||         \
    defined(__MIPSEB__)
#define EDGE_BIG_ENDIAN 1
#else
#if defined(EDGE_ARCH_X64) || defined(EDGE_ARCH_AARCH64)
#define EDGE_LITTLE_ENDIAN 1
#else
#warning "Could not detect endianness, assuming little endian"
#define EDGE_LITTLE_ENDIAN 1
#endif
#endif

// x86/x64 SIMD Instructions
#if defined(EDGE_ARCH_X64) || defined(EDGE_ARCH_X86)

// SSE family
#if defined(__SSE__) || defined(_M_X64) ||                                     \
    (defined(_M_IX86_FP) && _M_IX86_FP >= 1)
#define EDGE_HAS_SSE 1
#endif

#if defined(__SSE2__) || defined(_M_X64) ||                                    \
    (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#define EDGE_HAS_SSE2 1
#endif

#if defined(__SSE3__)
#define EDGE_HAS_SSE3 1
#endif

#if defined(__SSSE3__)
#define EDGE_HAS_SSSE3 1
#endif

#if defined(__SSE4_1__)
#define EDGE_HAS_SSE4_1 1
#endif

#if defined(__SSE4_2__)
#define EDGE_HAS_SSE4_2 1
#endif

// AVX family
#if defined(__AVX__)
#define EDGE_HAS_AVX 1
#endif

#if defined(__AVX2__)
#define EDGE_HAS_AVX2 1
#endif

#if defined(__AVX512F__)
#define EDGE_HAS_AVX512F 1
#endif

#if defined(__AVX512BW__)
#define EDGE_HAS_AVX512BW 1
#endif

#if defined(__AVX512CD__)
#define EDGE_HAS_AVX512CD 1
#endif

#if defined(__AVX512DQ__)
#define EDGE_HAS_AVX512DQ 1
#endif

#if defined(__AVX512VL__)
#define EDGE_HAS_AVX512VL 1
#endif

#if defined(__AVX512VNNI__)
#define EDGE_HAS_AVX512VNNI 1
#endif

#if defined(__AVX512BF16__)
#define EDGE_HAS_AVX512BF16 1
#endif

#if defined(__AVX512FP16__)
#define EDGE_HAS_AVX512FP16 1
#endif

// Other x86 extensions
#if defined(__FMA__)
#define EDGE_HAS_FMA 1
#endif

#if defined(__F16C__)
#define EDGE_HAS_F16C 1
#endif

#if defined(__BMI__)
#define EDGE_HAS_BMI 1
#endif

#if defined(__BMI2__)
#define EDGE_HAS_BMI2 1
#endif

#if defined(__AES__)
#define EDGE_HAS_AES 1
#endif

#if defined(__PCLMUL__)
#define EDGE_HAS_PCLMUL 1
#endif

#if defined(__POPCNT__)
#define EDGE_HAS_POPCNT 1
#endif

#if defined(__LZCNT__)
#define EDGE_HAS_LZCNT 1
#endif

#endif

#if defined(EDGE_ARCH_AARCH64) || defined(EDGE_ARCH_ARM)

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#define EDGE_HAS_NEON 1
#endif

#if defined(EDGE_ARCH_AARCH64)
// NEON is mandatory on AArch64
#ifndef EDGE_HAS_NEON
#define EDGE_HAS_NEON 1
#endif
#endif

#if defined(__ARM_FEATURE_FMA)
#define EDGE_HAS_NEON_FMA 1
#endif

#if defined(__ARM_FEATURE_CRYPTO)
#define EDGE_HAS_NEON_CRYPTO 1
#endif

#if defined(__ARM_FEATURE_CRC32)
#define EDGE_HAS_ARM_CRC32 1
#endif

#if defined(__ARM_FEATURE_DOTPROD)
#define EDGE_HAS_NEON_DOTPROD 1
#endif

#if defined(__ARM_FEATURE_FP16_VECTOR_ARITHMETIC)
#define EDGE_HAS_NEON_FP16 1
#endif

#if defined(__ARM_FEATURE_SVE)
#define EDGE_HAS_SVE 1
#endif

#if defined(__ARM_FEATURE_SVE2)
#define EDGE_HAS_SVE2 1
#endif

#if defined(__ARM_FEATURE_BF16)
#define EDGE_HAS_ARM_BF16 1
#endif

#if defined(__ARM_FEATURE_MATMUL_INT8)
#define EDGE_HAS_ARM_I8MM 1
#endif

#endif

#if defined(EDGE_HAS_SSE) || defined(EDGE_HAS_NEON)
#define EDGE_HAS_SIMD 1
#endif

#if defined(EDGE_HAS_AVX) || defined(EDGE_HAS_AVX2) || defined(EDGE_HAS_AVX512F)
#define EDGE_HAS_ADVANCED_SIMD 1
#endif

#if defined(EDGE_HAS_SSE)
#include <xmmintrin.h>
#endif

#if defined(EDGE_HAS_SSE2)
#include <emmintrin.h>
#endif

#if defined(EDGE_HAS_SSE3)
#include <pmmintrin.h>
#endif

#if defined(EDGE_HAS_SSSE3)
#include <tmmintrin.h>
#endif

#if defined(EDGE_HAS_SSE4_1)
#include <smmintrin.h>
#endif

#if defined(EDGE_HAS_SSE4_2)
#include <nmmintrin.h>
#endif

#if defined(EDGE_HAS_AVX)
#include <immintrin.h>
#endif

#if defined(EDGE_HAS_NEON)
#include <arm_neon.h>
#endif

#if defined(EDGE_HAS_SVE)
#include <arm_sve.h>
#endif

// Platform feature
#if defined(EDGE_PLATFORM_LINUX) || defined(EDGE_PLATFORM_ANDROID)
#define EDGE_PLATFORM_POSIX 1
#endif

#if defined(EDGE_PLATFORM_WINDOWS)
#define EDGE_HAS_WINDOWS_API 1
#endif

#if defined(EDGE_PLATFORM_ANDROID)
#define EDGE_HAS_ANDROID_NDK 1
#endif

// Attributes
#if defined(EDGE_COMPILER_MSVC)
#define EDGE_FORCE_INLINE __forceinline
#elif defined(EDGE_COMPILER_GCC) || defined(EDGE_COMPILER_CLANG)
#define EDGE_FORCE_INLINE __attribute__((always_inline)) inline
#else
#define EDGE_FORCE_INLINE inline
#endif

#if defined(EDGE_COMPILER_MSVC)
#define EDGE_NO_INLINE __declspec(noinline)
#elif defined(EDGE_COMPILER_GCC) || defined(EDGE_COMPILER_CLANG)
#define EDGE_NO_INLINE __attribute__((noinline))
#else
#define EDGE_NO_INLINE
#endif

#if defined(EDGE_PLATFORM_WINDOWS)
#if defined(EDGE_BUILD_SHARED)
#define EDGE_API __declspec(dllexport)
#elif defined(EDGE_USE_SHARED)
#define EDGE_API __declspec(dllimport)
#else
#define EDGE_API
#endif
#else
#if defined(EDGE_COMPILER_GCC) || defined(EDGE_COMPILER_CLANG)
#define EDGE_API __attribute__((visibility("default")))
#else
#define EDGE_API
#endif
#endif

#if defined(EDGE_COMPILER_MSVC)
#define EDGE_DEPRECATED __declspec(deprecated)
#elif defined(EDGE_COMPILER_GCC) || defined(EDGE_COMPILER_CLANG)
#define EDGE_DEPRECATED __attribute__((deprecated))
#else
#define EDGE_DEPRECATED
#endif

#if defined(EDGE_COMPILER_MSVC)
#define EDGE_ALIGN(x) __declspec(align(x))
#elif defined(EDGE_COMPILER_GCC) || defined(EDGE_COMPILER_CLANG)
#define EDGE_ALIGN(x) __attribute__((aligned(x)))
#else
#define EDGE_ALIGN(x)
#endif

#if defined(EDGE_COMPILER_GCC) || defined(EDGE_COMPILER_CLANG)
#define EDGE_LIKELY(x) __builtin_expect(!!(x), 1)
#define EDGE_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define EDGE_LIKELY(x) (x)
#define EDGE_UNLIKELY(x) (x)
#endif

#if defined(EDGE_COMPILER_MSVC)
#define EDGE_UNREACHABLE() __assume(0)
#elif defined(EDGE_COMPILER_GCC) || defined(EDGE_COMPILER_CLANG)
#define EDGE_UNREACHABLE() __builtin_unreachable()
#else
#define EDGE_UNREACHABLE()
#endif

#if defined(EDGE_HAS_AVX512F)
#define EDGE_SIMD_ALIGN EDGE_ALIGN(64)
#define EDGE_SIMD_ALIGNMENT 64
#elif defined(EDGE_HAS_AVX) || defined(EDGE_HAS_AVX2)
#define EDGE_SIMD_ALIGN EDGE_ALIGN(32)
#define EDGE_SIMD_ALIGNMENT 32
#elif defined(EDGE_HAS_SSE) || defined(EDGE_HAS_NEON)
#define EDGE_SIMD_ALIGN EDGE_ALIGN(16)
#define EDGE_SIMD_ALIGNMENT 16
#else
#define EDGE_SIMD_ALIGN
#define EDGE_SIMD_ALIGNMENT 0
#endif

#ifdef __cplusplus
}
#endif

#endif // EDGE_PLATFORM_DETECT_H
