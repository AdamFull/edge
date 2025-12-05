#ifndef EDGE_RANDOM_H
#define EDGE_RANDOM_H

#include "edge_base.h"

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct edge_allocator edge_allocator_t;

    typedef struct edge_re_pcg {
        u64 state;
        u64 inc;
    } edge_re_pcg_t;

    typedef struct edge_re_xoshiro256 {
        u64 s[4];
    } edge_re_xoshiro256_t;

    typedef struct edge_re_splitmix64 {
        u64 state;
    } edge_re_splitmix64_t;

    typedef enum edge_rng_algorithm {
        EDGE_RNG_PCG,
        EDGE_RNG_XOSHIRO256,
        EDGE_RNG_SPLITMIX64
    } edge_rng_algorithm_t;

    typedef struct edge_rng {
        edge_rng_algorithm_t algorithm;
        union {
            edge_re_pcg_t        pcg;
            edge_re_xoshiro256_t xoshiro256;
            edge_re_splitmix64_t splitmix64;
        } state;
    } edge_rng_t;

    void edge_rng_create(edge_rng_algorithm_t algorithm, u64 seed, edge_rng_t* rng);
    void edge_rng_seed(edge_rng_t* rng, u64 seed);
    void edge_rng_seed_entropy(edge_rng_t* rng);
    void edge_rng_seed_entropy_secure(edge_rng_t* rng);

    u32 edge_rng_u32(edge_rng_t* rng);
    u32 edge_rng_u32_bounded(edge_rng_t* rng, u32 bound);
    i32 edge_rng_i32_range(edge_rng_t* rng, i32 min, i32 max);

    u64 edge_rng_u64(edge_rng_t* rng);
    u64 edge_rng_u64_bounded(edge_rng_t* rng, u64 bound);
    i64 edge_rng_i64_range(edge_rng_t* rng, i64 min, i64 max);

    f32 edge_rng_f32(edge_rng_t* rng);
    f32 edge_rng_f32_range(edge_rng_t* rng, f32 min, f32 max);

    f64 edge_rng_f64(edge_rng_t* rng);
    f64 edge_rng_f64_range(edge_rng_t* rng, f64 min, f64 max);

    bool edge_rng_bool(edge_rng_t* rng, f32 probability);

    f32 edge_rng_normal_f32(edge_rng_t* rng, f32 mean, f32 stddev);
    f64 edge_rng_normal_f64(edge_rng_t* rng, f64 mean, f64 stddev);

    f32 edge_rng_exp_f32(edge_rng_t* rng, f32 lambda);
    f64 edge_rng_exp_f64(edge_rng_t* rng, f64 lambda);

    void edge_rng_shuffle(edge_rng_t* rng, const edge_allocator_t* alloc, void* array, size_t count, size_t element_size);
    void edge_rng_choice(edge_rng_t* rng, const void* array, size_t count, size_t element_size, void* out_element);
    void edge_rng_bytes(edge_rng_t* rng, void* buffer, size_t size);

#ifdef __cplusplus
}
#endif

#endif
