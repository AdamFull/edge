#ifndef EDGE_MATH_H
#define EDGE_MATH_H

#include "edge_base.h"

#include <math.h>

#define EM_PI32  3.14159265358979323846f
#define EM_PI64  3.14159265358979323846
#define EM_TAU32 6.28318530717958647692f
#define EM_TAU64 6.28318530717958647692
#define EM_E32   2.71828182845904523536f
#define EM_E64   2.71828182845904523536

#define EM_DEG_TO_RAD32 0.01745329251f
#define EM_RAD_TO_DEG32 57.2957795131f
#define EM_DEG_TO_RAD64 0.01745329251994329576923690768489
#define EM_RAD_TO_DEG64 57.295779513082320876798154814105

#define EM_EPSILON_F32 1.192092896e-07f
#define EM_EPSILON_F64 2.2204460492503131e-16

#ifdef __cplusplus
extern "C" {
#endif

	static inline i8 _em_min_i8(i8 a, i8 b) { return a < b ? a : b; }
	static inline u8 _em_min_u8(u8 a, u8 b) { return a < b ? a : b; }
	static inline i16 _em_min_i16(i16 a, i16 b) { return a < b ? a : b; }
	static inline u16 _em_min_u16(u16 a, u16 b) { return a < b ? a : b; }
	static inline i32 _em_min_i32(i32 a, i32 b) { return a < b ? a : b; }
	static inline u32 _em_min_u32(u32 a, u32 b) { return a < b ? a : b; }
	static inline i64 _em_min_i64(i64 a, i64 b) { return a < b ? a : b; }
	static inline u64 _em_min_u64(u64 a, u64 b) { return a < b ? a : b; }
	static inline f32 _em_min_f32(f32 a, f32 b) { return a < b ? a : b; }
	static inline f64 _em_min_f64(f64 a, f64 b) { return a < b ? a : b; }

#define em_min(a, b) _Generic((a), \
    i8: _em_min_i8, \
    u8: _em_min_u8, \
    i16: _em_min_i16, \
    u16: _em_min_u16, \
    i32: _em_min_i32, \
    u32: _em_min_u32, \
    i64: _em_min_i64, \
    u64: _em_min_u64, \
    f32: _em_min_f32, \
    f64: _em_min_f64 \
)(a, b)

	static inline i8 _em_max_i8(i8 a, i8 b) { return a > b ? a : b; }
	static inline u8 _em_max_u8(u8 a, u8 b) { return a > b ? a : b; }
	static inline i16 _em_max_i16(i16 a, i16 b) { return a > b ? a : b; }
	static inline u16 _em_max_u16(u16 a, u16 b) { return a > b ? a : b; }
	static inline i32 _em_max_i32(i32 a, i32 b) { return a > b ? a : b; }
	static inline u32 _em_max_u32(u32 a, u32 b) { return a > b ? a : b; }
	static inline i64 _em_max_i64(i64 a, i64 b) { return a > b ? a : b; }
	static inline u64 _em_max_u64(u64 a, u64 b) { return a > b ? a : b; }
	static inline f32 _em_max_f32(f32 a, f32 b) { return a > b ? a : b; }
	static inline f64 _em_max_f64(f64 a, f64 b) { return a > b ? a : b; }

#define em_max(a, b) _Generic((a), \
    i8:  _em_max_i8,  \
    u8:  _em_max_u8,  \
    i16: _em_max_i16, \
    u16: _em_max_u16, \
    i32: _em_max_i32, \
    u32: _em_max_u32, \
    i64: _em_max_i64, \
    u64: _em_max_u64, \
    f32: _em_max_f32, \
    f64: _em_max_f64  \
)(a, b)

    static inline i8 _em_clamp_i8(i8 x, i8 low, i8 high) { return x < low ? low : (x > high ? high : x); }
    static inline u8 _em_clamp_u8(u8 x, u8 low, u8 high) { return x < low ? low : (x > high ? high : x); }
    static inline i16 _em_clamp_i16(i16 x, i16 low, i16 high) { return x < low ? low : (x > high ? high : x); }
    static inline u16 _em_clamp_u16(u16 x, u16 low, u16 high) { return x < low ? low : (x > high ? high : x); }
    static inline i32 _em_clamp_i32(i32 x, i32 low, i32 high) { return x < low ? low : (x > high ? high : x); }
    static inline u32 _em_clamp_u32(u32 x, u32 low, u32 high) { return x < low ? low : (x > high ? high : x); }
    static inline i64 _em_clamp_i64(i64 x, i64 low, i64 high) { return x < low ? low : (x > high ? high : x); }
    static inline u64 _em_clamp_u64(u64 x, u64 low, u64 high) { return x < low ? low : (x > high ? high : x); }
    static inline f32 _em_clamp_f32(f32 x, f32 low, f32 high) { return x < low ? low : (x > high ? high : x); }
    static inline f64 _em_clamp_f64(f64 x, f64 low, f64 high) { return x < low ? low : (x > high ? high : x); }

