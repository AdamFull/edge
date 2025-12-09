#ifndef EDGE_MATH_H
#define EDGE_MATH_H

#include "stddef.hpp"

#include <cmath>

namespace edge {
	inline constexpr f32 PI_F32 = 3.14159265358979323846f;
	inline constexpr f64 PI_F64 = 3.14159265358979323846;
	inline constexpr f32 TAU_F32 = 6.28318530717958647692f;
	inline constexpr f64 TAU_F64 = 6.28318530717958647692;
	inline constexpr f32 E_F32 = 2.71828182845904523536f;
	inline constexpr f64 E_F64 = 2.71828182845904523536;

	inline constexpr f32 DEG_TO_RAD_F32 = 0.01745329251f;
	inline constexpr f32 RAD_TO_DEG_F32 = 57.2957795131f;
	inline constexpr f64 DEG_TO_RAD_F64 = 0.01745329251994329576923690768489;
	inline constexpr f64 RAD_TO_DEG_F64 = 57.295779513082320876798154814105;

	inline constexpr f32 EPSILON_F32 = 1.192092896e-07f;
	inline constexpr f64 EPSILON_F64 = 2.2204460492503131e-16;

	template<Arithmetic T>
	constexpr T min(T a, T b) {
		return a < b ? a : b;
	}

	template<Arithmetic T>
	constexpr T max(T a, T b) {
		return a > b ? a : b;
	}

	template<Arithmetic T>
	constexpr T clamp(T x, T low, T high) {
		return x < low ? low : (x > high ? high : x);
	}

	template<FloatingPoint T>
	constexpr T clamp01(T x) {
		return clamp(x, T(0), T(1));
	}

	template<std::integral T>
	constexpr T gcd(T a, T b) {
		if constexpr (std::is_signed_v<T>) {
			a = a < 0 ? -a : a;
			b = b < 0 ? -b : b;
		}
		while (b != 0) {
			T temp = b;
			b = a % b;
			a = temp;
		}
		return a;
	}

	template<std::integral T>
	constexpr T lcm(T a, T b) {
		if (a == 0 || b == 0) {
			return 0;
		}
		if constexpr (std::is_signed_v<T>) {
			a = a < 0 ? -a : a;
			b = b < 0 ? -b : b;
		}
		return (a / gcd(a, b)) * b;
	}

	template<SignedArithmetic T>
	constexpr T abs(T x) {
		if constexpr (std::is_floating_point_v<T>) {
			return std::abs(x);
		}
		else {
			return x < 0 ? -x : x;
		}
	}

	template<Arithmetic T>
	constexpr T sign(T x) {
		if constexpr (std::is_floating_point_v<T>) {
			return T((x > T(0)) - (x < T(0)));
		}
		else {
			return T((x > 0) - (x < 0));
		}
	}

	template<typename T>
	constexpr void swap(T& a, T& b) {
		T temp = a;
		a = b;
		b = temp;
	}

	template<UnsignedArithmetic T>
	constexpr bool is_pow2(T x) {
		return x != 0 && (x & (x - 1)) == 0;
	}

	template<UnsignedArithmetic T>
	constexpr T next_pow2(T x) {
		if (x == 0) {
			return 1;
		}
		x--;
		x |= x >> 1;
		x |= x >> 2;
		x |= x >> 4;
		x |= x >> 8;
		x |= x >> 16;
		if constexpr (sizeof(T) == 8) {
			x |= x >> 32;
		}
		return x + 1;
	}

	template<UnsignedArithmetic T>
	constexpr T prev_pow2(T x) {
		if (x == 0) {
			return 0;
		}
		x |= x >> 1;
		x |= x >> 2;
		x |= x >> 4;
		x |= x >> 8;
		x |= x >> 16;
		if constexpr (sizeof(T) == 8) {
			x |= x >> 32;
		}
		return x - (x >> 1);
	}

	template<UnsignedArithmetic T>
	constexpr i32 popcount(T x) {
		i32 count = 0;
		while (x) {
			count += x & 1;
			x >>= 1;
		}
		return count;
	}

	template<UnsignedArithmetic T>
	constexpr i32 clz(T x) {
		if (x == 0) {
			return sizeof(T) * 8;
		}
		i32 count = 0;
		T msb = T(1) << (sizeof(T) * 8 - 1);
		while ((x & msb) == 0) {
			count++;
			x <<= 1;
		}
		return count;
	}

	template<UnsignedArithmetic T>
	constexpr i32 ctz(T x) {
		if (x == 0) {
			return sizeof(T) * 8;
		}
		i32 count = 0;
		while ((x & 1) == 0) {
			count++;
			x >>= 1;
		}
		return count;
	}

	template<FloatingPoint T>
	constexpr T lerp(T a, T b, T t) {
		return a + (b - a) * t;
	}

	template<FloatingPoint T>
	constexpr T inv_lerp(T a, T b, T value) {
		return (value - a) / (b - a);
	}

	template<FloatingPoint T>
	constexpr T remap(T value, T from_min, T from_max, T to_min, T to_max) {
		T t = inv_lerp(from_min, from_max, value);
		return lerp(to_min, to_max, t);
	}

	template<FloatingPoint T>
	constexpr T smoothstep(T edge0, T edge1, T x) {
		T t = clamp((x - edge0) / (edge1 - edge0), T(0), T(1));
		return t * t * (T(3) - T(2) * t);
	}

