#ifndef EDGE_RANDOM_H
#define EDGE_RANDOM_H

#include "stddef.hpp"

namespace edge {
	struct RngPCG {
		u64 state;
		u64 inc;
	};

	struct RngXoshiro256 {
		u64 s[4];
	};

	struct RngSplitMix64 {
		u64 state;
	};

	enum class RngAlgorithm {
		PCG,
		Xoshiro256,
		SplitMix64
	};

	struct Rng {
		RngAlgorithm algorithm;
		union {
			RngPCG pcg;
			RngXoshiro256 xoshiro256;
			RngSplitMix64 splitmix64;
		} state;
	};

	void rng_create(Rng* rng, RngAlgorithm algorithm, u64 seed);
	void rng_seed(Rng* rng, u64 seed);
	void rng_seed_entropy(Rng* rng);
	void rng_seed_entropy_secure(Rng* rng);

	u32 rng_u32(Rng* rng);
	u32 rng_u32_bounded(Rng* rng, u32 bound);
	i32 rng_i32_range(Rng* rng, i32 min_val, i32 max_val);
	u64 rng_u64(Rng* rng);
	u64 rng_u64_bounded(Rng* rng, u64 bound);
	i64 rng_i64_range(Rng* rng, i64 min_val, i64 max_val);
	f32 rng_f32(Rng* rng);
	f32 rng_f32_range(Rng* rng, f32 min_val, f32 max_val);
	f64 rng_f64(Rng* rng);
	f64 rng_f64_range(Rng* rng, f64 min_val, f64 max_val);
	bool rng_bool(Rng* rng, f32 probability = 0.5f);

	f32 rng_normal_f32(Rng* rng, f32 mean = 0.0f, f32 stddev = 1.0f);
	f64 rng_normal_f64(Rng* rng, f64 mean = 0.0, f64 stddev = 1.0);
	f32 rng_exp_f32(Rng* rng, f32 lambda = 1.0f);
	f64 rng_exp_f64(Rng* rng, f64 lambda = 1.0);

	template<TrivialType T>
	inline void rng_shuffle(Rng* rng, T* array, usize count) {
		if (!rng || !array || count == 0) {
			return;
		}

		for (usize i = count - 1; i > 0; i--) {
			usize j = rng_u32_bounded(rng, (u32)(i + 1));
			T temp = array[j];
			array[j] = array[i];
			array[i] = temp;
		}
	}

	template<TrivialType T>
	inline T rng_choice(Rng* rng, const T* array, usize count) {
		if (!rng || !array || count == 0) {
			return T{};
		}

		usize index = rng_u32_bounded(rng, (u32)count);
		return array[index];
	}

	template<TrivialType T>
	inline T* rng_choice_ptr(Rng* rng, T* array, usize count) {
		if (!rng || !array || count == 0) {
			return nullptr;
		}

		usize index = rng_u32_bounded(rng, (u32)count);
		return &array[index];
	}

	void rng_bytes(Rng* rng, void* buffer, usize size);
}

#endif