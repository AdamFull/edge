#ifndef EDGE_BASE_H
#define EDGE_BASE_H

#include <stdint.h>
#include <stdbool.h>

#define EDGE_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#ifdef __cplusplus
extern "C" {
#endif

	typedef int8_t i8;
	typedef uint8_t u8;
	typedef int16_t i16;
	typedef uint16_t u16;
	typedef int32_t i32;
	typedef uint32_t u32;
	typedef int64_t i64;
	typedef uint64_t u64;
	typedef float f32;
	typedef double f64;

#ifdef __cplusplus
}
#endif

#endif // EDGE_BASE_H
