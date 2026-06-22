/*
 * sre_mini_api.c — SwMini-compatible Lua API for mod support
 *
 * Reimplements the Mini.*, LNI.*, and Components.* Lua tables
 * that SwMini registers in every Lua state. RLSwordigo scripts
 * (rlsw.scl, code.scl, mason.scl, etc.) depend on these APIs.
 *
 * Instead of loading libmini.so (which needs GlossHook + JNI),
 * we implement the same API surface directly in SRE.
 *
 * Hook: ProgramState::RegisterProgramLibrary
 * After the engine registers its own Lua libs, we inject ours.
 */

#include "sre.h"
#include "sre_lua.h"

/* =========================================================================
 * Shared globals for host communication
 * ========================================================================= */

/* Mod identification */
char g_sre_mod_arch[32] = "arm64-v8a";     /* Mini.Arch() return value */
char g_sre_mod_profile_id[64] = {0};       /* Mini.GetProfileID() */

/* Game speed — readable/writable from Lua, host polls this */
float g_sre_game_speed = 1.0f;

/* Controls visibility flag */
int g_sre_controls_hidden = 0;

/* Coin limit */
int g_sre_coin_limit = 9999;

/* Debug toggle */
int g_sre_debug_active = 0;

/* LNI command buffer — for commands that need host action */
char g_sre_lni_command[64] = {0};
char g_sre_lni_arg[256] = {0};
int  g_sre_lni_pending = 0;

/* =========================================================================
 * Mini.* Lua Functions
 * ========================================================================= */

/* Mini.Arch() → string ("arm64-v8a" or "armeabi-v7a") */
static int l_mini_arch(lua_State* L) {
    g_lua_pushstring(L, g_sre_mod_arch);
    return 1;
}

/* Mini.GetProfileID() → string (UUID of current save) */
static int l_mini_get_profile_id(lua_State* L) {
    g_lua_pushstring(L, g_sre_mod_profile_id);
    return 1;
}

/* Mini.SetControlsHidden(bool) */
static int l_mini_set_controls_hidden(lua_State* L) {
    g_sre_controls_hidden = g_lua_toboolean(L, 1);
    return 0;
}

/* Mini.SetCoinLimit(n) */
static int l_mini_set_coin_limit(lua_State* L) {
    int limit = (int)g_lua_tonumber(L, 1);
    if (limit > 0 && limit <= 65535) {
        g_sre_coin_limit = limit;
    }
    return 0;
}

/* Mini.ToggleDebug() */
static int l_mini_toggle_debug(lua_State* L) {
    (void)L;
    g_sre_debug_active = !g_sre_debug_active;
    return 0;
}

/* Mini.RecreateHero() — placeholder, needs engine call */
static int l_mini_recreate_hero(lua_State* L) {
    (void)L;
    /* TODO: Call Caver::RecreateHero via guest function pointer */
    return 0;
}

/* Mini.SceneFindAll() → table of scene object names
 * Placeholder — returns empty table for now */
static int l_mini_scene_find_all(lua_State* L) {
    g_lua_createtable(L, 0, 0);
    return 1;
}

/* Mini.map(...) — polymorphic map function
 * SwMini supports 4 overloads based on arg types:
 *   map(table, fn) — call fn(v) for each value in table
 *   map(fn, table) — call fn(v) for each value in array (same but reversed args)
 *   map(string, fn) — call fn(char) for each character
 *   map(number, fn) — call fn(i) for i=1..n
 * All return a table of results.
 */
