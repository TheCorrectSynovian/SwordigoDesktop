/* sre_lua.c — Lua API function pointers + ProgramState replacements
 *
 * This is the crown jewel of libsre.so Phase 1.
 * 
 * What it does:
 *   1. Stores Lua API function pointers (resolved by host from libswordigo.so)
 *   2. sre_lua_call_safe — REPLACES lua_call with lua_pcall globally
 *   3. Replaces ProgramState::Execute — uses lua_pcall instead of lua_call
 *   4. Replaces ProgramState::Resume — catches Lua errors gracefully
 *   5. Replaces ProgramState::Update — handles timer-based coroutine resumption
 *
 * The key insight: the engine calls lua_call from MANY places, not just
 * ProgramState::Execute. Hooking lua_call itself catches ALL errors globally.
 *
 *   Before: lua_call → Lua error → panic → abort() → return → retry → ∞
 *   After:  sre_lua_call_safe → lua_pcall → error caught → continue!
 *
 * Based on SwMini's panic.c by Ijsd (itsjustsomedude).
 */

#include "sre.h"
#include "sre_lua.h"

/* ========== Lua API Function Pointers ========== */

pfn_lua_pcall       g_lua_pcall = 0;
pfn_lua_resume      g_lua_resume = 0;
pfn_lua_settop      g_lua_settop = 0;
pfn_lua_gettop      g_lua_gettop = 0;
pfn_lua_tolstring   g_lua_tolstring = 0;
pfn_lua_call        g_lua_call = 0;
pfn_lua_pushstring  g_lua_pushstring = 0;
pfn_lua_pushcclosure g_lua_pushcclosure = 0;
pfn_lua_setfield    g_lua_setfield = 0;
pfn_lua_getfield    g_lua_getfield = 0;
pfn_lua_createtable g_lua_createtable = 0;
pfn_lua_pushnumber  g_lua_pushnumber = 0;
pfn_lua_pushboolean g_lua_pushboolean = 0;
pfn_lua_pushnil     g_lua_pushnil = 0;
pfn_lua_tonumber    g_lua_tonumber = 0;
pfn_lua_toboolean   g_lua_toboolean = 0;
pfn_lua_type        g_lua_type = 0;
pfn_luaL_register   g_luaL_register = 0;
pfn_lua_touserdata  g_lua_touserdata = 0;
pfn_lua_pushlightuserdata g_lua_pushlightuserdata = 0;
pfn_lua_error       g_lua_error = 0;

/* Pointer to SceneObject::updateSpeedMultiplier (resolved by host) */
typedef float (*pfn_getSpeedMultiplier)(void* sceneObject);
pfn_getSpeedMultiplier g_getSpeedMultiplier = 0;

/* ========== sre_init_lua — called by host to set up function pointers ========== */

/* Struct passed from host with all resolved addresses */
typedef struct {
    sre_u64 lua_pcall;
    sre_u64 lua_resume;
    sre_u64 lua_settop;
    sre_u64 lua_gettop;
    sre_u64 lua_tolstring;
    sre_u64 lua_call;
    sre_u64 lua_pushstring;
    sre_u64 lua_pushcclosure;
    sre_u64 lua_setfield;
    sre_u64 lua_getfield;
    sre_u64 lua_createtable;
    sre_u64 lua_pushnumber;
    sre_u64 lua_pushboolean;
    sre_u64 lua_pushnil;
    sre_u64 lua_tonumber;
    sre_u64 lua_toboolean;
    sre_u64 lua_type;
    sre_u64 luaL_register;
    sre_u64 lua_touserdata;
    sre_u64 lua_pushlightuserdata;
    sre_u64 lua_error;
    sre_u64 getSpeedMultiplier;
} SreLuaAddrs;