	template<FloatingPoint T>
	constexpr T smootherstep(T edge0, T edge1, T x) {
		T t = clamp((x - edge0) / (edge1 - edge0), T(0), T(1));
		return t * t * t * (t * (t * T(6) - T(15)) + T(10));
	}

	template<UnsignedArithmetic T>
	constexpr T align_up(T x, T align) {
		return (x + align - 1) & ~(align - 1);
	}

	template<UnsignedArithmetic T>
	constexpr T align_down(T x, T align) {
		return x & ~(align - 1);
	}

	template<Arithmetic T>
	constexpr T square(T x) {
		return x * x;
	}

	template<Arithmetic T>
	constexpr T cube(T x) {
		return x * x * x;
	}

	template<FloatingPoint T>
	inline T fract(T x) {
		return x - std::floor(x);
	}

	template<FloatingPoint T>
	inline T mod(T x, T y) {
		return x - y * std::floor(x / y);
	}

	template<FloatingPoint T>
	inline T wrap(T x, T min_val, T max_val) {
		T range = max_val - min_val;
		return min_val + mod(x - min_val, range);
	}

	template<FloatingPoint T>
	constexpr T radians(T degrees) {
		if constexpr (std::is_same_v<T, f32>) {
			return degrees * DEG_TO_RAD_F32;
		}
		else {
			return degrees * DEG_TO_RAD_F64;
		}
	}

	template<FloatingPoint T>
	constexpr T degrees(T radians) {
		if constexpr (std::is_same_v<T, f32>) {
			return radians * RAD_TO_DEG_F32;
		}
		else {
			return radians * RAD_TO_DEG_F64;
		}
	}

	template<FloatingPoint T>
	constexpr bool approx_equal(T a, T b, T epsilon) {
		return abs(a - b) <= epsilon;
	}

	template<FloatingPoint T>
	constexpr bool approx_equal(T a, T b) {
		if constexpr (std::is_same_v<T, f32>) {
			return approx_equal(a, b, EPSILON_F32);
		}
		else {
			return approx_equal(a, b, EPSILON_F64);
		}
	}

	template<FloatingPoint T>
	constexpr T distance(T a, T b) {
		return abs(b - a);
	}

	template<FloatingPoint T>
	constexpr T step(T edge, T x) {
		return x < edge ? T(0) : T(1);
	}

	template<FloatingPoint T>
	constexpr T ease_in_quad(T t) {
		return t * t;
	}

	template<FloatingPoint T>
	constexpr T ease_out_quad(T t) {
		return t * (T(2) - t);
	}

	template<FloatingPoint T>
	constexpr T ease_in_out_quad(T t) {
		return t < T(0.5) ? T(2) * t * t : T(-1) + (T(4) - T(2) * t) * t;
	}

	template<FloatingPoint T>
	constexpr T ease_in_cubic(T t) {
		return t * t * t;
	}

	template<FloatingPoint T>
	constexpr T ease_out_cubic(T t) {
		T t1 = t - T(1);
		return t1 * t1 * t1 + T(1);
	}

	template<FloatingPoint T>
	constexpr T ease_in_out_cubic(T t) {
		return t < T(0.5) ? T(4) * t * t * t : T(1) + (t - T(1)) * (T(2) * (t - T(1))) * (T(2) * (t - T(1)));
	}

	template<Arithmetic T>
	constexpr bool in_range(T x, T min_val, T max_val) {
		return x >= min_val && x <= max_val;
	}

	template<std::integral T>
	constexpr T floor(T x) {
		return x;
	}

	template<std::integral T>
	constexpr T ceil(T x) {
		return x;
	}

	template<std::integral T>
	constexpr T round(T x) {
		return x;
	}

	template<FloatingPoint T>
	inline T floor(T x) {
		return std::floor(x);
	}

	template<FloatingPoint T>
	inline T ceil(T x) {
		return std::ceil(x);
	}

	template<FloatingPoint T>
	inline T round(T x) {
		return std::round(x);
	}

	template<FloatingPoint T>
	inline T sqrt(T x) {
		return std::sqrt(x);
	}

	template<FloatingPoint T>
	inline T pow(T base, T exp) {
		return std::pow(base, exp);
	}

	template<FloatingPoint T>
	inline T sin(T x) {
		return std::sin(x);
	}

	template<FloatingPoint T>
	inline T cos(T x) {
		return std::cos(x);
	}

	template<FloatingPoint T>
	inline T tan(T x) {
		return std::tan(x);
	}

	template<FloatingPoint T>
	inline T asin(T x) {
		return std::asin(x);
	}

	template<FloatingPoint T>
	inline T acos(T x) {
		return std::acos(x);
	}

	template<FloatingPoint T>
	inline T atan(T x) {
		return std::atan(x);
	}

	template<FloatingPoint T>
	inline T atan2(T y, T x) {
		return std::atan2(y, x);
	}

	template<FloatingPoint T>
	inline T exp(T x) {
		return std::exp(x);
	}

	template<FloatingPoint T>
	inline T log(T x) {
		return std::log(x);
	}

	template<FloatingPoint T>
	inline T log10(T x) {
		return std::log10(x);
	}

	template<FloatingPoint T>
	inline T log2(T x) {
		return std::log2(x);
	}
}

#endif