#define em_clamp(x, low, high) _Generic((x), \
    i8:  _em_clamp_i8,  \
    u8:  _em_clamp_u8,  \
    i16: _em_clamp_i16, \
    u16: _em_clamp_u16, \
    i32: _em_clamp_i32, \
    u32: _em_clamp_u32, \
    i64: _em_clamp_i64, \
    u64: _em_clamp_u64, \
    f32: _em_clamp_f32, \
    f64: _em_clamp_f64  \
)(x, low, high)

    static inline f32 _em_clamp01_f32(f32 x) { return _em_clamp_f32(x, 0.0f, 1.0f); }
    static inline f64 _em_clamp01_f64(f64 x) { return _em_clamp_f64(x, 0.0, 1.0); }

#define em_clamp01(x) _Generic((x), \
    f32: _em_clamp01_f32, \
    f64: _em_clamp01_f64  \
)(x)

    static inline i32 _em_gcd_i32(i32 a, i32 b) {
        a = a < 0 ? -a : a;
        b = b < 0 ? -b : b;
        while (b != 0) {
            i32 temp = b;
            b = a % b;
            a = temp;
        }
        return a;
    }

    static inline u32 _em_gcd_u32(u32 a, u32 b) {
        while (b != 0) {
            u32 temp = b;
            b = a % b;
            a = temp;
        }
        return a;
    }

    static inline i64 _em_gcd_i64(i64 a, i64 b) {
        a = a < 0 ? -a : a;
        b = b < 0 ? -b : b;
        while (b != 0) {
            i64 temp = b;
            b = a % b;
            a = temp;
        }
        return a;
    }

    static inline u64 _em_gcd_u64(u64 a, u64 b) {
        while (b != 0) {
            u64 temp = b;
            b = a % b;
            a = temp;
        }
        return a;
    }

#define em_gcd(a, b) _Generic((a), \
    i32: _em_gcd_i32, \
    u32: _em_gcd_u32, \
    i64: _em_gcd_i64, \
    u64: _em_gcd_u64  \
)(a, b)

    static inline i32 _em_lcm_i32(i32 a, i32 b) {
        if (a == 0 || b == 0) return 0;
        a = a < 0 ? -a : a;
        b = b < 0 ? -b : b;
        return (a / _em_gcd_i32(a, b)) * b;
    }

    static inline u32 _em_lcm_u32(u32 a, u32 b) {
        if (a == 0 || b == 0) return 0;
        return (a / _em_gcd_u32(a, b)) * b;
    }

    static inline i64 _em_lcm_i64(i64 a, i64 b) {
        if (a == 0 || b == 0) return 0;
        a = a < 0 ? -a : a;
        b = b < 0 ? -b : b;
        return (a / _em_gcd_i64(a, b)) * b;
    }

    static inline u64 _em_lcm_u64(u64 a, u64 b) {
        if (a == 0 || b == 0) return 0;
        return (a / _em_gcd_u64(a, b)) * b;
    }

#define em_lcm(a, b) _Generic((a), \
    i32: _em_lcm_i32, \
    u32: _em_lcm_u32, \
    i64: _em_lcm_i64, \
    u64: _em_lcm_u64  \
)(a, b)

    static inline i8 _em_abs_i8(i8 x) { return x < 0 ? -x : x; }
    static inline i16 _em_abs_i16(i16 x) { return x < 0 ? -x : x; }
    static inline i32 _em_abs_i32(i32 x) { return x < 0 ? -x : x; }
    static inline i64 _em_abs_i64(i64 x) { return x < 0 ? -x : x; }
    static inline f32 _em_abs_f32(f32 x) { return fabsf(x); }
    static inline f64 _em_abs_f64(f64 x) { return fabs(x); }

