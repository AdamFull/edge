#include "random.hpp"
#include "platform_detect.hpp"

#include <cmath>
#include <cstring>
#include <ctime>

#if EDGE_HAS_WINDOWS_API
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#elif EDGE_PLATFORM_POSIX
#include <unistd.h>
#include <sys/time.h>
#else
#error "Unsupported platform"
#endif

namespace edge {
	namespace detail {
		inline u64 get_time_seed() {
#if EDGE_HAS_WINDOWS_API
			LARGE_INTEGER counter;
			QueryPerformanceCounter(&counter);
			return (u64)counter.QuadPart;
#elif EDGE_PLATFORM_POSIX
			struct timeval tv;
			gettimeofday(&tv, nullptr);
			return (u64)tv.tv_sec * 1000000ULL + (u64)tv.tv_usec;
#else
#error "Unsupported platform"
#endif
		}

		inline bool get_system_entropy(void* buffer, usize size) {
#if EDGE_HAS_WINDOWS_API
			return BCryptGenRandom(nullptr, (PUCHAR)buffer, (ULONG)size, BCRYPT_USE_SYSTEM_PREFERRED_RNG) >= 0;
#elif EDGE_PLATFORM_POSIX
			FILE* urandom = fopen("/dev/urandom", "rb");
			if (!urandom) {
				return false;
			}

			usize read = fread(buffer, 1, size, urandom);
			fclose(urandom);
			return read == size;
#else
#error "Unsupported platform"
#endif
		}

		inline u64 mix_entropy(u64 a, u64 b, u64 c) {
			a = a ^ (b << 13) ^ (c >> 7);
			b = b ^ (c << 17) ^ (a >> 11);
			c = c ^ (a << 5) ^ (b >> 23);
			return a ^ b ^ c;
		}

		inline u64 rotl64(u64 x, i32 k) {
			return (x << k) | (x >> (64 - k));
		}

		// PCG implementation
		inline void pcg_seed(RngPCG* pcg, u64 seed) {
			pcg->state = 0;
			pcg->inc = (seed << 1) | 1;
			pcg->state = pcg->state * 6364136223846793005ULL + pcg->inc;
			pcg->state += seed;
			pcg->state = pcg->state * 6364136223846793005ULL + pcg->inc;
		}

		inline u32 pcg_next(RngPCG* pcg) {
			u64 oldstate = pcg->state;
			pcg->state = oldstate * 6364136223846793005ULL + pcg->inc;
			u32 xorshifted = (u32)(((oldstate >> 18u) ^ oldstate) >> 27u);
			u32 rot = (u32)(oldstate >> 59u);
			return (xorshifted >> rot) | (xorshifted << ((-(i32)rot) & 31));
		}

		// Xoshiro256** implementation
		inline void xoshiro256_seed(RngXoshiro256* xs, u64 seed) {
			// Use SplitMix64 to generate initial state
			u64 z = seed;
			for (i32 i = 0; i < 4; i++) {
				z += 0x9e3779b97f4a7c15ULL;
				u64 t = z;
				t = (t ^ (t >> 30)) * 0xbf58476d1ce4e5b9ULL;
				t = (t ^ (t >> 27)) * 0x94d049bb133111ebULL;
				xs->s[i] = t ^ (t >> 31);
			}
		}

		inline u64 xoshiro256_next(RngXoshiro256* xs) {
			u64 result = rotl64(xs->s[1] * 5, 7) * 9;
			u64 t = xs->s[1] << 17;

			xs->s[2] ^= xs->s[0];
			xs->s[3] ^= xs->s[1];
			xs->s[1] ^= xs->s[2];
			xs->s[0] ^= xs->s[3];
			xs->s[2] ^= t;
			xs->s[3] = rotl64(xs->s[3], 45);

			return result;
		}