static int l_mini_map(lua_State* L) {
    int nargs = g_lua_gettop(L);
    if (nargs < 2) {
        g_lua_createtable(L, 0, 0);
        return 1;
    }

    int t1 = g_lua_type(L, 1);
    int t2 = g_lua_type(L, 2);
    int tbl_idx = 0, fn_idx = 0;

    if (t1 == LUA_TTABLE && t2 == LUA_TFUNCTION) {
        tbl_idx = 1; fn_idx = 2;
    } else if (t1 == LUA_TFUNCTION && t2 == LUA_TTABLE) {
        fn_idx = 1; tbl_idx = 2;
    } else if (t1 == LUA_TNUMBER && t2 == LUA_TFUNCTION) {
        /* map(n, fn) — iterate 1..n */
        int n = (int)g_lua_tonumber(L, 1);
        g_lua_createtable(L, n, 0);
        int result_idx = g_lua_gettop(L);
        int i;
        for (i = 1; i <= n; i++) {
            if (g_lua_pushvalue) g_lua_pushvalue(L, 2);  /* push fn */
            g_lua_pushnumber(L, (double)i);
            g_lua_pcall(L, 1, 1, 0);
            if (g_lua_rawseti) g_lua_rawseti(L, result_idx, i);
            else lua_pop(L, 1);
        }
        return 1;
    } else if (t1 == LUA_TSTRING && t2 == LUA_TFUNCTION) {
        /* map(string, fn) — iterate characters */
        sre_size_t slen = 0;
        const char* s = g_lua_tolstring(L, 1, &slen);
        g_lua_createtable(L, (int)slen, 0);
        int result_idx = g_lua_gettop(L);
        size_t i;
        for (i = 0; i < slen; i++) {
            if (g_lua_pushvalue) g_lua_pushvalue(L, 2);  /* push fn */
            if (g_lua_pushlstring) g_lua_pushlstring(L, s + i, 1);
            else g_lua_pushstring(L, "?");
            g_lua_pcall(L, 1, 1, 0);
            if (g_lua_rawseti) g_lua_rawseti(L, result_idx, (int)(i + 1));
            else lua_pop(L, 1);
        }
        return 1;
    } else {
        g_lua_createtable(L, 0, 0);
        return 1;
    }

    /* Table+function case */
    if (!g_lua_objlen || !g_lua_rawgeti || !g_lua_rawseti || !g_lua_pushvalue) {
        g_lua_createtable(L, 0, 0);
        return 1;
    }
    int tlen = (int)g_lua_objlen(L, tbl_idx);
    g_lua_createtable(L, tlen, 0);
    int result_idx = g_lua_gettop(L);
    int i;
    for (i = 1; i <= tlen; i++) {
        g_lua_pushvalue(L, fn_idx);
        g_lua_rawgeti(L, tbl_idx, i);
        g_lua_pcall(L, 1, 1, 0);
        g_lua_rawseti(L, result_idx, i);
    }
    return 1;
}

/* Mini.ExecuteLNI(funcName, ...) — bridge to host LNI system */
static int l_mini_execute_lni(lua_State* L) {
    const char* func = lua_tostring(L, 1);
    if (!func) return 0;

    /* Copy function name to LNI command buffer */
    int i;
    for (i = 0; i < 63 && func[i]; i++) {
        g_sre_lni_command[i] = func[i];
    }
    g_sre_lni_command[i] = '\0';

    /* If there's a string argument, copy it too */
    if (g_lua_gettop(L) >= 2 && g_lua_type(L, 2) == LUA_TSTRING) {
        const char* arg = lua_tostring(L, 2);
        if (arg) {
            for (i = 0; i < 255 && arg[i]; i++) {
                g_sre_lni_arg[i] = arg[i];
            }
            g_sre_lni_arg[i] = '\0';
        }
    } else {
        g_sre_lni_arg[0] = '\0';
    }

    g_sre_lni_pending = 1;
    return 0;
}

/* Mini.BindLNI(funcName) → function
 * Returns a Lua function that calls ExecuteLNI with the given name */
static int l_mini_bind_lni(lua_State* L) {
    /* Push the function name as an upvalue */
    g_lua_pushcclosure(L, l_mini_execute_lni, 1);
    return 1;
}

/* =========================================================================
 * LNI.* Lua Functions (direct host actions, no Java needed)
 * ========================================================================= */

/* LNI.getSpeed() → number */
static int l_lni_get_speed(lua_State* L) {
    g_lua_pushnumber(L, (double)g_sre_game_speed);
    return 1;
}

/* LNI.setSpeed(n) */
static int l_lni_set_speed(lua_State* L) {
    double speed = g_lua_tonumber(L, 1);
    if (speed > 0.0 && speed <= 10.0) {
        g_sre_game_speed = (float)speed;
    }
    return 0;
}