#define em_abs(x) _Generic((x), \
    i8:  _em_abs_i8,  \
    i16: _em_abs_i16, \
    i32: _em_abs_i32, \
    i64: _em_abs_i64, \
    f32: _em_abs_f32, \
    f64: _em_abs_f64  \
)(x)

    static inline i32 _em_sign_i32(i32 x) { return (x > 0) - (x < 0); }
    static inline i64 _em_sign_i64(i64 x) { return (x > 0) - (x < 0); }
    static inline f32 _em_sign_f32(f32 x) { return (x > 0.0f) - (x < 0.0f); }
    static inline f64 _em_sign_f64(f64 x) { return (x > 0.0) - (x < 0.0); }

#define em_sign(x) _Generic((x), \
    i32: _em_sign_i32, \
    i64: _em_sign_i64, \
    f32: _em_sign_f32, \
    f64: _em_sign_f64  \
)(x)

#define em_swap(a, b) do { \
    typeof(a) _temp = (a); \
    (a) = (b); \
    (b) = _temp; \
} while(0)

    static inline bool _em_is_pow2_u32(u32 x) { return x != 0 && (x & (x - 1)) == 0; }
    static inline bool _em_is_pow2_u64(u64 x) { return x != 0 && (x & (x - 1)) == 0; }

#define em_is_pow2(x) _Generic((x), \
    u32: _em_is_pow2_u32, \
    u64: _em_is_pow2_u64  \
)(x)

    static inline u32 _em_next_pow2_u32(u32 x) {
        if (x == 0) return 1;
        x--;
        x |= x >> 1;
        x |= x >> 2;
        x |= x >> 4;
        x |= x >> 8;
        x |= x >> 16;
        return x + 1;
    }

    static inline u64 _em_next_pow2_u64(u64 x) {
        if (x == 0) return 1;
        x--;
        x |= x >> 1;
        x |= x >> 2;
        x |= x >> 4;
        x |= x >> 8;
        x |= x >> 16;
        x |= x >> 32;
        return x + 1;
    }

#define em_next_pow2(x) _Generic((x), \
    u32: _em_next_pow2_u32, \
    u64: _em_next_pow2_u64  \
)(x)

    static inline i32 _em_popcount_u32(u32 x) {
        x = x - ((x >> 1) & 0x55555555);
        x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
        x = (x + (x >> 4)) & 0x0F0F0F0F;
        x = x + (x >> 8);
        x = x + (x >> 16);
        return x & 0x0000003F;
    }

    static inline i32 _em_popcount_u64(u64 x) {
        x = x - ((x >> 1) & 0x5555555555555555ULL);
        x = (x & 0x3333333333333333ULL) + ((x >> 2) & 0x3333333333333333ULL);
        x = (x + (x >> 4)) & 0x0F0F0F0F0F0F0F0FULL;
        x = x + (x >> 8);
        x = x + (x >> 16);
        x = x + (x >> 32);
        return x & 0x000000000000007FULL;
    }

#define em_popcount(x) _Generic((x), \
    u32: _em_popcount_u32, \
    u64: _em_popcount_u64  \
)(x)

    static inline i32 _em_ctz_u32(u32 x) {
        if (x == 0) return 32;
        i32 n = 0;
        if ((x & 0x0000FFFF) == 0) { n += 16; x >>= 16; }
        if ((x & 0x000000FF) == 0) { n += 8;  x >>= 8; }
        if ((x & 0x0000000F) == 0) { n += 4;  x >>= 4; }
        if ((x & 0x00000003) == 0) { n += 2;  x >>= 2; }
        if ((x & 0x00000001) == 0) { n += 1; }
        return n;
    }

    static inline i32 _em_ctz_u64(u64 x) {
        if (x == 0) return 64;
        i32 n = 0;
        if ((x & 0x00000000FFFFFFFFULL) == 0) { n += 32; x >>= 32; }
        if ((x & 0x000000000000FFFFULL) == 0) { n += 16; x >>= 16; }
        if ((x & 0x00000000000000FFULL) == 0) { n += 8;  x >>= 8; }
        if ((x & 0x000000000000000FULL) == 0) { n += 4;  x >>= 4; }
        if ((x & 0x0000000000000003ULL) == 0) { n += 2;  x >>= 2; }
        if ((x & 0x0000000000000001ULL) == 0) { n += 1; }
        return n;
    }