void sre_init_lua(SreLuaAddrs* addrs) {
    g_lua_pcall       = (pfn_lua_pcall)addrs->lua_pcall;
    g_lua_resume      = (pfn_lua_resume)addrs->lua_resume;
    g_lua_settop      = (pfn_lua_settop)addrs->lua_settop;
    g_lua_gettop      = (pfn_lua_gettop)addrs->lua_gettop;
    g_lua_tolstring   = (pfn_lua_tolstring)addrs->lua_tolstring;
    g_lua_call        = (pfn_lua_call)addrs->lua_call;
    g_lua_pushstring  = (pfn_lua_pushstring)addrs->lua_pushstring;
    g_lua_pushcclosure = (pfn_lua_pushcclosure)addrs->lua_pushcclosure;
    g_lua_setfield    = (pfn_lua_setfield)addrs->lua_setfield;
    g_lua_getfield    = (pfn_lua_getfield)addrs->lua_getfield;
    g_lua_createtable = (pfn_lua_createtable)addrs->lua_createtable;
    g_lua_pushnumber  = (pfn_lua_pushnumber)addrs->lua_pushnumber;
    g_lua_pushboolean = (pfn_lua_pushboolean)addrs->lua_pushboolean;
    g_lua_pushnil     = (pfn_lua_pushnil)addrs->lua_pushnil;
    g_lua_tonumber    = (pfn_lua_tonumber)addrs->lua_tonumber;
    g_lua_toboolean   = (pfn_lua_toboolean)addrs->lua_toboolean;
    g_lua_type        = (pfn_lua_type)addrs->lua_type;
    g_luaL_register   = (pfn_luaL_register)addrs->luaL_register;
    g_lua_touserdata  = (pfn_lua_touserdata)addrs->lua_touserdata;
    g_lua_pushlightuserdata = (pfn_lua_pushlightuserdata)addrs->lua_pushlightuserdata;
    g_lua_error       = (pfn_lua_error)addrs->lua_error;
    g_getSpeedMultiplier = (pfn_getSpeedMultiplier)addrs->getSpeedMultiplier;
}

/* ========== sre_lua_call_safe — GLOBAL lua_call replacement ==========
 *
 * The trampoline replaces lua_call's first instructions with a branch
 * to this function. Every single lua_call in the engine now goes through
 * pcall protection.
 *
 * Before: lua_call(L, nargs, nresults) → error → panic → abort()
 * After:  sre_lua_call_safe(L, nargs, nresults) → pcall → error caught!
 *
 * CRITICAL: We also set up a setjmp recovery point. If pcall internally
 * triggers __cxa_throw (because the Android Lua uses C++ exceptions for
 * error handling), our sre_cxa_throw hook will longjmp back here instead
 * of attempting the broken unwind → abort path.
 *
 * Note: We can't call g_lua_call here — that's OUR OWN address (trampoline
 * destroyed the original). We MUST use g_lua_pcall which is a different
 * function at a different address.
 */
#include "sre_setjmp.h"

static int g_lua_call_safe_errors = 0;

/* Recovery stack — shared with sre_cxa_throw (sre_effects.c) */
sre_recovery_entry g_sre_recovery_stack[SRE_MAX_RECOVERY];
int g_sre_recovery_depth = 0;

/* Push a recovery entry. Returns the depth index, or -1 if stack full. */
static int recovery_push(lua_State* L) {
    if (g_sre_recovery_depth >= SRE_MAX_RECOVERY) return -1;
    int idx = g_sre_recovery_depth;
    sre_recovery_entry* e = &g_sre_recovery_stack[idx];
    e->lua_state = L;
    if (L) {
        void** ejp = (void**)((char*)L + LUA_ERRORJMP_OFFSET);
        e->saved_errorJmp = *ejp;
    } else {
        e->saved_errorJmp = 0;
    }
    g_sre_recovery_depth++;
    return idx;
}

/* Pop recovery stack back to a given depth */
static void recovery_pop(int depth) {
    g_sre_recovery_depth = depth;
}

