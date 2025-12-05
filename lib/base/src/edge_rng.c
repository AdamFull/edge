#include "edge_rng.h"
#include "edge_allocator.h"
#include "edge_math.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#else
#include <unistd.h>
#include <sys/time.h>
#endif

static u64 get_time_seed(void) {
#ifdef _WIN32
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (u64)counter.QuadPart;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (u64)tv.tv_sec * 1000000ULL + (u64)tv.tv_usec;
#endif
}

static bool get_system_entropy(void* buffer, size_t size) {
#ifdef _WIN32
    return BCryptGenRandom(NULL, (PUCHAR)buffer, (ULONG)size, BCRYPT_USE_SYSTEM_PREFERRED_RNG) >= 0;
#else
    FILE* urandom = fopen("/dev/urandom", "rb");
    if (!urandom) {
        return false;
    }

    size_t read = fread(buffer, 1, size, urandom);
    fclose(urandom);
    return read == size;
#endif
}

static u64 mix_entropy(u64 a, u64 b, u64 c) {
    a = a ^ (b << 13) ^ (c >> 7);
    b = b ^ (c << 17) ^ (a >> 11);
    c = c ^ (a << 5) ^ (b >> 23);
    return a ^ b ^ c;
}

static void pcg_seed(edge_re_pcg_t* pcg, u64 seed) {
    pcg->state = 0;
    pcg->inc = (seed << 1) | 1;
    pcg->state = pcg->state * 6364136223846793005ULL + pcg->inc;
    pcg->state += seed;
    pcg->state = pcg->state * 6364136223846793005ULL + pcg->inc;
}

static u32 pcg_next(edge_re_pcg_t* pcg) {
    u64 oldstate = pcg->state;
    pcg->state = oldstate * 6364136223846793005ULL + pcg->inc;
    u32 xorshifted = (u32)(((oldstate >> 18u) ^ oldstate) >> 27u);
    u32 rot = (u32)(oldstate >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((-(i32)rot) & 31));
}

static inline u64 rotl64(u64 x, int k) {
    return (x << k) | (x >> (64 - k));
}

static void xoshiro256_seed(edge_re_xoshiro256_t* xs, u64 seed) {
    /* Use SplitMix64 to generate initial state */
    u64 z = seed;
    for (int i = 0; i < 4; i++) {
        z += 0x9e3779b97f4a7c15ULL;
        u64 t = z;
        t = (t ^ (t >> 30)) * 0xbf58476d1ce4e5b9ULL;
        t = (t ^ (t >> 27)) * 0x94d049bb133111ebULL;
        xs->s[i] = t ^ (t >> 31);
    }
}

