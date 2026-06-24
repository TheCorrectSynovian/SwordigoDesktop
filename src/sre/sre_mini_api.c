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
 * External globals from sre_scene_update.c — player stats
 * ========================================================================= */
extern volatile int g_sre_player_hp;
extern volatile int g_sre_player_max_hp;
extern volatile int g_sre_player_mana;
extern volatile int g_sre_player_max_mana;
extern volatile int g_sre_player_coins;
extern volatile int g_sre_player_xp;
extern volatile int g_sre_player_level;
extern volatile int g_sre_player_atk_level;
extern volatile int g_sre_player_hp_level;
extern volatile int g_sre_player_mana_level;

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

/* Character modification requests — host polls these */
volatile int g_sre_char_set_pending = 0;
volatile int g_sre_char_set_field = 0;  /* 0=none, 1=level, 2=exp, 3=hp, 4=mana, 5=coins */
volatile int g_sre_char_set_value = 0;

/* Camera state — SRE-side globals, host polls g_sre_cam_set_pending */
volatile float g_sre_cam_x = 0.0f;
volatile float g_sre_cam_y = 0.0f;
volatile float g_sre_cam_z = 0.0f;
volatile int   g_sre_cam_set_pending = 0;
volatile float g_sre_cam_zoom = 1.0f;
volatile int   g_sre_cam_follow = 1;  /* 1 = follow hero, 0 = free */

/* RecreateHero requires calling Caver::GameSceneController::RecreateHero()
 * at engine offset. This needs a guest function callback mechanism that
 * doesn't exist yet. For now, set a flag that the host can poll. */
volatile int g_sre_recreate_hero_pending = 0;

/* =========================================================================
 * String helper (no libc)
 * ========================================================================= */
static int sre_streq(const char* a, const char* b) {
    if (!a || !b) return 0;
    while (*a && *b) {
        if (*a != *b) return 0;
        a++; b++;
    }
    return *a == *b;
}

/* =========================================================================
 * Mini.Health.* Lua Functions
 * ========================================================================= */

/* Mini.Health.CurrentMana() → number */
static int l_mini_health_current_mana(lua_State* L) {
    g_lua_pushnumber(L, (double)g_sre_player_mana);
    return 1;
}

/* Mini.Health.CurrentManaPercent() → number (0.0–1.0) */
static int l_mini_health_current_mana_percent(lua_State* L) {
    if (g_sre_player_max_mana > 0)
        g_lua_pushnumber(L, (double)g_sre_player_mana / (double)g_sre_player_max_mana);
    else
        g_lua_pushnumber(L, 0.0);
    return 1;
}

/* =========================================================================
 * Mini.Character.* Lua Functions — full player stats
 * ========================================================================= */

static int l_mini_char_get_level(lua_State* L) {
    g_lua_pushnumber(L, (double)g_sre_player_level);
    return 1;
}

static int l_mini_char_get_exp(lua_State* L) {
    g_lua_pushnumber(L, (double)g_sre_player_xp);
    return 1;
}

static int l_mini_char_get_health(lua_State* L) {
    g_lua_pushnumber(L, (double)g_sre_player_hp);
    return 1;
}

static int l_mini_char_get_max_health(lua_State* L) {
    g_lua_pushnumber(L, (double)g_sre_player_max_hp);
    return 1;
}

static int l_mini_char_get_mana(lua_State* L) {
    g_lua_pushnumber(L, (double)g_sre_player_mana);
    return 1;
}

static int l_mini_char_get_max_mana(lua_State* L) {
    g_lua_pushnumber(L, (double)g_sre_player_max_mana);
    return 1;
}

static int l_mini_char_get_coins(lua_State* L) {
    g_lua_pushnumber(L, (double)g_sre_player_coins);
    return 1;
}

static int l_mini_char_get_atk_level(lua_State* L) {
    g_lua_pushnumber(L, (double)g_sre_player_atk_level);
    return 1;
}

static int l_mini_char_get_hp_level(lua_State* L) {
    g_lua_pushnumber(L, (double)g_sre_player_hp_level);
    return 1;
}

static int l_mini_char_get_mana_level(lua_State* L) {
    g_lua_pushnumber(L, (double)g_sre_player_mana_level);
    return 1;
}

/* Setters — write to pending buffer, host polls */
static int l_mini_char_set_level(lua_State* L) {
    g_sre_char_set_value = (int)g_lua_tonumber(L, 1);
    g_sre_char_set_field = 1;
    g_sre_char_set_pending = 1;
    return 0;
}

static int l_mini_char_set_exp(lua_State* L) {
    g_sre_char_set_value = (int)g_lua_tonumber(L, 1);
    g_sre_char_set_field = 2;
    g_sre_char_set_pending = 1;
    return 0;
}

static int l_mini_char_set_health(lua_State* L) {
    g_sre_char_set_value = (int)g_lua_tonumber(L, 1);
    g_sre_char_set_field = 3;
    g_sre_char_set_pending = 1;
    return 0;
}

static int l_mini_char_set_mana(lua_State* L) {
    g_sre_char_set_value = (int)g_lua_tonumber(L, 1);
    g_sre_char_set_field = 4;
    g_sre_char_set_pending = 1;
    return 0;
}

static int l_mini_char_set_coins(lua_State* L) {
    g_sre_char_set_value = (int)g_lua_tonumber(L, 1);
    g_sre_char_set_field = 5;
    g_sre_char_set_pending = 1;
    return 0;
}

/* =========================================================================
 * Mini.Character.* Movement/Speed Stubs (Combatch mod compatibility)
 *
 * These track values in SRE globals. The host can poll them
 * to apply engine-side changes if desired.
 * ========================================================================= */
float g_sre_walk_speed = 1.0f;
float g_sre_run_speed = 1.0f;
float g_sre_jump_height = 1.0f;
int   g_sre_move_direction = 0;  /* 0=stopped, -1=left, 1=right */

