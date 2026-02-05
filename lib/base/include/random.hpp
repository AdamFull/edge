#ifndef EDGE_RANDOM_H
#define EDGE_RANDOM_H

#include "stddef.hpp"

#include <array>
#include <bit>
#include <cmath>
#include <type_traits>

namespace edge {
struct RngPCG {
  u64 state = 0;
  u64 inc = 0;

  constexpr void seed(u64 seed_val) noexcept {
    state = 0;
    inc = (seed_val << 1) | 1;
    state = state * 6364136223846793005ULL + inc;
    state += seed_val;
    state = state * 6364136223846793005ULL + inc;
  }

  constexpr u32 next32() noexcept {
    u64 oldstate = state;
    state = oldstate * 6364136223846793005ULL + inc;
    u32 xorshifted = (u32)(((oldstate >> 18u) ^ oldstate) >> 27u);
    u32 rot = (u32)(oldstate >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((-(i32)rot) & 31));
  }

  constexpr u64 next64() noexcept {
    return (static_cast<u64>(next32()) << 32) | next32();
  }
};

struct RngXoshiro256 {
  u64 s[4] = {};
  static constexpr u64 jmpvals[] = {
      0x180ec6d33cfd0abaULL, 0xd5a61266f0c9392cULL, 0xa9582618e03fc9aaULL,
      0x39abdc4529b1661cULL};

  constexpr void seed(u64 seed_val) noexcept {
    u64 z = seed_val;
    for (i32 i = 0; i < 4; i++) {
      z += 0x9e3779b97f4a7c15ULL;
      u64 t = z;
      t = (t ^ (t >> 30)) * 0xbf58476d1ce4e5b9ULL;
      t = (t ^ (t >> 27)) * 0x94d049bb133111ebULL;
      s[i] = t ^ (t >> 31);
    }
  }

  constexpr u64 next64() noexcept {
    u64 result = std::rotl(s[1] * 5, 7) * 9;
    u64 t = s[1] << 17;

    s[2] ^= s[0];
    s[3] ^= s[1];
    s[1] ^= s[2];
    s[0] ^= s[3];
    s[2] ^= t;
    s[3] = std::rotl(s[3], 45);

    return result;
  }

  constexpr u32 next32() noexcept { return static_cast<u32>(next64()); }

  constexpr void jump() noexcept {
    u64 s0 = 0, s1 = 0, s2 = 0, s3 = 0;
    for (i32 i = 0; i < 4; i++) {
      for (i32 b = 0; b < 64; b++) {
        if (jmpvals[i] & (1ULL << b)) {
          s0 ^= s[0];
          s1 ^= s[1];
          s2 ^= s[2];
          s3 ^= s[3];
        }
        next64();
      }
    }
    s[0] = s0;
    s[1] = s1;
    s[2] = s2;
    s[3] = s3;
  }
};

struct RngSplitMix64 {
  u64 state = 0;

  constexpr void seed(u64 seed_val) noexcept { state = seed_val; }