		inline void xoshiro256_jump(RngXoshiro256* xs) {
			static const u64 JUMP[] = {
				0x180ec6d33cfd0abaULL, 0xd5a61266f0c9392cULL,
				0xa9582618e03fc9aaULL, 0x39abdc4529b1661cULL
			};

			u64 s0 = 0, s1 = 0, s2 = 0, s3 = 0;
			for (i32 i = 0; i < 4; i++) {
				for (i32 b = 0; b < 64; b++) {
					if (JUMP[i] & (1ULL << b)) {
						s0 ^= xs->s[0];
						s1 ^= xs->s[1];
						s2 ^= xs->s[2];
						s3 ^= xs->s[3];
					}
					xoshiro256_next(xs);
				}
			}
			xs->s[0] = s0;
			xs->s[1] = s1;
			xs->s[2] = s2;
			xs->s[3] = s3;
		}

		// SplitMix64 implementation
		inline void splitmix64_seed(RngSplitMix64* sm, u64 seed) {
			sm->state = seed;
		}

		inline u64 splitmix64_next(RngSplitMix64* sm) {
			u64 z = (sm->state += 0x9e3779b97f4a7c15ULL);
			z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
			z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
			return z ^ (z >> 31);
		}
	}

	void rng_create(Rng* rng, RngAlgorithm algorithm, u64 seed) {
		if (!rng) {
			return;
		}

		rng->algorithm = algorithm;
		rng_seed(rng, seed);
	}

	void rng_seed(Rng* rng, u64 seed) {
		if (!rng) {
			return;
		}

		switch (rng->algorithm) {
		case RngAlgorithm::PCG:
			detail::pcg_seed(&rng->state.pcg, seed);
			break;
		case RngAlgorithm::Xoshiro256:
			detail::xoshiro256_seed(&rng->state.xoshiro256, seed);
			break;
		case RngAlgorithm::SplitMix64:
			detail::splitmix64_seed(&rng->state.splitmix64, seed);
			break;
		}
	}

	void rng_seed_entropy(Rng* rng) {
		if (!rng) {
			return;
		}

		u64 seed = detail::get_time_seed();
		seed ^= (u64)clock();
		seed ^= (u64)(uintptr_t)rng;
		rng_seed(rng, seed);
	}

	void rng_seed_entropy_secure(Rng* rng) {
		if (!rng) {
			return;
		}

		u64 entropy[4];
		if (detail::get_system_entropy(entropy, sizeof(entropy))) {
			u64 seed = detail::mix_entropy(entropy[0], entropy[1], entropy[2]) ^ entropy[3];
			rng_seed(rng, seed);
		}
		else {
			rng_seed_entropy(rng);
		}
	}

	u32 rng_u32(Rng* rng) {
		if (!rng) {
			return 0;
		}

		switch (rng->algorithm) {
		case RngAlgorithm::PCG:
			return detail::pcg_next(&rng->state.pcg);
		case RngAlgorithm::Xoshiro256:
			return (u32)detail::xoshiro256_next(&rng->state.xoshiro256);
		case RngAlgorithm::SplitMix64:
			return (u32)detail::splitmix64_next(&rng->state.splitmix64);
		}

		return 0;
	}

	u32 rng_u32_bounded(Rng* rng, u32 bound) {
		if (!rng || bound == 0) {
			return 0;
		}

		u32 threshold = (u32)(-bound) % bound;
		for (;;) {
			u32 r = rng_u32(rng);
			u64 m = (u64)r * (u64)bound;
			u32 l = (u32)m;
			if (l < threshold) {
				continue;
			}
			return (u32)(m >> 32);
		}
	}

	i32 rng_i32_range(Rng* rng, i32 min_val, i32 max_val) {
		if (!rng) {
			return 0;
		}

		if (min_val > max_val) {
			i32 temp = min_val;
			min_val = max_val;
			max_val = temp;
		}
		u32 range = (u32)(max_val - min_val + 1);
		return (i32)rng_u32_bounded(rng, range) + min_val;
	}

	u64 rng_u64(Rng* rng) {
		if (!rng) {
			return 0;
		}

		switch (rng->algorithm) {
		case RngAlgorithm::PCG:
			return ((u64)detail::pcg_next(&rng->state.pcg) << 32) | detail::pcg_next(&rng->state.pcg);
		case RngAlgorithm::Xoshiro256:
			return detail::xoshiro256_next(&rng->state.xoshiro256);
		case RngAlgorithm::SplitMix64:
			return detail::splitmix64_next(&rng->state.splitmix64);
		}
		return 0;
	}