#define em_ctz(x) _Generic((x), \
    u32: _em_ctz_u32, \
    u64: _em_ctz_u64  \
)(x)

    static inline i32 _em_clz_u32(u32 x) {
        if (x == 0) return 32;
        i32 n = 0;
        if ((x & 0xFFFF0000) == 0) { n += 16; x <<= 16; }
        if ((x & 0xFF000000) == 0) { n += 8;  x <<= 8; }
        if ((x & 0xF0000000) == 0) { n += 4;  x <<= 4; }
        if ((x & 0xC0000000) == 0) { n += 2;  x <<= 2; }
        if ((x & 0x80000000) == 0) { n += 1; }
        return n;
    }

    static inline i32 _em_clz_u64(u64 x) {
        if (x == 0) return 64;
        i32 n = 0;
        if ((x & 0xFFFFFFFF00000000ULL) == 0) { n += 32; x <<= 32; }
        if ((x & 0xFFFF000000000000ULL) == 0) { n += 16; x <<= 16; }
        if ((x & 0xFF00000000000000ULL) == 0) { n += 8;  x <<= 8; }
        if ((x & 0xF000000000000000ULL) == 0) { n += 4;  x <<= 4; }
        if ((x & 0xC000000000000000ULL) == 0) { n += 2;  x <<= 2; }
        if ((x & 0x8000000000000000ULL) == 0) { n += 1; }
        return n;
    }

#define em_clz(x) _Generic((x), \
    u32: _em_clz_u32, \
    u64: _em_clz_u64  \
)(x)

    static inline i32 _em_log2_u32(u32 x) {
        if (x == 0) return -1;
        return 31 - _em_clz_u32(x);
    }

    static inline i32 _em_log2_u64(u64 x) {
        if (x == 0) return -1;
        return 63 - _em_clz_u64(x);
    }

#define em_log2i(x) _Generic((x), \
    u32: _em_log2_u32, \
    u64: _em_log2_u64  \
)(x)

    static inline f32 _em_lerp_f32(f32 a, f32 b, f32 t) {
        return a + (b - a) * t;
    }

    static inline f64 _em_lerp_f64(f64 a, f64 b, f64 t) {
        return a + (b - a) * t;
    }

#define em_lerp(a, b, t) _Generic((a), \
    f32: _em_lerp_f32, \
    f64: _em_lerp_f64  \
)(a, b, t)

    static inline f32 _em_inv_lerp_f32(f32 a, f32 b, f32 value) {
        return (value - a) / (b - a);
    }

    static inline f64 _em_inv_lerp_f64(f64 a, f64 b, f64 value) {
        return (value - a) / (b - a);
    }

#define em_inv_lerp(a, b, value) _Generic((a), \
    f32: _em_inv_lerp_f32, \
    f64: _em_inv_lerp_f64  \
)(a, b, value)

    static inline f32 _em_remap_f32(f32 value, f32 from_min, f32 from_max, f32 to_min, f32 to_max) {
        f32 t = _em_inv_lerp_f32(from_min, from_max, value);
        return _em_lerp_f32(to_min, to_max, t);
    }

    static inline f64 _em_remap_f64(f64 value, f64 from_min, f64 from_max, f64 to_min, f64 to_max) {
        f64 t = _em_inv_lerp_f64(from_min, from_max, value);
        return _em_lerp_f64(to_min, to_max, t);
    }