static int l_mini_char_get_walk_speed(lua_State* L) {
    g_lua_pushnumber(L, (double)g_sre_walk_speed);
    return 1;
}

static int l_mini_char_set_walk_speed(lua_State* L) {
    g_sre_walk_speed = (float)g_lua_tonumber(L, 1);
    return 0;
}

static int l_mini_char_get_run_speed(lua_State* L) {
    g_lua_pushnumber(L, (double)g_sre_run_speed);
    return 1;
}

static int l_mini_char_set_run_speed(lua_State* L) {
    g_sre_run_speed = (float)g_lua_tonumber(L, 1);
    return 0;
}

static int l_mini_char_set_jump_height(lua_State* L) {
    g_sre_jump_height = (float)g_lua_tonumber(L, 1);
    return 0;
}

static int l_mini_char_get_jump_height(lua_State* L) {
    g_lua_pushnumber(L, (double)g_sre_jump_height);
    return 1;
}

/* Mini.Character.StartMovingToDirection(dir) — dir: -1=left, 1=right */
static int l_mini_char_start_moving(lua_State* L) {
    g_sre_move_direction = (int)g_lua_tonumber(L, 1);
    return 0;
}

/* Mini.Character.StopMovingToDirection() */
static int l_mini_char_stop_moving(lua_State* L) {
    (void)L;
    g_sre_move_direction = 0;
    return 0;
}

/* =========================================================================
 * Mini.SetTrinketColor(item_id, r, g, b, a, intensity)
 *
 * Stores trinket glow color in a packed table for host to read.
 * Format: item_id\0 followed by 5 floats (r,g,b,a,intensity)
 * ========================================================================= */
typedef struct {
    char item_id[32];
    float r, g, b, a;
    float intensity;
} SreTrinketGlow;

SreTrinketGlow g_sre_trinket_glows[16] = {{0}};
int g_sre_trinket_glow_count = 0;

static int l_mini_set_trinket_color(lua_State* L) {
    const char* item_id = lua_tostring(L, 1);
    if (!item_id) return 0;

    float r = (float)g_lua_tonumber(L, 2);
    float g = (float)g_lua_tonumber(L, 3);
    float b = (float)g_lua_tonumber(L, 4);
    float a = (float)g_lua_tonumber(L, 5);
    float intensity = (float)g_lua_tonumber(L, 6);

    /* Find existing or add new */
    int idx = -1;
    int i;
    for (i = 0; i < g_sre_trinket_glow_count; i++) {
        /* strcmp inline */
        const char* p = g_sre_trinket_glows[i].item_id;
        const char* q = item_id;
        int match = 1;
        while (*p || *q) {
            if (*p != *q) { match = 0; break; }
            p++; q++;
        }
        if (match) { idx = i; break; }
    }
    if (idx < 0 && g_sre_trinket_glow_count < 16) {
        idx = g_sre_trinket_glow_count++;
    }
    if (idx < 0) return 0;  /* table full */

    /* Copy item_id */
    for (i = 0; i < 31 && item_id[i]; i++)
        g_sre_trinket_glows[idx].item_id[i] = item_id[i];
    g_sre_trinket_glows[idx].item_id[i] = '\0';

    g_sre_trinket_glows[idx].r = r;
    g_sre_trinket_glows[idx].g = g;
    g_sre_trinket_glows[idx].b = b;
    g_sre_trinket_glows[idx].a = a;
    g_sre_trinket_glows[idx].intensity = intensity;
    return 0;
}

/* =========================================================================
 * Mini.Camera.* Lua Functions
 * ========================================================================= */

/* Mini.Camera.GetPosition() → table {x, y, z} */
static int l_mini_cam_get_position(lua_State* L) {
    g_lua_createtable(L, 0, 3);
    g_lua_pushnumber(L, (double)g_sre_cam_x);
    g_lua_setfield(L, -2, "x");
    g_lua_pushnumber(L, (double)g_sre_cam_y);
    g_lua_setfield(L, -2, "y");
    g_lua_pushnumber(L, (double)g_sre_cam_z);
    g_lua_setfield(L, -2, "z");
    return 1;
}

/* Mini.Camera.SetPosition(x, y, z) */
static int l_mini_cam_set_position(lua_State* L) {
    g_sre_cam_x = (float)g_lua_tonumber(L, 1);
    g_sre_cam_y = (float)g_lua_tonumber(L, 2);
    g_sre_cam_z = (float)g_lua_tonumber(L, 3);
    g_sre_cam_set_pending = 1;
    return 0;
}

/* Mini.Camera.GetZoom() → number */
static int l_mini_cam_get_zoom(lua_State* L) {
    g_lua_pushnumber(L, (double)g_sre_cam_zoom);
    return 1;
}

/* Mini.Camera.SetZoom(n) */
static int l_mini_cam_set_zoom(lua_State* L) {
    float z = (float)g_lua_tonumber(L, 1);
    if (z > 0.0f && z <= 20.0f)
        g_sre_cam_zoom = z;
    return 0;
}

/* Mini.Camera.GetFollow() → boolean */
static int l_mini_cam_get_follow(lua_State* L) {
    g_lua_pushboolean(L, g_sre_cam_follow);
    return 1;
}

/* Mini.Camera.SetFollow(bool) */
static int l_mini_cam_set_follow(lua_State* L) {
    g_sre_cam_follow = g_lua_toboolean(L, 1);
    return 0;
}

/* =========================================================================
 * Components.Health / Components.Physics / Components.Entity
 * ========================================================================= */