void sre_lua_call_safe(lua_State* L, int nargs, int nresults) {
    if (!g_lua_pcall) {
        return;
    }
    
    /* Save stack top for recovery */
    int saved_top = 0;
    if (g_lua_gettop) {
        saved_top = g_lua_gettop(L);
    }
    
    /* Push recovery entry */
    int my_depth = recovery_push(L);
    if (my_depth < 0) {
        /* Stack full — fall through to raw pcall without recovery */
        int result = g_lua_pcall(L, nargs, nresults, 0);
        if (result != 0 && g_lua_settop) g_lua_settop(L, -2);
        return;
    }
    
    if (sre_setjmp(g_sre_recovery_stack[my_depth].buf) != 0) {
        /* Caught C++ exception via longjmp from sre_cxa_throw!
         * errorJmp was already restored by sre_cxa_throw. */
        recovery_pop(my_depth);
        g_lua_call_safe_errors++;
        
        /* Restore Lua stack to pre-call state */
        if (g_lua_settop && saved_top > 0) {
            g_lua_settop(L, saved_top - (nargs + 1));
        }
        
        /* Push nils for expected return values */
        if (nresults > 0 && nresults != -1 && g_lua_pushnil) {
            int i;
            for (i = 0; i < nresults; i++) {
                g_lua_pushnil(L);
            }
        }
        return;
    }
    
    int result = g_lua_pcall(L, nargs, nresults, 0);
    recovery_pop(my_depth);
    
    if (result != 0) {
        g_lua_call_safe_errors++;
        if (g_lua_settop) {
            g_lua_settop(L, -2);
        }
        if (nresults != -1 && g_lua_pushnil) {
            int i;
            for (i = 0; i < nresults; i++) {
                g_lua_pushnil(L);
            }
        }
    }
}
/* ========== Lua Console — in-engine Lua code execution ==========
 * 
 * The host writes a Lua code string to g_lua_console_buf[] and
 * sets g_lua_console_pending = 1. On the next ProgramState::Execute,
 * SRE runs the code in the engine's lua_State.
 *
 * Results/errors are written to g_lua_console_result[].
 *
 * These globals are at known addresses that the host reads/writes
 * directly in guest memory via get_symbol_vaddr().
 */
#define CONSOLE_BUF_SIZE 4096
char g_lua_console_buf[CONSOLE_BUF_SIZE];     /* Lua code to execute */
char g_lua_console_result[CONSOLE_BUF_SIZE];  /* Output/error message */
int  g_lua_console_pending = 0;               /* 1 = host wants to run code */
int  g_lua_console_status = 0;                /* 0=idle, 1=success, 2=error */

/* Last captured lua_State — for host inspection */
lua_State* g_sre_last_lua_state = 0;

/* Internal: execute pending console command in the given lua_State
 * 
 * Uses Lua's own loadstring() function via pcall — this avoids the
 * setjmp/longjmp crash that happens when luaL_loadbuffer hits syntax errors.
 * 
 * Flow: pcall(loadstring, code) → pcall(result_func)
 */
