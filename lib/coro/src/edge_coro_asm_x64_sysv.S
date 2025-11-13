#if defined(__x86_64__) && !defined(_WIN32)

.text
.globl edge_coro_swap_context

/* void edge_coro_swap_context(edge_coro_context_t* from, edge_coro_context_t* to) */
/* System V ABI: rdi = from, rsi = to */
edge_coro_swap_context:
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

.size edge_coro_swap_context, .-edge_coro_swap_context

#endif