/* Components.Health.GetValue(entity, fieldName) */
static int l_comp_health_get_value(lua_State* L) {
    const char* field = lua_tostring(L, 2);
    if (!field) return 0;

    if (sre_streq(field, "CurrentHealth")) {
        g_lua_pushnumber(L, (double)g_sre_player_hp);
        return 1;
    }
    if (sre_streq(field, "MaxHealth")) {
        g_lua_pushnumber(L, (double)g_sre_player_max_hp);
        return 1;
    }
    if (sre_streq(field, "CurrentMana")) {
        g_lua_pushnumber(L, (double)g_sre_player_mana);
        return 1;
    }
    if (sre_streq(field, "MaxMana")) {
        g_lua_pushnumber(L, (double)g_sre_player_max_mana);
        return 1;
    }

    g_lua_pushnumber(L, 0.0);
    return 1;
}

/* Components.Health.SetValue(entity, fieldName, value) */
static int l_comp_health_set_value(lua_State* L) {
    const char* field = lua_tostring(L, 2);
    int val = (int)g_lua_tonumber(L, 3);
    if (!field) return 0;

    if (sre_streq(field, "CurrentHealth")) {
        g_sre_char_set_value = val;
        g_sre_char_set_field = 3;
        g_sre_char_set_pending = 1;
    } else if (sre_streq(field, "CurrentMana")) {
        g_sre_char_set_value = val;
        g_sre_char_set_field = 4;
        g_sre_char_set_pending = 1;
    }
    return 0;
}

/* Components.Physics.GetValue(entity, fieldName) — stub */
static int l_comp_physics_get_value(lua_State* L) {
    (void)L;
    g_lua_pushnumber(L, 0.0);
    return 1;
}

/* Components.Entity.GetValue(entity, fieldName) — stub */
static int l_comp_entity_get_value(lua_State* L) {
    (void)L;
    g_lua_pushnumber(L, 0.0);
    return 1;
}

/* =========================================================================
 * Skeleton.* Lua Functions (stubs)
 * ========================================================================= */

/* Skeleton.GetBonePosition(entity, boneName) → {0, 0, 0} */
static int l_skeleton_get_bone_position(lua_State* L) {
    (void)L;
    g_lua_createtable(L, 3, 0);
    g_lua_pushnumber(L, 0.0);
    if (g_lua_rawseti) g_lua_rawseti(L, -2, 1);
    else lua_pop(L, 1);
    g_lua_pushnumber(L, 0.0);
    if (g_lua_rawseti) g_lua_rawseti(L, -2, 2);
    else lua_pop(L, 1);
    g_lua_pushnumber(L, 0.0);
    if (g_lua_rawseti) g_lua_rawseti(L, -2, 3);
    else lua_pop(L, 1);
    return 1;
}

/* Skeleton.GetBoneRotation(entity, boneName) → {0, 0, 0, 1} (quaternion) */
static int l_skeleton_get_bone_rotation(lua_State* L) {
    (void)L;
    g_lua_createtable(L, 4, 0);
    g_lua_pushnumber(L, 0.0);
    if (g_lua_rawseti) g_lua_rawseti(L, -2, 1);
    else lua_pop(L, 1);
    g_lua_pushnumber(L, 0.0);
    if (g_lua_rawseti) g_lua_rawseti(L, -2, 2);
    else lua_pop(L, 1);
    g_lua_pushnumber(L, 0.0);
    if (g_lua_rawseti) g_lua_rawseti(L, -2, 3);
    else lua_pop(L, 1);
    g_lua_pushnumber(L, 1.0);
    if (g_lua_rawseti) g_lua_rawseti(L, -2, 4);
    else lua_pop(L, 1);
    return 1;
}

/* Skeleton.SetBoneScale(entity, boneName, sx, sy, sz) → 0 (no-op) */
static int l_skeleton_set_bone_scale(lua_State* L) {
    (void)L;
    g_lua_pushnumber(L, 0.0);
    return 1;
}

/* =========================================================================
 * CharAnimController.* Lua Functions (stubs)
 * ========================================================================= */

/* CharAnimController.Play(entity, animName) → no-op */
static int l_charanim_play(lua_State* L) {
    (void)L;
    return 0;
}

/* CharAnimController.Stop(entity) → no-op */
static int l_charanim_stop(lua_State* L) {
    (void)L;
    return 0;
}

/* CharAnimController.GetCurrent(entity) → "" */
static int l_charanim_get_current(lua_State* L) {
    (void)L;
    g_lua_pushstring(L, "");
    return 1;
}

/* =========================================================================
 * ButtonController.* Lua Functions (stubs)
 *
 * Mods heavily use ButtonController for custom UI. These stubs prevent
 * nil-call crashes. A real implementation would require host-side Android
 * View creation, which we don't have on desktop yet.
 * ========================================================================= */

/* ButtonController.New(id, label, x, y, w, h) → 0 */
static int l_btn_new(lua_State* L) {
    (void)L;
    g_lua_pushnumber(L, 0.0);
    return 1;
}

/* Generic no-op stub — used for many ButtonController functions */
static int l_btn_noop(lua_State* L) {
    (void)L;
    return 0;
}

/* ButtonController.IsPressed(id) → false */
static int l_btn_is_pressed(lua_State* L) {
    (void)L;
    g_lua_pushboolean(L, 0);
    return 1;
}

/* ButtonController.IsDragging(id) → false */
static int l_btn_is_dragging(lua_State* L) {
    (void)L;
    g_lua_pushboolean(L, 0);
    return 1;
}

/* ButtonController.Exists(id) → false */
static int l_btn_exists(lua_State* L) {
    (void)L;
    g_lua_pushboolean(L, 0);
    return 1;
}

/* ButtonController.GetPosition(id) → 0, 0 */
static int l_btn_get_position(lua_State* L) {
    (void)L;
    g_lua_pushnumber(L, 0.0);
    g_lua_pushnumber(L, 0.0);
    return 2;
}

/* ButtonController.GetPositionX(id) → 0 */
static int l_btn_get_position_x(lua_State* L) {
    (void)L;
    g_lua_pushnumber(L, 0.0);
    return 1;
}

