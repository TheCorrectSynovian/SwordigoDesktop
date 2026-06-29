#include "sre.h"
#include "sre_lua.h"

/* Minimal hook implementation for luaL_newstate — disabled injection to avoid early-state side effects */

/* Original symbol pointer (set by host relay) */
static void* (*orig_luaL_newstate)(void) = 0;

void* sre_luaL_newstate(void) {
    /* Call original only; do NOT attempt automatic injection here. */
    if (orig_luaL_newstate) return orig_luaL_newstate();
    return NULL;
}