	u64 rng_u64_bounded(Rng* rng, u64 bound) {
		if (!rng || bound == 0) {
			return 0;
		}

		return rng_u64(rng) % bound;
	}

	i64 rng_i64_range(Rng* rng, i64 min_val, i64 max_val) {
		if (!rng) {
			return 0;
		}

		if (min_val > max_val) {
			i64 temp = min_val;
			min_val = max_val;
			max_val = temp;
		}
		u64 range = (u64)(max_val - min_val + 1);
		return (i64)rng_u64_bounded(rng, range) + min_val;
	}

	f32 rng_f32(Rng* rng) {
		if (!rng) {
			return 0.0f;
		}

		u32 r = rng_u32(rng) >> 8;
		return (f32)r * (1.0f / 16777216.0f);
	}

	f32 rng_f32_range(Rng* rng, f32 min_val, f32 max_val) {
		if (!rng) {
			return 0.0f;
		}

		return min_val + rng_f32(rng) * (max_val - min_val);
	}

	f64 rng_f64(Rng* rng) {
		if (!rng) {
			return 0.0;
		}

		u64 r = rng_u64(rng) >> 11;
		return (f64)r * (1.0 / 9007199254740992.0);
	}

	f64 rng_f64_range(Rng* rng, f64 min_val, f64 max_val) {
		if (!rng) {
			return 0.0;
		}

		return min_val + rng_f64(rng) * (max_val - min_val);
	}

	bool rng_bool(Rng* rng, f32 probability) {
		if (!rng) {
			return false;
		}

		return rng_f32(rng) < probability;
	}

	f32 rng_normal_f32(Rng* rng, f32 mean, f32 stddev) {
		if (!rng) {
			return 0.0f;
		}

		static thread_local bool has_spare = false;
		static thread_local f32 spare;

		if (has_spare) {
			has_spare = false;
			return mean + stddev * spare;
		}

		f32 u, v, s;
		do {
			u = rng_f32_range(rng, -1.0f, 1.0f);
			v = rng_f32_range(rng, -1.0f, 1.0f);
			s = u * u + v * v;
		} while (s >= 1.0f || s == 0.0f);

		s = std::sqrt(-2.0f * std::log(s) / s);
		spare = v * s;
		has_spare = true;

		return mean + stddev * u * s;
	}

	f64 rng_normal_f64(Rng* rng, f64 mean, f64 stddev) {
		if (!rng) {
			return 0.0;
		}

		static thread_local bool has_spare = false;
		static thread_local f64 spare;

		if (has_spare) {
			has_spare = false;
			return mean + stddev * spare;
		}

		f64 u, v, s;
		do {
			u = rng_f64_range(rng, -1.0, 1.0);
			v = rng_f64_range(rng, -1.0, 1.0);
			s = u * u + v * v;
		} while (s >= 1.0 || s == 0.0);

		s = std::sqrt(-2.0 * std::log(s) / s);
		spare = v * s;
		has_spare = true;

		return mean + stddev * u * s;
	}

	f32 rng_exp_f32(Rng* rng, f32 lambda) {
		if (!rng || lambda <= 0.0f) {
			return 0.0f;
		}

		return -std::log(1.0f - rng_f32(rng)) / lambda;
	}

	f64 rng_exp_f64(Rng* rng, f64 lambda) {
		if (!rng || lambda <= 0.0) {
			return 0.0;
		}

		return -std::log(1.0 - rng_f64(rng)) / lambda;
	}

	void rng_bytes(Rng* rng, void* buffer, usize size) {
		if (!rng || !buffer || size == 0) {
			return;
		}

		u8* bytes = (u8*)buffer;
		usize i = 0;

		while (i + 8 <= size) {
			u64 val = rng_u64(rng);
			memcpy(bytes + i, &val, 8);
			i += 8;
		}

		if (i < size) {
			u64 val = rng_u64(rng);
			memcpy(bytes + i, &val, size - i);
		}
	}
}