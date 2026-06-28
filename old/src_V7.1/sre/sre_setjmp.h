/* sre_setjmp.h — Minimal setjmp/longjmp + recovery stack for libsre.so */
#ifndef SRE_SETJMP_H
#define SRE_SETJMP_H

/* jmp_buf: 176 bytes (13 GP regs + SP + 8 FP regs = 22 slots) */
typedef unsigned long long sre_jmp_buf[22];

/* Returns 0 on initial call, non-zero when longjmp fires */
extern int sre_setjmp(sre_jmp_buf buf);

/* Jump back to setjmp point with return value val */
extern void sre_longjmp(sre_jmp_buf buf, int val);

/* ========== Recovery Stack ==========
 * Handles nested lua_call → pcall → lua_call → pcall chains.
 * Each entry on the stack has its own jmp_buf and saved errorJmp. */

#define SRE_MAX_RECOVERY 16

typedef struct {
    sre_jmp_buf buf;
    void*       saved_errorJmp;  /* L->errorJmp before pcall */
    void*       lua_state;       /* L pointer for this level */
} sre_recovery_entry;

extern sre_recovery_entry g_sre_recovery_stack[SRE_MAX_RECOVERY];
extern int g_sre_recovery_depth;

/* Offset of errorJmp in lua_State (ARM64 Lua 5.1) */
#define LUA_ERRORJMP_OFFSET 0xa8

#endif