static u64 xoshiro256_next(edge_re_xoshiro256_t* xs) {
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

static void xoshiro256_jump(edge_re_xoshiro256_t* xs) {
    static const u64 JUMP[] = {
        0x180ec6d33cfd0abaULL, 0xd5a61266f0c9392cULL,
        0xa9582618e03fc9aaULL, 0x39abdc4529b1661cULL
    };

    u64 s0 = 0, s1 = 0, s2 = 0, s3 = 0;
    for (int i = 0; i < 4; i++) {
        for (int b = 0; b < 64; b++) {
            if (JUMP[i] & (1ULL << b)) {
                s0 ^= xs->s[0];
                s1 ^= xs->s[1];
                s2 ^= xs->s[2];
                s3 ^= xs->s[3];
            }
            xoshiro256_next(xs);
        }
    }
    xs->s[0] = s0; xs->s[1] = s1; xs->s[2] = s2; xs->s[3] = s3;
}

static void splitmix64_seed(edge_re_splitmix64_t* sm, u64 seed) {
    sm->state = seed;
}

static u64 splitmix64_next(edge_re_splitmix64_t* sm) {
    u64 z = (sm->state += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

void edge_rng_create(edge_rng_algorithm_t algorithm, u64 seed, edge_rng_t* rng) {
    if (!rng) {
        return;
    }

    rng->algorithm = algorithm;
    edge_rng_seed(rng, seed);
}

void edge_rng_seed(edge_rng_t* rng, u64 seed) {
    if (!rng) {
        return;
    }

    switch (rng->algorithm) {
    case EDGE_RNG_PCG:
        pcg_seed(&rng->state.pcg, seed);
        break;
    case EDGE_RNG_XOSHIRO256:
        xoshiro256_seed(&rng->state.xoshiro256, seed);
        break;
    case EDGE_RNG_SPLITMIX64:
        splitmix64_seed(&rng->state.splitmix64, seed);
        break;
    }
}

void edge_rng_seed_entropy(edge_rng_t* rng) {
    if (!rng) {
        return;
    }

    u64 seed = get_time_seed();
    seed ^= (u64)clock();
    seed ^= (u64)(uintptr_t)rng;
    edge_rng_seed(rng, seed);
}

void edge_rng_seed_entropy_secure(edge_rng_t* rng) {
    if (!rng) {
        return;
    }

    u64 entropy[4];
    if (get_system_entropy(entropy, sizeof(entropy))) {
        u64 seed = mix_entropy(entropy[0], entropy[1], entropy[2]) ^ entropy[3];
        edge_rng_seed(rng, seed);
    }
    else {
        edge_rng_seed_entropy(rng);
    }
}

u32 edge_rng_u32(edge_rng_t* rng) {
    if (!rng) {
        return 0;
    }

    switch (rng->algorithm) {
    case EDGE_RNG_PCG:
        return pcg_next(&rng->state.pcg);
    case EDGE_RNG_XOSHIRO256:
        return (u32)xoshiro256_next(&rng->state.xoshiro256);
    case EDGE_RNG_SPLITMIX64:
        return (u32)splitmix64_next(&rng->state.splitmix64);
    }

    return 0;
}

u32 edge_rng_u32_bounded(edge_rng_t* rng, u32 bound) {
    if (!rng || bound == 0) {
        return 0;
    }

    u32 threshold = (u32)(-bound) % bound;
    for (;;) {
        u32 r = edge_rng_u32(rng);
        u64 m = (u64)r * (u64)bound;
        u32 l = (u32)m;
        if (l < threshold) {
            continue;
        }
        return (u32)(m >> 32);
    }
}

i32 edge_rng_i32_range(edge_rng_t* rng, i32 min, i32 max) {
    if (!rng) {
        return 0;
    }

    if (min > max) {
        i32 temp = min;
        min = max;
        max = temp;
    }
    u32 range = (u32)(max - min + 1);
    return (i32)edge_rng_u32_bounded(rng, range) + min;
}

u64 edge_rng_u64(edge_rng_t* rng) {
    if (!rng) {
        return 0;
    }

    switch (rng->algorithm) {
    case EDGE_RNG_PCG:
        return ((u64)pcg_next(&rng->state.pcg) << 32) | pcg_next(&rng->state.pcg);
    case EDGE_RNG_XOSHIRO256:
        return xoshiro256_next(&rng->state.xoshiro256);
    case EDGE_RNG_SPLITMIX64:
        return splitmix64_next(&rng->state.splitmix64);
    }
    return 0;
}

u64 edge_rng_u64_bounded(edge_rng_t* rng, u64 bound) {
    if (!rng || bound == 0) {
        return 0;
    }

    return edge_rng_u64(rng) % bound;
}

i64 edge_rng_i64_range(edge_rng_t* rng, i64 min, i64 max) {
    if (min > max) {
        i64 temp = min;
        min = max;
        max = temp;
    }
    u64 range = (u64)(max - min + 1);
    return (i64)edge_rng_u64_bounded(rng, range) + min;
}

f32 edge_rng_f32(edge_rng_t* rng) {
    if (!rng) {
        return 0.0f;
    }

    u32 r = edge_rng_u32(rng) >> 8;
    return (f32)r * (1.0f / 16777216.0f);
}

f32 edge_rng_f32_range(edge_rng_t* rng, f32 min, f32 max) {
    if (!rng) {
        return 0.0f;
    }

    return min + edge_rng_f32(rng) * (max - min);
}

f64 edge_rng_f64(edge_rng_t* rng) {
    if (!rng) {
        return 0.0;
    }

    u64 r = edge_rng_u64(rng) >> 11;
    return (f64)r * (1.0 / 9007199254740992.0);
}

f64 edge_rng_f64_range(edge_rng_t* rng, f64 min, f64 max) {
    if (!rng) {
        return 0.0;
    }

    return min + edge_rng_f64(rng) * (max - min);
}

bool edge_rng_bool(edge_rng_t* rng, f32 probability) {
    if (!rng) {
        return false;
    }

    return edge_rng_f32(rng) < probability;
}

f32 edge_rng_normal_f32(edge_rng_t* rng, f32 mean, f32 stddev) {
    if (!rng) {
        return 0.0f;
    }

    static bool has_spare = false;
    static f32 spare;

    if (has_spare) {
        has_spare = false;
        return mean + stddev * spare;
    }

    f32 u, v, s;
    do {
        u = edge_rng_f32_range(rng, -1.0f, 1.0f);
        v = edge_rng_f32_range(rng, -1.0f, 1.0f);
        s = u * u + v * v;
    } while (s >= 1.0f || s == 0.0f);

    // TODO: Too slow
    s = sqrtf(-2.0f * logf(s) / s);
    spare = v * s;
    has_spare = true;

    return mean + stddev * u * s;
}

f64 edge_rng_normal_f64(edge_rng_t* rng, f64 mean, f64 stddev) {
    if (!rng) {
        return 0.0;
    }

    static bool has_spare = false;
    static f64 spare;

    if (has_spare) {
        has_spare = false;
        return mean + stddev * spare;
    }

    f64 u, v, s;
    do {
        u = edge_rng_f64_range(rng, -1.0, 1.0);
        v = edge_rng_f64_range(rng, -1.0, 1.0);
        s = u * u + v * v;
    } while (s >= 1.0 || s == 0.0);

    // TODO: Too slow
    s = sqrt(-2.0 * log(s) / s);
    spare = v * s;
    has_spare = true;

    return mean + stddev * u * s;
}

f32 edge_rng_exp_f32(edge_rng_t* rng, f32 lambda) {
    if (!rng || lambda <= 0.0f) {
        return 0.0f;
    }

    return -logf(1.0f - edge_rng_f32(rng)) / lambda;
}

f64 edge_rng_exp_f64(edge_rng_t* rng, f64 lambda) {
    if (!rng || lambda <= 0.0) {
        return 0.0;
    }

    return -log(1.0 - edge_rng_f64(rng)) / lambda;
}

void edge_rng_shuffle(edge_rng_t* rng, const edge_allocator_t* alloc, void* array, size_t count, size_t element_size) {
    if (!rng || !alloc || !array || count == 0) {
        return;
    }

    u8* arr = (u8*)array;
    u8* temp = (u8*)edge_allocator_malloc(alloc, element_size);
    if (!temp) {
        return;
    }

    for (size_t i = count - 1; i > 0; i--) {
        size_t j = edge_rng_u32_bounded(rng, (u32)(i + 1));

        memcpy(temp, arr + j * element_size, element_size);
        memcpy(arr + j * element_size, arr + i * element_size, element_size);
        memcpy(arr + i * element_size, temp, element_size);
    }

    edge_allocator_free(alloc, temp);
}

void edge_rng_choice(edge_rng_t* rng, const void* array, size_t count, size_t element_size, void* out_element) {
    if (!rng || !array || count == 0) {
        return;
    }

    size_t index = edge_rng_u32_bounded(rng, (u32)count);
    memcpy(out_element, (const u8*)array + index * element_size, element_size);
}

void edge_rng_bytes(edge_rng_t* rng, void* buffer, size_t size) {
    if (!buffer || size == 0) {
        return;
    }

    u8* bytes = (u8*)buffer;
    size_t i = 0;

    while (i + 8 <= size) {
        u64 val = edge_rng_u64(rng);
        memcpy(bytes + i, &val, 8);
        i += 8;
    }

    if (i < size) {
        u64 val = edge_rng_u64(rng);
        memcpy(bytes + i, &val, size - i);
    }
}