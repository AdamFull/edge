#ifndef SHRD_INTEROP_H
#define SHRD_INTEROP_H

#define SAMPLER_SLOT 0
#define SRV_TEXTURE_SLOT 1
#define UAV_TEXTURE_SLOT 2

#define MAX_SAMPLER_SLOTS 65535
#define MAX_SRV_TEXTURE_SLOTS 65535
#define MAX_UAV_TEXTURE_SLOTS 65535

#ifdef __cplusplus
#include <glm/glm.hpp>
#include <cstdint>
using namespace glm;

#define SHDR_NAMESPACE_BEGIN namespace {
#define SHDR_NAMESPACE_END }
#define SHDR_CONST constexpr
#else
#define SHDR_NAMESPACE_BEGIN
#define SHDR_NAMESPACE_END

#endif

#ifdef __cplusplus

#include <glm/glm.hpp>
#include <glm/packing.hpp>
#include <glm/gtx/compatibility.hpp>

using int8_t2 = glm::i8vec2;
using int8_t3 = glm::i8vec3;
using int8_t4 = glm::i8vec4;

using uint8_t2 = glm::u8vec2;
using uint8_t3 = glm::u8vec3;
using uint8_t4 = glm::u8vec4;

using int16_t2 = glm::i16vec2;
using int16_t3 = glm::i16vec3;
using int16_t4 = glm::i16vec4;

using uint16_t2 = glm::u16vec2;
using uint16_t3 = glm::u16vec3;
using uint16_t4 = glm::u16vec4;

using int32_t2 = glm::i32vec2;
using int32_t3 = glm::i32vec3;
using int32_t4 = glm::i32vec4;

using uint32_t2 = glm::u32vec2;
using uint32_t3 = glm::u32vec3;
using uint32_t4 = glm::u32vec4;

using int64_t2 = glm::i64vec2;
using int64_t3 = glm::i64vec3;
using int64_t4 = glm::i64vec4;

using uint64_t2 = glm::u64vec2;
using uint64_t3 = glm::u64vec3;
using uint64_t4 = glm::u64vec4;

using float2 = glm::vec2;
using float3 = glm::vec3;
using float4 = glm::vec4;

using double2 = glm::dvec2;
using double3 = glm::dvec3;
using double4 = glm::dvec4;

using float2x2 = glm::mat2;
using float2x3 = glm::mat2x3;
using float2x4 = glm::mat2x4;

using float3x2 = glm::mat3x2;
using float3x3 = glm::mat3;
using float3x4 = glm::mat3x4;

using float4x2 = glm::mat4x2;
using float4x3 = glm::mat4x3;
using float4x4 = glm::mat4;

namespace edge::gfx {
	using namespace glm;

	template<typename _Ty>
	inline _Ty select(bool cond, _Ty l, _Ty r) {
		return cond ? l : r;
	}

	template<typename _Ty, typename _Tty>
	inline auto mul(const _Ty& a, const _Tty& b) -> decltype(a* b) {
		return a * b;
	}

	template<typename _Ty>
	struct GPUPointer {
		GPUPointer(uint64_t addr = ~0ull) 
			: address_{ addr } {
		}

		auto operator[](size_t idx) const -> const _Ty& {
			static _Ty t{};
			return t;
		};

		operator bool() const {
			return address_ != ~0ull;
		}

		uint64_t address_{ ~0ull };
	};
}

#define TYPE_PTR(type) edge::gfx::GPUPointer<type>
#define CONSTRUCTOR(type, ...) type(__VA_ARGS__)
#define AS_IN(type) const type&
#define AS_OUT(type) type&
#define AS_INOUT(type) type&
#elif __SLANG__
#define TYPE_PTR(type) type*
#define CONSTRUCTOR(type, ...) __init(__VA_ARGS__)
#define AS_IN(type) in type
#define AS_OUT(type) out type
#define AS_INOUT(type) inout type
#endif

namespace edge::gfx {
	inline uint8_t4 encode_color(AS_IN(float4) c) {
		return uint8_t4(clamp(round(c * 255.0f), 0.0f, 255.0f));
	}

	inline float4 decode_color(AS_IN(uint8_t4) c) {
		return float4(c) / 255.0f;
	}
}

#endif