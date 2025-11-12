#ifndef EDGE_CORO_INTERNAL_H
#define EDGE_CORO_INTERNAL_H

#include "edge_coro.h"

#ifdef __cplusplus
extern "C" {
#endif

    /* Architecture-specific context structure */
    struct edge_coro_context {
#if defined(__x86_64__) || defined(_M_X64)
        /* x86_64 context: save callee-saved registers */
        void* rip;    /* Return address */
        void* rsp;    /* Stack pointer */
        void* rbp;    /* Base pointer */
        void* rbx;
        void* r12;
        void* r13;
        void* r14;
        void* r15;
#ifdef _WIN32
        /* Windows x64 ABI requires saving these */
        void* rdi;
        void* rsi;
        uint64_t xmm6[2];
        uint64_t xmm7[2];
        uint64_t xmm8[2];
        uint64_t xmm9[2];
        uint64_t xmm10[2];
        uint64_t xmm11[2];
        uint64_t xmm12[2];
        uint64_t xmm13[2];
        uint64_t xmm14[2];
        uint64_t xmm15[2];
#endif

#elif defined(__aarch64__) || defined(_M_ARM64)
        /* aarch64 context: save callee-saved registers */
        /* ARM64 callee-saved registers */
        void* lr;     /* Link register (return address) */
        void* sp;     /* Stack pointer */
        void* fp;     /* Frame pointer (x29) */
        void* x19;
        void* x20;
        void* x21;
        void* x22;
        void* x23;
        void* x24;
        void* x25;
        void* x26;
        void* x27;
        void* x28;

        /* Floating point registers (d8-d15 are callee-saved) */
        uint64_t d8;
        uint64_t d9;
        uint64_t d10;
        uint64_t d11;
        uint64_t d12;
        uint64_t d13;
        uint64_t d14;
        uint64_t d15;
#else
#error "Unsupported architecture"
#endif
    };

    /* Opaque coroutine handle */
    struct edge_coro {
        struct edge_coro_context* context;
        edge_coro_fn func;
        void* user_data;
        void* stack;
        edge_coro_status_t status;
        struct edge_coro* caller;
    };

/* Swap contexts */
extern void edge_coro_swap_context(struct edge_coro_context* from, struct edge_coro_context* to);

#ifdef __cplusplus
}
#endif

#endif /* EDGE_CORO_INTERNAL_H */