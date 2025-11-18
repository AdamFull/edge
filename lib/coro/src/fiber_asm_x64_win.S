#if defined(__x86_64__) && defined(_WIN32)

.text
.globl fiber_swap_context_internal
.globl fiber_main
.extern fiber_init

/* void fiber_main() 
 *   [rsp+0]  = entry function pointer
 */
 fiber_main:
    movq 8(%rsp), %rbx
    movq 0(%rsp), %rbp
    subq $16, %rsp
    callq fiber_init
    pushq %rbp
    jmpq *%rbx

/* void fiber_swap_context_internal(edge_fiber_context_t* from, edge_fiber_context_t* to) */
/* Windows x64 ABI: rcx = from, rdx = to */
fiber_swap_context_internal:
    /* Save return address */
    movq (%rsp), %rax
    movq %rax, 0(%rcx)       /* Save RIP */
    
    /* Save stack pointer (after return address) */
    leaq 8(%rsp), %rax       /* FIX: RSP should point past the return address */
    movq %rax, 8(%rcx)       /* Save RSP */
    
    /* Save current context to 'from' (rcx) */
    movq %rbx, 24(%rcx)      /* Save RBX */
    movq %rbp, 16(%rcx)      /* Save RBP */
    movq %rdi, 64(%rcx)      /* Save RDI */
    movq %rsi, 72(%rcx)      /* Save RSI */
    movq %r12, 32(%rcx)      /* Save R12 */
    movq %r13, 40(%rcx)      /* Save R13 */
    movq %r14, 48(%rcx)      /* Save R14 */
    movq %r15, 56(%rcx)      /* Save R15 */
    
    /* Save XMM registers (Windows x64 ABI) */
    movdqu %xmm6, 80(%rcx)
    movdqu %xmm7, 96(%rcx)
    movdqu %xmm8, 112(%rcx)
    movdqu %xmm9, 128(%rcx)
    movdqu %xmm10, 144(%rcx)
    movdqu %xmm11, 160(%rcx)
    movdqu %xmm12, 176(%rcx)
    movdqu %xmm13, 192(%rcx)
    movdqu %xmm14, 208(%rcx)
    movdqu %xmm15, 224(%rcx)
    
    /* Restore context from 'to' (rdx) */
    movq 24(%rdx), %rbx      /* Restore RBX */
    movq 16(%rdx), %rbp      /* Restore RBP */
    movq 64(%rdx), %rdi      /* Restore RDI */
    movq 72(%rdx), %rsi      /* Restore RSI */
    movq 32(%rdx), %r12      /* Restore R12 */
    movq 40(%rdx), %r13      /* Restore R13 */
    movq 48(%rdx), %r14      /* Restore R14 */
    movq 56(%rdx), %r15      /* Restore R15 */
    
    /* Restore XMM registers */
    movdqu 80(%rdx), %xmm6
    movdqu 96(%rdx), %xmm7
    movdqu 112(%rdx), %xmm8
    movdqu 128(%rdx), %xmm9
    movdqu 144(%rdx), %xmm10
    movdqu 160(%rdx), %xmm11
    movdqu 176(%rdx), %xmm12
    movdqu 192(%rdx), %xmm13
    movdqu 208(%rdx), %xmm14
    movdqu 224(%rdx), %xmm15
    
    /* Restore stack pointer */
    movq 8(%rdx), %rsp       /* Restore RSP */
    
    /* Jump to saved instruction pointer */
    movq 0(%rdx), %rax       /* Load RIP */
    jmpq *%rax               /* Jump to RIP */

#endif