/* LNI.quit() — signal host to exit */
static int l_lni_quit(lua_State* L) {
    (void)L;
    /* Set LNI command for host to process */
    g_sre_lni_command[0] = 'q'; g_sre_lni_command[1] = 'u';
    g_sre_lni_command[2] = 'i'; g_sre_lni_command[3] = 't';
    g_sre_lni_command[4] = '\0';
    g_sre_lni_pending = 1;
    return 0;
}

/* LNI.copyToClipboard(text) — signal host */
static int l_lni_copy_to_clipboard(lua_State* L) {
    const char* text = lua_tostring(L, 1);
    if (!text) return 0;

    /* Copy function name */
    const char* cmd = "copyToClipboard";
    int i;
    for (i = 0; cmd[i]; i++) g_sre_lni_command[i] = cmd[i];
    g_sre_lni_command[i] = '\0';

    /* Copy argument */
    for (i = 0; i < 255 && text[i]; i++) g_sre_lni_arg[i] = text[i];
    g_sre_lni_arg[i] = '\0';

    g_sre_lni_pending = 1;
    return 0;
}

/* LNI.openUrl(url) — signal host */
static int l_lni_open_url(lua_State* L) {
    const char* url = lua_tostring(L, 1);
    if (!url) return 0;

    const char* cmd = "openUrl";
    int i;
    for (i = 0; cmd[i]; i++) g_sre_lni_command[i] = cmd[i];
    g_sre_lni_command[i] = '\0';

    for (i = 0; i < 255 && url[i]; i++) g_sre_lni_arg[i] = url[i];
    g_sre_lni_arg[i] = '\0';

    g_sre_lni_pending = 1;
    return 0;
}

/* =========================================================================
 * Registration — inject Mini/LNI tables into a Lua state
 * ========================================================================= */

/*
 * sre_register_mini_api — Register Mini.* and LNI.* tables
 *
 * Called from our RegisterProgramLibrary hook for every new Lua state.
 * Creates the global tables that RL scripts expect.
 */