  constexpr u64 next64() noexcept {
    u64 z = (state += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
  }

  constexpr u32 next32() noexcept { return static_cast<u32>(next64()); }
};

template <typename T>
concept RngAlgorithm = requires(T rng, u64 seed) {
  { rng.seed(seed) } -> std::same_as<void>;
  { rng.next32() } -> std::same_as<u32>;
  { rng.next64() } -> std::same_as<u64>;
};

template <RngAlgorithm Algorithm>
inline constexpr u32 rng_gen_u32_bounded(Algorithm &state, u32 bound) noexcept {
  if (bound == 0) {
    return 0;
  }

  auto threshold = (u32)(-bound) % bound;
  for (;;) {
    auto r = state.next32();
    auto m = static_cast<u64>(r) * static_cast<u64>(bound);
    auto l = static_cast<u32>(m);
    if (l >= threshold) {
      return static_cast<u32>(m >> 32);
    }
  }
}

template <RngAlgorithm Algorithm>
inline constexpr i32 rng_gen_i32_range(Algorithm &state, i32 min_val,
                                       i32 max_val) noexcept {
  if (min_val > max_val) {
    i32 temp = min_val;
    min_val = max_val;
    max_val = temp;
  }
  u32 range = static_cast<u32>(max_val - min_val + 1);
  return static_cast<i32>(rng_gen_u32_bounded(state, range)) + min_val;
}

template <RngAlgorithm Algorithm>
inline constexpr f32 rng_gen_f32(Algorithm &state) noexcept {
  auto r = state.next32() >> 8;
  return static_cast<f32>(r) * (1.0f / 16777216.0f);
}

template <RngAlgorithm Algorithm>
inline constexpr f32 rng_gen_f32_range(Algorithm &state, f32 min_val,
                                       f32 max_val) noexcept {
  return min_val + state.next32() * (max_val - min_val);
}

template <RngAlgorithm Algorithm>
inline f32 rng_gen_normal_f32(Algorithm &state, f32 mean = 0.0f,
                              f32 stddev = 1.0f) noexcept {
  static thread_local bool has_spare = false;
  static thread_local f32 spare;

  if (has_spare) {
    has_spare = false;
    return mean + stddev * spare;
  }

  f32 u, v, s;
  do {
    u = rng_gen_f32_range(state, -1.0f, 1.0f);
    v = rng_gen_f32_range(state, -1.0f, 1.0f);
    s = u * u + v * v;
  } while (s >= 1.0f || s == 0.0f);

  s = std::sqrt(-2.0f * std::log(s) / s);
  spare = v * s;
  has_spare = true;

  return mean + stddev * u * s;
}

template <RngAlgorithm Algorithm>
inline f32 gen_exp_f32(Algorithm &state, f32 lambda = 1.0f) noexcept {
  if (lambda <= 0.0f) {
    return 0.0f;
  }
  return -std::log(1.0f - rng_gen_f32(state)) / lambda;
}

template <RngAlgorithm Algorithm>
inline constexpr u64 rng_gen_u64_bounded(Algorithm &state, u64 bound) noexcept {
  if (bound == 0) {
    return 0;
  }
  return state.next64() % bound;
}

template <RngAlgorithm Algorithm>
inline constexpr i64 rng_gen_i64_range(Algorithm &state, i64 min_val,
                                       i64 max_val) {
  if (min_val > max_val) {
    i64 temp = min_val;
    min_val = max_val;
    max_val = temp;
  }
  auto range = static_cast<u64>(max_val - min_val + 1);
  return static_cast<i64>(rng_gen_u64_bounded(state, range)) + min_val;
}

template <RngAlgorithm Algorithm>
inline constexpr f64 rng_gen_f64(Algorithm &state) noexcept {
  auto r = state.next64() >> 11;
  return static_cast<f64>(r) * (1.0 / 9007199254740992.0);
}

template <RngAlgorithm Algorithm>
inline constexpr f64 rng_gen_f64_range(Algorithm &state, f64 min_val,
                                       f64 max_val) noexcept {
  return min_val + rng_gen_f64(state) * (max_val - min_val);
}

template <RngAlgorithm Algorithm>
inline f64 rng_gen_normal_f64(Algorithm &state, f64 mean = 0.0,
                              f64 stddev = 1.0) noexcept {
  static thread_local bool has_spare = false;
  static thread_local f64 spare;

  if (has_spare) {
    has_spare = false;
    return mean + stddev * spare;
  }

  f64 u, v, s;
  do {
    u = rng_gen_f64_range(state, -1.0, 1.0);
    v = rng_gen_f64_range(state, -1.0, 1.0);
    s = u * u + v * v;
  } while (s >= 1.0 || s == 0.0);

  s = std::sqrt(-2.0 * std::log(s) / s);
  spare = v * s;
  has_spare = true;

  return mean + stddev * u * s;
}

template <RngAlgorithm Algorithm>
inline f64 rng_gen_exp_f64(Algorithm &state, f64 lambda = 1.0) noexcept {
  if (lambda <= 0.0) {
    return 0.0;
  }
  return -std::log(1.0 - rng_gen_f64(state)) / lambda;
}

template <RngAlgorithm Algorithm>
inline constexpr bool rng_gen_bool(Algorithm &state,
                                   f32 probability = 0.5f) noexcept {
  return rng_gen_f32(state) < probability;
}

template <RngAlgorithm Algorithm, TrivialType T>
inline constexpr void rng_shuffle(Algorithm &state, T *array,
                                  usize count) noexcept {
  if (!array || count == 0) {
    return;
  }

  for (usize i = count - 1; i > 0; i--) {
    usize j = rng_gen_u32_bounded(state, static_cast<u32>(i + 1));
    auto temp = array[j];
    array[j] = array[i];
    array[i] = temp;
  }
}

template <RngAlgorithm Algorithm, TrivialType T>
inline constexpr T rng_choice(Algorithm &state, const T *array,
                              usize count) noexcept {
  if (!array || count == 0) {
    return T{};
  }

  usize index = rng_gen_u32_bounded(state, static_cast<u32>(count));
  return array[index];
}

template <RngAlgorithm Algorithm, TrivialType T>
inline constexpr T *rng_choice_ptr(Algorithm &state, T *array,
                                   usize count) noexcept {
  if (!array || count == 0) {
    return nullptr;
  }
  usize index = rng_gen_u32_bounded(state, static_cast<u32>(count));
  return &array[index];
}

template <RngAlgorithm Algorithm>
inline constexpr void rng_gen_bytes(Algorithm &state, void *buffer,
                                    usize size) noexcept {
  if (!buffer || size == 0)
    return;

  u8 *bytes = static_cast<u8 *>(buffer);
  usize i = 0;

  while (i + 8 <= size) {
    auto val = state.next64();
    memcpy(bytes + i, &val, 8);
    i += 8;
  }

  if (i < size) {
    auto val = state.next64();
    memcpy(bytes + i, &val, size - i);
  }
}

template <RngAlgorithm Algorithm> void rng_seed_entropy(Algorithm &state);

template <RngAlgorithm Algorithm>
void rng_seed_entropy_secure(Algorithm &state);
} // namespace edge

#endif