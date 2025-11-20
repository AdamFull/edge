#ifndef EDGE_PLATFORM_DETECT_H
#define EDGE_PLATFORM_DETECT_H

#ifdef __cplusplus
extern "C" {
#endif

	// Target platform detection
#if defined(_WIN32) || defined(_WIN64) || defined(__WIN32__) || defined(__WINDOWS__)
#define EDGE_PLATFORM_WINDOWS 1
#define EDGE_PLATFORM_NAME "Windows"
#elif defined(__ANDROID__)
#define EDGE_PLATFORM_ANDROID 1
#define EDGE_PLATFORM_NAME "Android"
#elif defined(__linux__) || defined(__gnu_linux__)
#define EDGE_PLATFORM_LINUX 1
#define EDGE_PLATFORM_NAME "Linux"
#else
#define EDGE_PLATFORM_UNKNOWN 1
#define EDGE_PLATFORM_NAME "Unknown"
#warning "Unknown platform detected"
#endif

	// Target arch detection
#if defined(__x86_64__) || defined(__x86_64) || defined(__amd64__) || defined(__amd64) || defined(_M_X64) || defined(_M_AMD64)
#define EDGE_ARCH_X64 1
#define EDGE_ARCH_NAME "x64"
#define EDGE_ARCH_BITS 64
#elif defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64)
#define EDGE_ARCH_AARCH64 1
#define EDGE_ARCH_NAME "aarch64"
#define EDGE_ARCH_BITS 64
#else
#define EDGE_ARCH_UNKNOWN 1
#define EDGE_ARCH_NAME "Unknown"
#define EDGE_ARCH_BITS 0
#warning "Unknown architecture detected"
#endif

	// Compiler detection
#if defined(_MSC_VER)
#define EDGE_COMPILER_MSVC 1
#define EDGE_COMPILER_NAME "MSVC"
#define EDGE_COMPILER_VERSION _MSC_VER
#elif defined(__clang__)
#define EDGE_COMPILER_CLANG 1
#define EDGE_COMPILER_NAME "Clang"
#define EDGE_COMPILER_VERSION (__clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__)
#elif defined(__GNUC__)
#define EDGE_COMPILER_GCC 1
#define EDGE_COMPILER_NAME "GCC"
#define EDGE_COMPILER_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
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
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && defined(__ORDER_BIG_ENDIAN__)
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define EDGE_LITTLE_ENDIAN 1
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define EDGE_BIG_ENDIAN 1
#endif
#elif defined(_WIN32) || defined(_WIN64)
#define EDGE_LITTLE_ENDIAN 1
#elif defined(__LITTLE_ENDIAN__) || defined(__ARMEL__) || defined(__THUMBEL__) || \
      defined(__AARCH64EL__) || defined(_MIPSEL) || defined(__MIPSEL) || defined(__MIPSEL__)
#define EDGE_LITTLE_ENDIAN 1
#elif defined(__BIG_ENDIAN__) || defined(__ARMEB__) || defined(__THUMBEB__) || \
      defined(__AARCH64EB__) || defined(_MIPSEB) || defined(__MIPSEB) || defined(__MIPSEB__)
#define EDGE_BIG_ENDIAN 1
#else
#if defined(EDGE_ARCH_X64) || defined(EDGE_ARCH_AARCH64)
#define EDGE_LITTLE_ENDIAN 1
#else
#warning "Could not detect endianness, assuming little endian"
#define EDGE_LITTLE_ENDIAN 1
#endif
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
#define EDGE_LIKELY(x)   __builtin_expect(!!(x), 1)
#define EDGE_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define EDGE_LIKELY(x)   (x)
#define EDGE_UNLIKELY(x) (x)
#endif

#if defined(EDGE_COMPILER_MSVC)
#define EDGE_UNREACHABLE() __assume(0)
#elif defined(EDGE_COMPILER_GCC) || defined(EDGE_COMPILER_CLANG)
#define EDGE_UNREACHABLE() __builtin_unreachable()
#else
#define EDGE_UNREACHABLE()
#endif

#ifdef __cplusplus
}
#endif

#endif // EDGE_PLATFORM_DETECT_H