#define em_remap(value, from_min, from_max, to_min, to_max) _Generic((value), \
    f32: _em_remap_f32, \
    f64: _em_remap_f64  \
)(value, from_min, from_max, to_min, to_max)

    static inline f32 _em_smoothstep_f32(f32 edge0, f32 edge1, f32 x) {
        f32 t = _em_clamp_f32((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    static inline f64 _em_smoothstep_f64(f64 edge0, f64 edge1, f64 x) {
        f64 t = _em_clamp_f64((x - edge0) / (edge1 - edge0), 0.0, 1.0);
        return t * t * (3.0 - 2.0 * t);
    }

#define em_smoothstep(edge0, edge1, x) _Generic((x), \
    f32: _em_smoothstep_f32, \
    f64: _em_smoothstep_f64  \
)(edge0, edge1, x)

    static inline f32 _em_smootherstep_f32(f32 edge0, f32 edge1, f32 x) {
        f32 t = _em_clamp_f32((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
        return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    }

    static inline f64 _em_smootherstep_f64(f64 edge0, f64 edge1, f64 x) {
        f64 t = _em_clamp_f64((x - edge0) / (edge1 - edge0), 0.0, 1.0);
        return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
    }

#define em_smootherstep(edge0, edge1, x) _Generic((x), \
    f32: _em_smootherstep_f32, \
    f64: _em_smootherstep_f64  \
)(edge0, edge1, x)

    static inline u32 _em_align_up_u32(u32 x, u32 align) {
        return (x + align - 1) & ~(align - 1);
    }

    static inline u64 _em_align_up_u64(u64 x, u64 align) {
        return (x + align - 1) & ~(align - 1);
    }

#define em_align_up(x, align) _Generic((x), \
    u32: _em_align_up_u32, \
    u64: _em_align_up_u64  \
)(x, align)

    static inline u32 _em_align_down_u32(u32 x, u32 align) {
        return x & ~(align - 1);
    }

    static inline u64 _em_align_down_u64(u64 x, u64 align) {
        return x & ~(align - 1);
    }

#define em_align_down(x, align) _Generic((x), \
    u32: _em_align_down_u32, \
    u64: _em_align_down_u64  \
)(x, align)

    static inline i32 _em_square_i32(i32 x) { return x * x; }
    static inline u32 _em_square_u32(u32 x) { return x * x; }
    static inline i64 _em_square_i64(i64 x) { return x * x; }
    static inline u64 _em_square_u64(u64 x) { return x * x; }
    static inline f32 _em_square_f32(f32 x) { return x * x; }
    static inline f64 _em_square_f64(f64 x) { return x * x; }

#define em_square(x) _Generic((x), \
    i32: _em_square_i32, \
    u32: _em_square_u32, \
    i64: _em_square_i64, \
    u64: _em_square_u64, \
    f32: _em_square_f32, \
    f64: _em_square_f64  \
)(x)

    static inline i32 _em_cube_i32(i32 x) { return x * x * x; }
    static inline u32 _em_cube_u32(u32 x) { return x * x * x; }
    static inline i64 _em_cube_i64(i64 x) { return x * x * x; }
    static inline u64 _em_cube_u64(u64 x) { return x * x * x; }
    static inline f32 _em_cube_f32(f32 x) { return x * x * x; }
    static inline f64 _em_cube_f64(f64 x) { return x * x * x; }

#define em_cube(x) _Generic((x), \
    i32: _em_cube_i32, \
    u32: _em_cube_u32, \
    i64: _em_cube_i64, \
    u64: _em_cube_u64, \
    f32: _em_cube_f32, \
    f64: _em_cube_f64  \
)(x)

    static inline f32 _em_fract_f32(f32 x) { return x - floorf(x); }
    static inline f64 _em_fract_f64(f64 x) { return x - floor(x); }

#define em_fract(x) _Generic((x), \
    f32: _em_fract_f32, \
    f64: _em_fract_f64  \
)(x)

    static inline f32 _em_mod_f32(f32 x, f32 y) {
        return x - y * floorf(x / y);
    }

    static inline f64 _em_mod_f64(f64 x, f64 y) {
        return x - y * floor(x / y);
    }

#define em_mod(x, y) _Generic((x), \
    f32: _em_mod_f32, \
    f64: _em_mod_f64  \
)(x, y)

    static inline f32 _em_wrap_f32(f32 x, f32 min, f32 max) {
        f32 range = max - min;
        return min + _em_mod_f32(x - min, range);
    }

    static inline f64 _em_wrap_f64(f64 x, f64 min, f64 max) {
        f64 range = max - min;
        return min + _em_mod_f64(x - min, range);
    }

