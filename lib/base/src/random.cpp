#include "random.hpp"

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
		static u64 get_time_seed() {
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

		static bool get_system_entropy(void* buffer, usize size) {
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

		static u64 mix_entropy(u64 a, u64 b, u64 c) {
			a = a ^ (b << 13) ^ (c >> 7);
			b = b ^ (c << 17) ^ (a >> 11);
			c = c ^ (a << 5) ^ (b >> 23);
			return a ^ b ^ c;
		}
	}

	template<RngAlgorithm Algorithm>
	void rng_seed_entropy(Algorithm& state) {
		u64 s = detail::get_time_seed();
		s ^= static_cast<u64>(clock());
		s ^= static_cast<u64>(reinterpret_cast<uintptr_t>(&state));
		set_seed(s);
	}

	template<RngAlgorithm Algorithm>
	void rng_seed_entropy_secure(Algorithm& state) {
		u64 entropy[4];
		if (detail::get_system_entropy(entropy, sizeof(entropy))) {
			u64 s = detail::mix_entropy(entropy[0], entropy[1], entropy[2]) ^ entropy[3];
			state.seed(s);
		}
		else {
			rng_seed_entropy(state);
		}
	}
}