static void sre_run_console(lua_State* L) {
    if (!g_lua_getfield || !g_lua_pcall || !g_lua_pushstring) {
        const char* msg = "ERR: Lua API not resolved";
        int i;
        for (i = 0; msg[i] && i < CONSOLE_BUF_SIZE - 1; i++)
            g_lua_console_result[i] = msg[i];
        g_lua_console_result[i] = 0;
        g_lua_console_status = 2;
        return;
    }
    
    int base_top = g_lua_gettop(L);
    
    /* Step 1: Get loadstring function from globals */
    g_lua_getfield(L, LUA_GLOBALSINDEX, "loadstring");
    
    /* Step 2: Push the code string */
    g_lua_pushstring(L, g_lua_console_buf);
    
    /* Step 3: pcall loadstring(code) → (func, nil) or (nil, errmsg) */
    int r = g_lua_pcall(L, 1, 2, 0);
    if (r != 0) {
        /* pcall itself failed (shouldn't happen) */
        const char* err = lua_tostring(L, -1);
        if (err) {
            int i;
            for (i = 0; err[i] && i < CONSOLE_BUF_SIZE - 1; i++)
                g_lua_console_result[i] = err[i];
            g_lua_console_result[i] = 0;
        }
        g_lua_settop(L, base_top);
        g_lua_console_status = 2;
        return;
    }
    
    /* Step 4: Check if loadstring returned a function or nil+error */
    int type_at_minus2 = g_lua_type(L, -2);
    
    if (type_at_minus2 != LUA_TFUNCTION) {
        /* Syntax error: stack is [nil, errmsg] */
        const char* err = lua_tostring(L, -1);
        if (err) {
            int i;
            for (i = 0; err[i] && i < CONSOLE_BUF_SIZE - 1; i++)
                g_lua_console_result[i] = err[i];
            g_lua_console_result[i] = 0;
        } else {
            g_lua_console_result[0] = '?';
            g_lua_console_result[1] = 0;
        }
        g_lua_settop(L, base_top);
        g_lua_console_status = 2;
        return;
    }
    
    /* Step 5: Pop the nil, keep the function */
    lua_pop(L, 1);  /* remove nil from top, function is now at top */
    
    /* Step 6: pcall the compiled function */
    r = g_lua_pcall(L, 0, 1, 0);
    if (r != 0) {
        /* Runtime error */
        const char* err = lua_tostring(L, -1);
        if (err) {
            int i;
            for (i = 0; err[i] && i < CONSOLE_BUF_SIZE - 1; i++)
                g_lua_console_result[i] = err[i];
            g_lua_console_result[i] = 0;
        }
        g_lua_settop(L, base_top);
        g_lua_console_status = 2;
        return;
    }
    
    /* Step 7: Read result */
    if (g_lua_gettop(L) > base_top && g_lua_type(L, -1) != LUA_TNIL) {
        const char* result = lua_tostring(L, -1);
        if (result) {
            int i;
            for (i = 0; result[i] && i < CONSOLE_BUF_SIZE - 1; i++)
                g_lua_console_result[i] = result[i];
            g_lua_console_result[i] = 0;
        } else {
            g_lua_console_result[0] = 'O';
            g_lua_console_result[1] = 'K';
            g_lua_console_result[2] = 0;
        }
    } else {
        g_lua_console_result[0] = 'O';
        g_lua_console_result[1] = 'K';
        g_lua_console_result[2] = 0;
    }
    g_lua_settop(L, base_top);
    g_lua_console_status = 1;
}

/* ========== ProgramState::Execute replacement ========== 
 * Original: calls lua_call(L, nargs, 0) which aborts on error
 * Ours: calls lua_pcall(L, nargs, 0, 0) which catches errors
 *
 * Symbol: _ZN5Caver12ProgramState7ExecuteEi
 * SwMini: patches/panic.c
 */
void sre_ProgramState_Execute(void* self, int stackIndex) {
    lua_State* L = PS_GET(self, PS_LUA_STATE, lua_State*);
    
    /* Capture the lua_State for the host and console */
    g_sre_last_lua_state = L;
    
    /* Check for pending console command */
    if (g_lua_console_pending && L) {
        g_lua_console_pending = 0;
        sre_run_console(L);
    }
    
    /* Push recovery entry */
    int my_depth = recovery_push(L);
    if (my_depth < 0) {
        /* Stack full — no recovery, just run */
        g_lua_pcall(L, stackIndex, 0, 0);
        return;
    }
    
    if (sre_setjmp(g_sre_recovery_stack[my_depth].buf) != 0) {
        /* C++ exception caught — errorJmp restored by sre_cxa_throw */
        recovery_pop(my_depth);
        return;
    }
    
    /* Check if this is a coroutine (field at 0x08 != NULL) */
    void* coroutine = PS_GET(self, PS_COROUTINE, void*);
    
    if (coroutine == 0) {
        /* Not a thread — use pcall for error catching */
        int result = g_lua_pcall(L, stackIndex, 0, 0);
        recovery_pop(my_depth);
        
        if (result == 0) {
            return;
        }
        
        /* Clean up the stack as the engine would */
        g_lua_settop(L, -2);
    } else {
        /* Coroutine — use lua_resume */
        PS_SET(self, PS_IS_SUSPENDED, int, 0);
        
        int result = g_lua_resume(L, stackIndex);
        recovery_pop(my_depth);
        
        if (result != LUA_YIELD) {
            PS_SET(self, PS_COMPLETED, char, 1);
        }
    }
}

/* ========== ProgramState::Resume replacement ==========
 * Symbol: _ZN5Caver12ProgramState6ResumeEi
 */
