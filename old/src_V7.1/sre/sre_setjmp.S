/* sre_setjmp.S — Minimal ARM64 setjmp/longjmp for libsre.so
 *
 * Used by sre_cxa_throw to convert broken C++ exceptions
 * into clean C-style error recovery via longjmp.
 *
 * jmp_buf layout (168 bytes):
 *   [0x00] X19, X20
 *   [0x10] X21, X22
 *   [0x20] X23, X24
 *   [0x30] X25, X26
 *   [0x40] X27, X28
 *   [0x50] X29 (FP), X30 (LR)
 *   [0x60] SP
 *   [0x68] D8, D9
 *   [0x78] D10, D11
 *   [0x88] D12, D13
 *   [0x98] D14, D15
 */

.text
.global sre_setjmp
.global sre_longjmp

/* int sre_setjmp(sre_jmp_buf *buf)
 * Saves callee-saved registers, returns 0.
 */
.type sre_setjmp, %function
sre_setjmp:
    stp x19, x20, [x0, #0x00]
    stp x21, x22, [x0, #0x10]
    stp x23, x24, [x0, #0x20]
    stp x25, x26, [x0, #0x30]
    stp x27, x28, [x0, #0x40]
    stp x29, x30, [x0, #0x50]
    mov x2, sp
    str x2, [x0, #0x60]
    stp d8,  d9,  [x0, #0x68]
    stp d10, d11, [x0, #0x78]
    stp d12, d13, [x0, #0x88]
    stp d14, d15, [x0, #0x98]
    mov w0, #0
    ret
.size sre_setjmp, .-sre_setjmp

/* void sre_longjmp(sre_jmp_buf *buf, int val)
 * Restores registers, returns val (or 1 if val==0) to setjmp's caller.
 */
.type sre_longjmp, %function
sre_longjmp:
    ldp x19, x20, [x0, #0x00]
    ldp x21, x22, [x0, #0x10]
    ldp x23, x24, [x0, #0x20]
    ldp x25, x26, [x0, #0x30]
    ldp x27, x28, [x0, #0x40]
    ldp x29, x30, [x0, #0x50]
    ldr x2, [x0, #0x60]
    mov sp, x2
    ldp d8,  d9,  [x0, #0x68]
    ldp d10, d11, [x0, #0x78]
    ldp d12, d13, [x0, #0x88]
    ldp d14, d15, [x0, #0x98]
    cmp w1, #0
    csinc w0, w1, wzr, ne   /* return val, or 1 if val==0 */
    ret                      /* returns to LR from setjmp */
.size sre_longjmp, .-sre_longjmp