void sre_register_mini_api(lua_State* L) {
    if (!g_lua_createtable || !g_lua_setfield || !g_lua_pushcclosure) {
        return;  /* Lua API not initialized yet */
    }

    /* ---- Mini table ---- */
    g_lua_createtable(L, 0, 12);  /* Mini = {} */

    g_lua_pushcclosure(L, l_mini_arch, 0);
    g_lua_setfield(L, -2, "Arch");

    g_lua_pushcclosure(L, l_mini_get_profile_id, 0);
    g_lua_setfield(L, -2, "GetProfileID");

    g_lua_pushcclosure(L, l_mini_set_controls_hidden, 0);
    g_lua_setfield(L, -2, "SetControlsHidden");

    g_lua_pushcclosure(L, l_mini_set_coin_limit, 0);
    g_lua_setfield(L, -2, "SetCoinLimit");

    g_lua_pushcclosure(L, l_mini_toggle_debug, 0);
    g_lua_setfield(L, -2, "ToggleDebug");

    g_lua_pushcclosure(L, l_mini_recreate_hero, 0);
    g_lua_setfield(L, -2, "RecreateHero");

    g_lua_pushcclosure(L, l_mini_scene_find_all, 0);
    g_lua_setfield(L, -2, "SceneFindAll");

    g_lua_pushcclosure(L, l_mini_execute_lni, 0);
    g_lua_setfield(L, -2, "ExecuteLNI");

    g_lua_pushcclosure(L, l_mini_bind_lni, 0);
    g_lua_setfield(L, -2, "BindLNI");

    g_lua_pushcclosure(L, l_mini_map, 0);
    g_lua_setfield(L, -2, "map");

    /* Mini.Health = {} (sub-table) */
    g_lua_createtable(L, 0, 2);
    g_lua_setfield(L, -2, "Health");

    /* Mini.Character = {} (sub-table) */
    g_lua_createtable(L, 0, 2);
    g_lua_setfield(L, -2, "Character");

    g_lua_setfield(L, LUA_GLOBALSINDEX, "Mini");  /* _G.Mini = table */

    /* ---- LNI table ---- */
    g_lua_createtable(L, 0, 5);  /* LNI = {} */

    g_lua_pushcclosure(L, l_lni_get_speed, 0);
    g_lua_setfield(L, -2, "getSpeed");

    g_lua_pushcclosure(L, l_lni_set_speed, 0);
    g_lua_setfield(L, -2, "setSpeed");

    g_lua_pushcclosure(L, l_lni_quit, 0);
    g_lua_setfield(L, -2, "quit");

    g_lua_pushcclosure(L, l_lni_copy_to_clipboard, 0);
    g_lua_setfield(L, -2, "copyToClipboard");

    g_lua_pushcclosure(L, l_lni_open_url, 0);
    g_lua_setfield(L, -2, "openUrl");

    /* Case-insensitive aliases — RL scripts use various casings */
    g_lua_pushcclosure(L, l_lni_get_speed, 0);
    g_lua_setfield(L, -2, "GetSpeed");
    g_lua_pushcclosure(L, l_lni_set_speed, 0);
    g_lua_setfield(L, -2, "SetSpeed");
    g_lua_pushcclosure(L, l_lni_quit, 0);
    g_lua_setfield(L, -2, "Quit");
    g_lua_pushcclosure(L, l_lni_copy_to_clipboard, 0);
    g_lua_setfield(L, -2, "CopyToClipboard");
    g_lua_pushcclosure(L, l_lni_copy_to_clipboard, 0);
    g_lua_setfield(L, -2, "copy");
    g_lua_pushcclosure(L, l_lni_copy_to_clipboard, 0);
    g_lua_setfield(L, -2, "Copy");
    g_lua_pushcclosure(L, l_lni_open_url, 0);
    g_lua_setfield(L, -2, "OpenUrl");
    g_lua_pushcclosure(L, l_lni_open_url, 0);
    g_lua_setfield(L, -2, "openURL");
    g_lua_pushcclosure(L, l_lni_open_url, 0);
    g_lua_setfield(L, -2, "OpenURL");

    g_lua_setfield(L, LUA_GLOBALSINDEX, "LNI");  /* _G.LNI = table */

    /* ---- Components table (stub) ---- */
    g_lua_createtable(L, 0, 4);

    /* Components.Health = {} */
    g_lua_createtable(L, 0, 4);
    g_lua_setfield(L, -2, "Health");

    /* Components.Physics = {} */
    g_lua_createtable(L, 0, 4);
    g_lua_setfield(L, -2, "Physics");

    /* Components.Entity = {} */
    g_lua_createtable(L, 0, 4);
    g_lua_setfield(L, -2, "Entity");

    g_lua_setfield(L, LUA_GLOBALSINDEX, "Components");

    /* ---- fs (LuaFileSystem stub) ---- */
    g_lua_createtable(L, 0, 0);
    g_lua_setfield(L, LUA_GLOBALSINDEX, "fs");
}

/* =========================================================================
 * Lazy Mini.* injection — called from sre_lua_call_safe
 * =========================================================================
 * Instead of hooking RegisterProgramLibrary (which needs relay stubs that
 * crash on PC-relative instructions), we check on every lua_call whether
 * Mini.* is registered. If not, inject it.
 *
 * Caches up to 8 lua_State pointers to avoid re-checking.
 */
#define MAX_INJECTED_STATES 8
static lua_State* g_injected_states[MAX_INJECTED_STATES] = {0};
static int g_injected_count = 0;

void sre_mini_ensure_injected(lua_State* L) {
    if (!L) return;
    if (!g_lua_createtable) return;  /* Lua API not ready */

    /* Check if already injected for this state */
    int i;
    for (i = 0; i < g_injected_count; i++) {
        if (g_injected_states[i] == L) return;  /* Already done */
    }

    /* Inject standard Lua libraries (math, table, os, debug, io) */
    extern void sre_open_std_libs(lua_State* L);
    sre_open_std_libs(L);

    /* Inject Mini.*, LNI.*, Components.*, fs tables */
    sre_register_mini_api(L);

    /* Cache this state */
    if (g_injected_count < MAX_INJECTED_STATES) {
        g_injected_states[g_injected_count++] = L;
    }
}