#define em_wrap(x, min, max) _Generic((x), \
    f32: _em_wrap_f32, \
    f64: _em_wrap_f64  \
)(x, min, max)

    static inline f32 _em_radians_f32(f32 degrees) { return degrees * EM_DEG_TO_RAD32; }
    static inline f64 _em_radians_f64(f64 degrees) { return degrees * EM_DEG_TO_RAD64; }

#define em_radians(x) _Generic((x), \
    f32: _em_radians_f32, \
    f64: _em_radians_f64  \
)(x)

    static inline f32 _em_degrees_f32(f32 radians) { return radians * EM_RAD_TO_DEG32; }
    static inline f64 _em_degrees_f64(f64 radians) { return radians * EM_RAD_TO_DEG64; }

#define em_degrees(x) _Generic((x), \
    f32: _em_degrees_f32, \
    f64: _em_degrees_f64  \
)(x)

    static inline bool _em_approx_equal_f32(f32 a, f32 b, f32 epsilon) {
        return fabsf(a - b) <= epsilon;
    }

    static inline bool _em_approx_equal_f64(f64 a, f64 b, f64 epsilon) {
        return fabs(a - b) <= epsilon;
    }

#define em_approx_equal(a, b, epsilon) _Generic((a), \
    f32: _em_approx_equal_f32, \
    f64: _em_approx_equal_f64  \
)(a, b, epsilon)

    static inline f32 _em_distance_f32(f32 a, f32 b) {
        return fabsf(b - a);
    }

    static inline f64 _em_distance_f64(f64 a, f64 b) {
        return fabs(b - a);
    }

#define em_distance(a, b) _Generic((a), \
    f32: _em_distance_f32, \
    f64: _em_distance_f64  \
)(a, b)

    static inline f32 _em_step_f32(f32 edge, f32 x) {
        return x < edge ? 0.0f : 1.0f;
    }

    static inline f64 _em_step_f64(f64 edge, f64 x) {
        return x < edge ? 0.0 : 1.0;
    }

#define em_step(edge, x) _Generic((x), \
    f32: _em_step_f32, \
    f64: _em_step_f64  \
)(edge, x)

    static inline f32 _em_ease_in_quad_f32(f32 t) { return t * t; }
    static inline f32 _em_ease_out_quad_f32(f32 t) { return t * (2.0f - t); }
    static inline f32 _em_ease_in_out_quad_f32(f32 t) {
        return t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t;
    }

    static inline f64 _em_ease_in_quad_f64(f64 t) { return t * t; }
    static inline f64 _em_ease_out_quad_f64(f64 t) { return t * (2.0 - t); }
    static inline f64 _em_ease_in_out_quad_f64(f64 t) {
        return t < 0.5 ? 2.0 * t * t : -1.0 + (4.0 - 2.0 * t) * t;
    }

#define em_ease_in_quad(t) _Generic((t), f32: _em_ease_in_quad_f32, f64: _em_ease_in_quad_f64)(t)
#define em_ease_out_quad(t) _Generic((t), f32: _em_ease_out_quad_f32, f64: _em_ease_out_quad_f64)(t)
#define em_ease_in_out_quad(t) _Generic((t), f32: _em_ease_in_out_quad_f32, f64: _em_ease_in_out_quad_f64)(t)

    static inline bool _em_in_range_i32(i32 x, i32 min, i32 max) { return x >= min && x <= max; }
    static inline bool _em_in_range_u32(u32 x, u32 min, u32 max) { return x >= min && x <= max; }
    static inline bool _em_in_range_f32(f32 x, f32 min, f32 max) { return x >= min && x <= max; }
    static inline bool _em_in_range_f64(f64 x, f64 min, f64 max) { return x >= min && x <= max; }

#define em_in_range(x, min, max) _Generic((x), \
    i32: _em_in_range_i32, \
    u32: _em_in_range_u32, \
    f32: _em_in_range_f32, \
    f64: _em_in_range_f64  \
)(x, min, max)

#ifdef __cplusplus
}
#endif

#endif // EDGE_MATH_H