void sre_ProgramState_Resume(void* self, int stackIndex) {
    lua_State* L = PS_GET(self, PS_LUA_STATE, lua_State*);
    
    /* Clear suspended flag */
    PS_SET(self, PS_IS_SUSPENDED, int, 0);
    
    /* Push recovery entry */
    int my_depth = recovery_push(L);
    if (my_depth < 0) {
        int result = g_lua_resume(L, stackIndex);
        if (result != LUA_YIELD) PS_SET(self, PS_COMPLETED, char, 1);
        return;
    }
    
    if (sre_setjmp(g_sre_recovery_stack[my_depth].buf) != 0) {
        recovery_pop(my_depth);
        PS_SET(self, PS_COMPLETED, char, 1);
        return;
    }
    
    int result = g_lua_resume(L, stackIndex);
    recovery_pop(my_depth);
    
    if (result != LUA_YIELD) {
        PS_SET(self, PS_COMPLETED, char, 1);
    }
}

/* ========== ProgramState::Update replacement ==========
 * Symbol: _ZN5Caver12ProgramState6UpdateEf
 * 
 * This handles timer-based coroutine resumption.
 * Only hooked on ARM64 (on ARM32, Update calls Resume which we already hook).
 */

/* We need the original ProgramState::Update for the second half of the function.
 * The host stores the original entry point here. */
typedef void (*pfn_orig_Update)(void* self, float deltaTime);
pfn_orig_Update g_orig_ProgramState_Update = 0;

void sre_ProgramState_Update(void* self, float deltaTime) {
    /* Capture lua_State and check console EVERY frame */
    lua_State* L = PS_GET(self, PS_LUA_STATE, lua_State*);
    if (L) {
        g_sre_last_lua_state = L;
        if (g_lua_console_pending) {
            g_lua_console_pending = 0;
            sre_run_console(L);
        }
    }
    
    char condition1 = PS_GET(self, PS_CONDITION1, char);
    char paused = PS_GET(self, PS_PAUSED, char);
    
    /* If neither condition is true, skip the rest (optimization) */
    if (!condition1 && !paused) return;
    
    /* Get the scene object for speed multiplier */
    void* thisObject = PS_GET(self, PS_SCENE_OBJECT, void*);
    float speedScaling = PS_GET(self, PS_SPEED_SCALING, float);
    int isSuspended = PS_GET(self, PS_IS_SUSPENDED, int);
    
    float scaledDelta = deltaTime;
    if (thisObject != 0 && g_getSpeedMultiplier != 0) {
        float mult = g_getSpeedMultiplier(thisObject);
        scaledDelta = mult * deltaTime;
    }
    float finalMultiplier = scaledDelta * speedScaling;
    
    if (isSuspended == 1) {
        /* Count down the sleep timer */
        float currentTimer = PS_GET(self, PS_SLEEP_TIME, float);
        float loweredTimer = currentTimer - finalMultiplier;
        PS_SET(self, PS_SLEEP_TIME, float, loweredTimer);
        
        if (loweredTimer < 0.0f) {
            /* Timer expired — resume the coroutine */
            PS_SET(self, PS_IS_SUSPENDED, int, 0);
            
            lua_State* L = PS_GET(self, PS_LUA_STATE, lua_State*);
            
            /* Recovery for C++ exceptions from lua_resume */
            int my_depth = recovery_push(L);
            if (my_depth < 0) {
                int result = g_lua_resume(L, 0);
                if (result != LUA_YIELD) PS_SET(self, PS_COMPLETED, char, 1);
            } else if (sre_setjmp(g_sre_recovery_stack[my_depth].buf) != 0) {
                recovery_pop(my_depth);
                PS_SET(self, PS_COMPLETED, char, 1);
            } else {
                int result = g_lua_resume(L, 0);
                recovery_pop(my_depth);
                
                if (result != LUA_YIELD) {
                    PS_SET(self, PS_COMPLETED, char, 1);
                }
            }
        }
    }
    
    /* Call original for the remaining logic (SceneObject iteration etc.)
     * We've already cleared isSuspended, so the original won't re-run the timer. */
    if (g_orig_ProgramState_Update != 0) {
        g_orig_ProgramState_Update(self, deltaTime);
    }
}