/* ButtonController.GetPositionY(id) → 0 */
static int l_btn_get_position_y(lua_State* L) {
    (void)L;
    g_lua_pushnumber(L, 0.0);
    return 1;
}

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

/* =========================================================================
 * Coin Limit (Phase 3.5)
 * ========================================================================= */

/* Mini.SetCoinLimit(n) — set max coin count (host patches binary) */
static int l_mini_set_coin_limit(lua_State* L) {
    int n = (int)g_lua_tonumber(L, 1);
    if (n < 0) n = 0;
    if (n > 65535) n = 65535;
    g_sre_coin_limit = n;
    return 0;
}

/* Mini.GetCoinLimit() */
static int l_mini_get_coin_limit(lua_State* L) {
    g_lua_pushnumber(L, (double)g_sre_coin_limit);
    return 1;
}

/* Mini.ToggleDebug() */
static int l_mini_toggle_debug(lua_State* L) {
    (void)L;
    g_sre_debug_active = !g_sre_debug_active;
    return 0;
}

/* Mini.RecreateHero() — sets pending flag for host to poll.
 * The real implementation needs Caver::GameSceneController::RecreateHero()
 * called at the engine offset via a guest→host callback mechanism. */
static int l_mini_recreate_hero(lua_State* L) {
    (void)L;
    g_sre_recreate_hero_pending = 1;
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
 * Armor Model Swap System (Phase 3.2)
 * =========================================================================
 * Host reads these to override ModelNameForArmor results.
 * Format: packed key\0value\0 pairs like CString table.
 * ========================================================================= */
char g_sre_armor_models[2048] = {0};   /* item_id\0model_name\0...\0\0 */
int  g_sre_armor_model_count = 0;
char g_sre_default_player_model[64] = "hiro";

/* Mini.SetArmorModel(item_id, model_name) */
static int sre_mini_set_armor_model(lua_State* L) {
    const char* item_id = lua_tostring(L, 1);
    const char* model   = lua_tostring(L, 2);
    if (!item_id || !model) return 0;

    /* Find end of table */
    int pos = 0;
    int entries = 0;
    while (pos < 2046 && entries < g_sre_armor_model_count) {
        while (pos < 2046 && g_sre_armor_models[pos]) pos++;
        pos++;
        while (pos < 2046 && g_sre_armor_models[pos]) pos++;
        pos++;
        entries++;
    }

    /* Copy item_id */
    int len1 = 0;
    while (item_id[len1]) len1++;
    if (pos + len1 + 1 >= 2046) return 0;
    int i;
    for (i = 0; i <= len1; i++)
        g_sre_armor_models[pos + i] = item_id[i];
    pos += len1 + 1;

    /* Copy model_name */
    int len2 = 0;
    while (model[len2]) len2++;
    if (pos + len2 + 1 >= 2046) return 0;
    for (i = 0; i <= len2; i++)
        g_sre_armor_models[pos + i] = model[i];
    pos += len2 + 1;

    g_sre_armor_models[pos] = '\0';
    g_sre_armor_model_count++;
    return 0;
}

/* Mini.SetDefaultPlayerModel(model_name) */
static int sre_mini_set_default_model(lua_State* L) {
    const char* model = lua_tostring(L, 1);
    if (!model) return 0;
    int i;
    for (i = 0; i < 63 && model[i]; i++)
        g_sre_default_player_model[i] = model[i];
    g_sre_default_player_model[i] = '\0';
    return 0;
}

/* =========================================================================
 * Scene Event System (Phase 4.2)
 * =========================================================================
 * The host updates g_sre_current_scene_name whenever the scene changes.
 * We check for changes and call registered Lua callbacks.
 * ========================================================================= */
char g_sre_current_scene_name[128] = {0};
char g_sre_previous_scene_name[128] = {0};
int  g_sre_scene_changed = 0;     /* Host sets to 1 when scene changes */
int  g_sre_scene_callback_ref = 0; /* Lua registry ref for callback */

/* Mini.OnSceneChange(callback_function)
 * Stores the callback in the Lua registry at a fixed integer key.
 * We use raw registry key 91001 as our private slot. */
#define SRE_SCENE_CB_REGKEY 91001

static int sre_mini_on_scene_change(lua_State* L) {
    if (g_lua_type(L, 1) == LUA_TFUNCTION) {
        if (g_lua_pushvalue) g_lua_pushvalue(L, 1);
        if (g_lua_rawseti) g_lua_rawseti(L, LUA_REGISTRYINDEX, SRE_SCENE_CB_REGKEY);
        g_sre_scene_callback_ref = SRE_SCENE_CB_REGKEY;
    }
    return 0;
}

/* Mini.GetCurrentScene() */
static int sre_mini_get_current_scene(lua_State* L) {
    g_lua_pushstring(L, g_sre_current_scene_name);
    return 1;
}

/* Mini.GetPreviousScene() */
static int sre_mini_get_previous_scene(lua_State* L) {
    g_lua_pushstring(L, g_sre_previous_scene_name);
    return 1;
}

/* =========================================================================
 * Mod Info (Phase 4.3)
 * ========================================================================= */
char g_sre_mod_name[128] = {0};
char g_sre_mod_version[32] = {0};
char g_sre_mod_author[128] = {0};

/* Mini.GetModName() */
static int sre_mini_get_mod_name(lua_State* L) {
    g_lua_pushstring(L, g_sre_mod_name[0] ? g_sre_mod_name : "Unknown");
    return 1;
}

/* Mini.GetModVersion() */
static int sre_mini_get_mod_version(lua_State* L) {
    g_lua_pushstring(L, g_sre_mod_version[0] ? g_sre_mod_version : "1.0");
    return 1;
}

/* Mini.GetModAuthor() */
static int sre_mini_get_mod_author(lua_State* L) {
    g_lua_pushstring(L, g_sre_mod_author[0] ? g_sre_mod_author : "Unknown");
    return 1;
}

/* =========================================================================
 * Bauble System (Phase 3.3)
 * =========================================================================
 * Tracks equippable baubles with name, level, and equipped state.
 * Lua scripts access via the global Bauble table.
 * ========================================================================= */

typedef struct {
    char name[32];
    int level;
    int equipped;
} SreBauble;

SreBauble g_sre_baubles[32] = {{0}};
int g_sre_bauble_count = 0;

static SreBauble* sre_find_bauble(const char* name) {
    int i;
    for (i = 0; i < g_sre_bauble_count; i++) {
        if (sre_streq(g_sre_baubles[i].name, name)) return &g_sre_baubles[i];
    }
    return 0;
}

static SreBauble* sre_find_or_create_bauble(const char* name) {
    SreBauble* b = sre_find_bauble(name);
    if (b) return b;
    if (g_sre_bauble_count >= 32) return 0;
    b = &g_sre_baubles[g_sre_bauble_count++];
    int i;
    for (i = 0; name[i] && i < 31; i++) b->name[i] = name[i];
    b->name[i] = '\0';
    b->level = 0;
    b->equipped = 0;
    return b;
}

/* Bauble.Find(name) → table {name, level, equipped} or nil */
static int l_bauble_find(lua_State* L) {
    const char* name = lua_tostring(L, 1);
    if (!name) { g_lua_pushnil(L); return 1; }
    SreBauble* b = sre_find_bauble(name);
    if (!b) { g_lua_pushnil(L); return 1; }
    g_lua_createtable(L, 0, 3);
    g_lua_pushstring(L, b->name);
    g_lua_setfield(L, -2, "name");
    g_lua_pushnumber(L, (double)b->level);
    g_lua_setfield(L, -2, "level");
    g_lua_pushboolean(L, b->equipped);
    g_lua_setfield(L, -2, "equipped");
    return 1;
}

/* Bauble.Equip(name) — set equipped=1, create if not found */
static int l_bauble_equip(lua_State* L) {
    const char* name = lua_tostring(L, 1);
    if (!name) return 0;
    SreBauble* b = sre_find_or_create_bauble(name);
    if (b) b->equipped = 1;
    return 0;
}

/* Bauble.Unequip(name) — set equipped=0 */
static int l_bauble_unequip(lua_State* L) {
    const char* name = lua_tostring(L, 1);
    if (!name) return 0;
    SreBauble* b = sre_find_bauble(name);
    if (b) b->equipped = 0;
    return 0;
}

/* Bauble.IsWearing(name) → boolean */
static int l_bauble_is_wearing(lua_State* L) {
    const char* name = lua_tostring(L, 1);
    if (!name) { g_lua_pushboolean(L, 0); return 1; }
    SreBauble* b = sre_find_bauble(name);
    g_lua_pushboolean(L, b ? b->equipped : 0);
    return 1;
}

/* Bauble.GetLevel(name) → integer (0 if not found) */
static int l_bauble_get_level(lua_State* L) {
    const char* name = lua_tostring(L, 1);
    if (!name) { g_lua_pushnumber(L, 0.0); return 1; }
    SreBauble* b = sre_find_bauble(name);
    g_lua_pushnumber(L, b ? (double)b->level : 0.0);
    return 1;
}

/* Bauble.IncLevel(name) — increment level by 1, create with level=1 if not found */
static int l_bauble_inc_level(lua_State* L) {
    const char* name = lua_tostring(L, 1);
    if (!name) return 0;
    SreBauble* b = sre_find_or_create_bauble(name);
    if (b) b->level++;
    return 0;
}

/* Bauble.HideAll() — set all baubles' equipped=0 */
static int l_bauble_hide_all(lua_State* L) {
    (void)L;
    int i;
    for (i = 0; i < g_sre_bauble_count; i++) {
        g_sre_baubles[i].equipped = 0;
    }
    return 0;
}

/* =========================================================================
 * Achievement System (Phase 3.4)
 * =========================================================================
 * Tracks unlocked achievements with id, title, description.
 * Lua scripts access via Mini.Achievement sub-table.
 * Host polls g_sre_achievement_pending for popup display.
 * ========================================================================= */

typedef struct {
    char id[32];
    char title[64];
    char desc[128];
    int unlocked;
} SreAchievement;

SreAchievement g_sre_achievements[64] = {{0}};
int g_sre_achievement_count = 0;

/* Pending popup data — host polls this */
int g_sre_achievement_pending = 0;
char g_sre_achievement_pending_title[64] = {0};
char g_sre_achievement_pending_desc[128] = {0};

static SreAchievement* sre_find_achievement(const char* id) {
    int i;
    for (i = 0; i < g_sre_achievement_count; i++) {
        if (sre_streq(g_sre_achievements[i].id, id)) return &g_sre_achievements[i];
    }
    return 0;
}

static SreAchievement* sre_find_or_create_achievement(const char* id) {
    SreAchievement* a = sre_find_achievement(id);
    if (a) return a;
    if (g_sre_achievement_count >= 64) return 0;
    a = &g_sre_achievements[g_sre_achievement_count++];
    int i;
    for (i = 0; id[i] && i < 31; i++) a->id[i] = id[i];
    a->id[i] = '\0';
    a->title[0] = '\0';
    a->desc[0] = '\0';
    a->unlocked = 0;
    return a;
}

/* Mini.Achievement.Unlock(id, title, desc) — mark unlocked, set pending */
static int l_achievement_unlock(lua_State* L) {
    const char* id = lua_tostring(L, 1);
    if (!id) return 0;
    const char* title = lua_tostring(L, 2);
    const char* desc = lua_tostring(L, 3);

    SreAchievement* a = sre_find_or_create_achievement(id);
    if (!a) return 0;

    a->unlocked = 1;

    /* Copy title */
    if (title) {
        int i;
        for (i = 0; title[i] && i < 63; i++) a->title[i] = title[i];
        a->title[i] = '\0';
        /* Also copy to pending title */
        for (i = 0; title[i] && i < 63; i++) g_sre_achievement_pending_title[i] = title[i];
        g_sre_achievement_pending_title[i] = '\0';
    }

    /* Copy desc */
    if (desc) {
        int i;
        for (i = 0; desc[i] && i < 127; i++) a->desc[i] = desc[i];
        a->desc[i] = '\0';
        /* Also copy to pending desc */
        for (i = 0; desc[i] && i < 127; i++) g_sre_achievement_pending_desc[i] = desc[i];
        g_sre_achievement_pending_desc[i] = '\0';
    }

    g_sre_achievement_pending = 1;
    return 0;
}

/* Mini.Achievement.IsUnlocked(id) → boolean */
static int l_achievement_is_unlocked(lua_State* L) {
    const char* id = lua_tostring(L, 1);
    if (!id) { g_lua_pushboolean(L, 0); return 1; }
    SreAchievement* a = sre_find_achievement(id);
    g_lua_pushboolean(L, a ? a->unlocked : 0);
    return 1;
}

/* Mini.Achievement.GetAll() → table of achievement tables */
static int l_achievement_get_all(lua_State* L) {
    g_lua_createtable(L, g_sre_achievement_count, 0);
    int i;
    for (i = 0; i < g_sre_achievement_count; i++) {
        g_lua_createtable(L, 0, 4);
        g_lua_pushstring(L, g_sre_achievements[i].id);
        g_lua_setfield(L, -2, "id");
        g_lua_pushstring(L, g_sre_achievements[i].title);
        g_lua_setfield(L, -2, "title");
        g_lua_pushstring(L, g_sre_achievements[i].desc);
        g_lua_setfield(L, -2, "desc");
        g_lua_pushboolean(L, g_sre_achievements[i].unlocked);
        g_lua_setfield(L, -2, "unlocked");
        if (g_lua_rawseti) g_lua_rawseti(L, -2, i + 1);
        else lua_pop(L, 1);
    }
    return 1;
}

/* Mini.Achievement.Reset(id) — re-lock one achievement */
static int l_achievement_reset(lua_State* L) {
    const char* id = lua_tostring(L, 1);
    if (!id) return 0;
    SreAchievement* a = sre_find_achievement(id);
    if (a) a->unlocked = 0;
    return 0;
}

/* Mini.Achievement.ResetAll() — re-lock all achievements */
static int l_achievement_reset_all(lua_State* L) {
    (void)L;
    int i;
    for (i = 0; i < g_sre_achievement_count; i++) {
        g_sre_achievements[i].unlocked = 0;
    }
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
    g_lua_createtable(L, 0, 16);  /* Mini = {} */

    g_lua_pushcclosure(L, l_mini_arch, 0);
    g_lua_setfield(L, -2, "Arch");

    g_lua_pushcclosure(L, l_mini_get_profile_id, 0);
    g_lua_setfield(L, -2, "GetProfileID");

    g_lua_pushcclosure(L, l_mini_set_controls_hidden, 0);
    g_lua_setfield(L, -2, "SetControlsHidden");

    g_lua_pushcclosure(L, l_mini_set_trinket_color, 0);
    g_lua_setfield(L, -2, "SetTrinketColor");

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

    /* Phase 3.2: Armor models */
    g_lua_pushcclosure(L, sre_mini_set_armor_model, 0);
    g_lua_setfield(L, -2, "SetArmorModel");
    g_lua_pushcclosure(L, sre_mini_set_default_model, 0);
    g_lua_setfield(L, -2, "SetDefaultPlayerModel");

    /* Phase 3.5: Coin limit (getter) */
    g_lua_pushcclosure(L, l_mini_get_coin_limit, 0);
    g_lua_setfield(L, -2, "GetCoinLimit");

    /* Phase 4.2: Scene events */
    g_lua_pushcclosure(L, sre_mini_on_scene_change, 0);
    g_lua_setfield(L, -2, "OnSceneChange");
    g_lua_pushcclosure(L, sre_mini_get_current_scene, 0);
    g_lua_setfield(L, -2, "GetCurrentScene");
    g_lua_pushcclosure(L, sre_mini_get_previous_scene, 0);
    g_lua_setfield(L, -2, "GetPreviousScene");

    /* Phase 4.3: Mod info */
    g_lua_pushcclosure(L, sre_mini_get_mod_name, 0);
    g_lua_setfield(L, -2, "GetModName");
    g_lua_pushcclosure(L, sre_mini_get_mod_version, 0);
    g_lua_setfield(L, -2, "GetModVersion");
    g_lua_pushcclosure(L, sre_mini_get_mod_author, 0);
    g_lua_setfield(L, -2, "GetModAuthor");

    /* Mini.Health = {} (sub-table) */
    g_lua_createtable(L, 0, 4);
    g_lua_pushcclosure(L, l_mini_health_current_mana, 0);
    g_lua_setfield(L, -2, "CurrentMana");
    g_lua_pushcclosure(L, l_mini_health_current_mana_percent, 0);
    g_lua_setfield(L, -2, "CurrentManaPercent");
    g_lua_setfield(L, -2, "Health");

    /* Mini.Character = {} (sub-table) */
    g_lua_createtable(L, 0, 16);
    g_lua_pushcclosure(L, l_mini_char_get_level, 0);
    g_lua_setfield(L, -2, "GetLevel");
    g_lua_pushcclosure(L, l_mini_char_get_exp, 0);
    g_lua_setfield(L, -2, "GetExp");
    g_lua_pushcclosure(L, l_mini_char_get_health, 0);
    g_lua_setfield(L, -2, "GetHealth");
    g_lua_pushcclosure(L, l_mini_char_get_max_health, 0);
    g_lua_setfield(L, -2, "GetMaxHealth");
    g_lua_pushcclosure(L, l_mini_char_get_mana, 0);
    g_lua_setfield(L, -2, "GetMana");
    g_lua_pushcclosure(L, l_mini_char_get_max_mana, 0);
    g_lua_setfield(L, -2, "GetMaxMana");
    g_lua_pushcclosure(L, l_mini_char_get_coins, 0);
    g_lua_setfield(L, -2, "GetCoins");
    g_lua_pushcclosure(L, l_mini_char_get_atk_level, 0);
    g_lua_setfield(L, -2, "GetAttackLevel");
    g_lua_pushcclosure(L, l_mini_char_get_hp_level, 0);
    g_lua_setfield(L, -2, "GetHealthLevel");
    g_lua_pushcclosure(L, l_mini_char_get_mana_level, 0);
    g_lua_setfield(L, -2, "GetManaLevel");
    g_lua_pushcclosure(L, l_mini_char_set_level, 0);
    g_lua_setfield(L, -2, "SetLevel");
    g_lua_pushcclosure(L, l_mini_char_set_exp, 0);
    g_lua_setfield(L, -2, "SetExp");
    g_lua_pushcclosure(L, l_mini_char_set_health, 0);
    g_lua_setfield(L, -2, "SetHealth");
    g_lua_pushcclosure(L, l_mini_char_set_mana, 0);
    g_lua_setfield(L, -2, "SetMana");
    g_lua_pushcclosure(L, l_mini_char_set_coins, 0);
    g_lua_setfield(L, -2, "SetCoins");
    g_lua_pushcclosure(L, l_mini_char_get_walk_speed, 0);
    g_lua_setfield(L, -2, "GetWalkSpeed");
    g_lua_pushcclosure(L, l_mini_char_set_walk_speed, 0);
    g_lua_setfield(L, -2, "SetWalkSpeed");
    g_lua_pushcclosure(L, l_mini_char_get_run_speed, 0);
    g_lua_setfield(L, -2, "GetRunSpeed");
    g_lua_pushcclosure(L, l_mini_char_set_run_speed, 0);
    g_lua_setfield(L, -2, "SetRunSpeed");
    g_lua_pushcclosure(L, l_mini_char_set_jump_height, 0);
    g_lua_setfield(L, -2, "SetJumpHeight");
    g_lua_pushcclosure(L, l_mini_char_get_jump_height, 0);
    g_lua_setfield(L, -2, "GetJumpHeight");
    g_lua_pushcclosure(L, l_mini_char_start_moving, 0);
    g_lua_setfield(L, -2, "StartMovingToDirection");
    g_lua_pushcclosure(L, l_mini_char_stop_moving, 0);
    g_lua_setfield(L, -2, "StopMovingToDirection");
    g_lua_setfield(L, -2, "Character");

    /* Mini.Camera = {} (sub-table) */
    g_lua_createtable(L, 0, 8);
    g_lua_pushcclosure(L, l_mini_cam_get_position, 0);
    g_lua_setfield(L, -2, "GetPosition");
    g_lua_pushcclosure(L, l_mini_cam_set_position, 0);
    g_lua_setfield(L, -2, "SetPosition");
    g_lua_pushcclosure(L, l_mini_cam_get_zoom, 0);
    g_lua_setfield(L, -2, "GetZoom");
    g_lua_pushcclosure(L, l_mini_cam_set_zoom, 0);
    g_lua_setfield(L, -2, "SetZoom");
    g_lua_pushcclosure(L, l_mini_cam_get_follow, 0);
    g_lua_setfield(L, -2, "GetFollow");
    g_lua_pushcclosure(L, l_mini_cam_set_follow, 0);
    g_lua_setfield(L, -2, "SetFollow");
    g_lua_setfield(L, -2, "Camera");

    /* Mini.Achievement = {} (sub-table, Phase 3.4) */
    g_lua_createtable(L, 0, 5);
    g_lua_pushcclosure(L, l_achievement_unlock, 0);
    g_lua_setfield(L, -2, "Unlock");
    g_lua_pushcclosure(L, l_achievement_is_unlocked, 0);
    g_lua_setfield(L, -2, "IsUnlocked");
    g_lua_pushcclosure(L, l_achievement_get_all, 0);
    g_lua_setfield(L, -2, "GetAll");
    g_lua_pushcclosure(L, l_achievement_reset, 0);
    g_lua_setfield(L, -2, "Reset");
    g_lua_pushcclosure(L, l_achievement_reset_all, 0);
    g_lua_setfield(L, -2, "ResetAll");
    g_lua_setfield(L, -2, "Achievement");

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

    /* ---- Components table ---- */
    g_lua_createtable(L, 0, 4);

    /* Components.Health = { GetValue, SetValue } */
    g_lua_createtable(L, 0, 4);
    g_lua_pushcclosure(L, l_comp_health_get_value, 0);
    g_lua_setfield(L, -2, "GetValue");
    g_lua_pushcclosure(L, l_comp_health_set_value, 0);
    g_lua_setfield(L, -2, "SetValue");
    g_lua_setfield(L, -2, "Health");

    /* Components.Physics = { GetValue } */
    g_lua_createtable(L, 0, 4);
    g_lua_pushcclosure(L, l_comp_physics_get_value, 0);
    g_lua_setfield(L, -2, "GetValue");
    g_lua_setfield(L, -2, "Physics");

    /* Components.Entity = { GetValue } */
    g_lua_createtable(L, 0, 4);
    g_lua_pushcclosure(L, l_comp_entity_get_value, 0);
    g_lua_setfield(L, -2, "GetValue");
    g_lua_setfield(L, -2, "Entity");

    g_lua_setfield(L, LUA_GLOBALSINDEX, "Components");

    /* ---- Skeleton table (stubs) ---- */
    g_lua_createtable(L, 0, 4);
    g_lua_pushcclosure(L, l_skeleton_get_bone_position, 0);
    g_lua_setfield(L, -2, "GetBonePosition");
    g_lua_pushcclosure(L, l_skeleton_get_bone_rotation, 0);
    g_lua_setfield(L, -2, "GetBoneRotation");
    g_lua_pushcclosure(L, l_skeleton_set_bone_scale, 0);
    g_lua_setfield(L, -2, "SetBoneScale");
    g_lua_setfield(L, LUA_GLOBALSINDEX, "Skeleton");

    /* ---- CharAnimController table (stubs) ---- */
    g_lua_createtable(L, 0, 4);
    g_lua_pushcclosure(L, l_charanim_play, 0);
    g_lua_setfield(L, -2, "Play");
    g_lua_pushcclosure(L, l_charanim_stop, 0);
    g_lua_setfield(L, -2, "Stop");
    g_lua_pushcclosure(L, l_charanim_get_current, 0);
    g_lua_setfield(L, -2, "GetCurrent");
    g_lua_setfield(L, LUA_GLOBALSINDEX, "CharAnimController");

    /* ---- Bauble table (Phase 3.3) ---- */
    g_lua_createtable(L, 0, 8);
    g_lua_pushcclosure(L, l_bauble_find, 0);
    g_lua_setfield(L, -2, "Find");
    g_lua_pushcclosure(L, l_bauble_equip, 0);
    g_lua_setfield(L, -2, "Equip");
    g_lua_pushcclosure(L, l_bauble_equip, 0);  /* Wear is alias for Equip */
    g_lua_setfield(L, -2, "Wear");
    g_lua_pushcclosure(L, l_bauble_unequip, 0);
    g_lua_setfield(L, -2, "Unequip");
    g_lua_pushcclosure(L, l_bauble_is_wearing, 0);
    g_lua_setfield(L, -2, "IsWearing");
    g_lua_pushcclosure(L, l_bauble_get_level, 0);
    g_lua_setfield(L, -2, "GetLevel");
    g_lua_pushcclosure(L, l_bauble_inc_level, 0);
    g_lua_setfield(L, -2, "IncLevel");
    g_lua_pushcclosure(L, l_bauble_hide_all, 0);
    g_lua_setfield(L, -2, "HideAll");
    g_lua_setfield(L, LUA_GLOBALSINDEX, "Bauble");

    /* ---- ButtonController table (stubs) ---- */
    g_lua_createtable(L, 0, 26);
    g_lua_pushcclosure(L, l_btn_new, 0);
    g_lua_setfield(L, -2, "New");
    g_lua_pushcclosure(L, l_btn_noop, 0);
    g_lua_setfield(L, -2, "Delete");
    g_lua_pushcclosure(L, l_btn_noop, 0);
    g_lua_setfield(L, -2, "SetHidden");
    g_lua_pushcclosure(L, l_btn_is_pressed, 0);
    g_lua_setfield(L, -2, "IsPressed");
    g_lua_pushcclosure(L, l_btn_is_dragging, 0);
    g_lua_setfield(L, -2, "IsDragging");
    g_lua_pushcclosure(L, l_btn_exists, 0);
    g_lua_setfield(L, -2, "Exists");
    g_lua_pushcclosure(L, l_btn_noop, 0);
    g_lua_setfield(L, -2, "SetText");
    g_lua_pushcclosure(L, l_btn_noop, 0);
    g_lua_setfield(L, -2, "SetPosition");
    g_lua_pushcclosure(L, l_btn_get_position, 0);
    g_lua_setfield(L, -2, "GetPosition");
    g_lua_pushcclosure(L, l_btn_noop, 0);
    g_lua_setfield(L, -2, "DeleteAll");
    g_lua_pushcclosure(L, l_btn_noop, 0);
    g_lua_setfield(L, -2, "SetAlpha");
    g_lua_pushcclosure(L, l_btn_noop, 0);
    g_lua_setfield(L, -2, "SetScaling");
    g_lua_pushcclosure(L, l_btn_noop, 0);
    g_lua_setfield(L, -2, "SetDimensions");
    g_lua_pushcclosure(L, l_btn_noop, 0);
    g_lua_setfield(L, -2, "MakeMovable");
    g_lua_pushcclosure(L, l_btn_noop, 0);
    g_lua_setfield(L, -2, "SetClickable");
    g_lua_pushcclosure(L, l_btn_get_position_x, 0);
    g_lua_setfield(L, -2, "GetPositionX");
    g_lua_pushcclosure(L, l_btn_get_position_y, 0);
    g_lua_setfield(L, -2, "GetPositionY");
    g_lua_pushcclosure(L, l_btn_noop, 0);
    g_lua_setfield(L, -2, "SetTextFont");
    g_lua_pushcclosure(L, l_btn_noop, 0);
    g_lua_setfield(L, -2, "SetTextScale");
    g_lua_pushcclosure(L, l_btn_noop, 0);
    g_lua_setfield(L, -2, "SetTextColor");
    g_lua_pushcclosure(L, l_btn_noop, 0);
    g_lua_setfield(L, -2, "SetPadding");
    g_lua_pushcclosure(L, l_btn_noop, 0);
    g_lua_setfield(L, -2, "SetAlignment");
    g_lua_pushcclosure(L, l_btn_noop, 0);
    g_lua_setfield(L, -2, "SetBackgroundResource");
    g_lua_pushcclosure(L, l_btn_noop, 0);
    g_lua_setfield(L, -2, "SetBackgroundAlpha");
    g_lua_setfield(L, LUA_GLOBALSINDEX, "ButtonController");

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

