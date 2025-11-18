#if defined(__x86_64__) && !defined(_WIN32)

.text
.globl fiber_swap_context_internal
.globl fiber_main
.extern fiber_init

/* void fiber_main() */
fiber_main:
    movq 0(%rsp), %rbx
    movq 8(%rsp), %r12
    addq $16, %rsp
    callq fiber_init
    pushq %rbx
    jmpq *%r12

/* void fiber_swap_context_internal(edge_fiber_context_t* from, edge_fiber_context_t* to) */
/* System V ABI: rdi = from, rsi = to */
fiber_swap_context_internal:
    /* Save return address */
    movq (%rsp), %rax
    movq %rax, 0(%rdi)       /* Save RIP */
    
    leaq 8(%rsp), %rax
    movq %rax, 8(%rdi)       /* Save RSP */
    
    /* Save current context to 'from' */
    movq %rbx, 24(%rdi)      /* Save RBX */
    movq %rbp, 16(%rdi)      /* Save RBP */
    movq %r12, 32(%rdi)      /* Save R12 */
    movq %r13, 40(%rdi)      /* Save R13 */
    movq %r14, 48(%rdi)      /* Save R14 */
    movq %r15, 56(%rdi)      /* Save R15 */
    
    /* Restore context from 'to' */
    movq 24(%rsi), %rbx      /* Restore RBX */
    movq 16(%rsi), %rbp      /* Restore RBP */
    movq 32(%rsi), %r12      /* Restore R12 */
    movq 40(%rsi), %r13      /* Restore R13 */
    movq 48(%rsi), %r14      /* Restore R14 */
    movq 56(%rsi), %r15      /* Restore R15 */
    
    /* Restore stack pointer */
    movq 8(%rsi), %rsp       /* Restore RSP */
    
    /* Jump to saved instruction pointer */
    movq 0(%rsi), %rax       /* Load RIP */
    jmpq *%rax               /* Jump to RIP */

.size fiber_swap_context_internal, .-fiber_swap_context_internal
.size fiber_main, .-fiber_main

#endif