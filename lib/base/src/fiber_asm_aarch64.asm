#if defined(__aarch64__)

.text
.globl fiber_swap_context_internal
.globl fiber_main
.type fiber_swap_context_internal, %function
.type fiber_main, %function
.extern fiber_init

/* void fiber_main() */
fiber_main:
    ldp x0, x1, [sp], #16
    mov x19, x1
    mov x20, x0
    bl fiber_init
    br x19

/* void fiber_swap_context_internal(edge_fiber_context_t* from, edge_fiber_context_t* to) */
/* AAPCS64: 
 * x0 = from context
 * x1 = to context
 */
fiber_swap_context_internal:
    /* Save current context to 'from' (x0) */
    mov x9, sp
    stp x30, x9,  [x0, #0]    /* Save LR, SP */
    stp x29, x19, [x0, #16]   /* Save FP, x19 */
    stp x20, x21, [x0, #32]   /* Save x20, x21 */
    stp x22, x23, [x0, #48]   /* Save x22, x23 */
    stp x24, x25, [x0, #64]   /* Save x24, x25 */
    stp x26, x27, [x0, #80]   /* Save x26, x27 */
    str x28,      [x0, #96]   /* Save x28 */
    
    /* Save floating point registers d8-d15 */
    stp d8,  d9,  [x0, #104]
    stp d10, d11, [x0, #120]
    stp d12, d13, [x0, #136]
    stp d14, d15, [x0, #152]
    
    /* Restore context from 'to' (x1) */
    ldp x30, x9,  [x1, #0]    /* Restore LR, SP */
    mov sp, x9
    ldp x29, x19, [x1, #16]   /* Restore FP, x19 */
    ldp x20, x21, [x1, #32]   /* Restore x20, x21 */
    ldp x22, x23, [x1, #48]   /* Restore x22, x23 */
    ldp x24, x25, [x1, #64]   /* Restore x24, x25 */
    ldp x26, x27, [x1, #80]   /* Restore x26, x27 */
    ldr x28,      [x1, #96]   /* Restore x28 */
    
    /* Restore floating point registers d8-d15 */
    ldp d8,  d9,  [x1, #104]
    ldp d10, d11, [x1, #120]
    ldp d12, d13, [x1, #136]
    ldp d14, d15, [x1, #152]
    
    /* Return to saved LR */
    ret

.size fiber_swap_context_internal, .-fiber_swap_context_internal
.size fiber_main, .-fiber_main

#endif