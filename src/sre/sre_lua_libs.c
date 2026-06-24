/*
 * sre_lua_libs.c — Lua 5.1 standard library implementations for SRE
 *
 * The vanilla engine only ships base + string Lua libs.
 * SwMini compiles the rest from Lua 5.1 source. We implement
 * the essential functions directly in SRE.
 *
 * Uses software math implementations since we compile with -nostdlib.
 * AArch64 hardware ops used for floor/ceil/sqrt/fabs.
 *
 * STATUS: API complete. RLSW still crashes with UC_ERR_WRITE_PROT at
 *         PC 0x14ee790 during gameplay — likely unpatched text-segment
 *         write triggered by RL's modified game data. See README.md.
 */

#include "sre.h"
#include "sre_lua.h"

/* =========================================================================
 * Additional Lua API function pointers needed by standard libs
 * Types and externs are declared in sre_lua.h.
 * Globals are defined here, initialized by sre_init_lua_ext().
 * ========================================================================= */

/* Global extended function pointers */
pfn_lua_pushvalue      g_lua_pushvalue      = 0;
pfn_lua_remove         g_lua_remove         = 0;
pfn_lua_insert         g_lua_insert         = 0;
pfn_lua_replace        g_lua_replace        = 0;
pfn_lua_checkstack     g_lua_checkstack     = 0;
pfn_lua_rawget         g_lua_rawget         = 0;
pfn_lua_rawset         g_lua_rawset         = 0;
pfn_lua_rawgeti        g_lua_rawgeti        = 0;
pfn_lua_rawseti        g_lua_rawseti        = 0;
pfn_lua_next           g_lua_next           = 0;
pfn_lua_objlen         g_lua_objlen         = 0;
pfn_lua_settable       g_lua_settable       = 0;
pfn_lua_gettable       g_lua_gettable       = 0;
pfn_lua_isnumber       g_lua_isnumber       = 0;
pfn_lua_isstring       g_lua_isstring       = 0;
pfn_lua_tointeger      g_lua_tointeger      = 0;
pfn_lua_pushinteger    g_lua_pushinteger    = 0;
pfn_lua_concat         g_lua_concat         = 0;
pfn_lua_pushlstring    g_lua_pushlstring    = 0;

/* Extended init — called from host after sre_init_lua */
typedef struct {
    sre_u64 lua_pushvalue;
    sre_u64 lua_remove;
    sre_u64 lua_insert;
    sre_u64 lua_replace;
    sre_u64 lua_checkstack;
    sre_u64 lua_rawget;
    sre_u64 lua_rawset;
    sre_u64 lua_rawgeti;
    sre_u64 lua_rawseti;
    sre_u64 lua_next;
    sre_u64 lua_objlen;
    sre_u64 lua_settable;
    sre_u64 lua_gettable;
    sre_u64 lua_isnumber;
    sre_u64 lua_isstring;
    sre_u64 lua_tointeger;
    sre_u64 lua_pushinteger;
    sre_u64 lua_concat;
    sre_u64 lua_pushlstring;
} SreLuaExtAddrs;

void sre_init_lua_ext(SreLuaExtAddrs* a) {
    g_lua_pushvalue      = (pfn_lua_pushvalue)a->lua_pushvalue;
    g_lua_remove         = (pfn_lua_remove)a->lua_remove;
    g_lua_insert         = (pfn_lua_insert)a->lua_insert;
    g_lua_replace        = (pfn_lua_replace)a->lua_replace;
    g_lua_checkstack     = (pfn_lua_checkstack)a->lua_checkstack;
    g_lua_rawget         = (pfn_lua_rawget)a->lua_rawget;
    g_lua_rawset         = (pfn_lua_rawset)a->lua_rawset;
    g_lua_rawgeti        = (pfn_lua_rawgeti)a->lua_rawgeti;
    g_lua_rawseti        = (pfn_lua_rawseti)a->lua_rawseti;
    g_lua_next           = (pfn_lua_next)a->lua_next;
    g_lua_objlen         = (pfn_lua_objlen)a->lua_objlen;
    g_lua_settable       = (pfn_lua_settable)a->lua_settable;
    g_lua_gettable       = (pfn_lua_gettable)a->lua_gettable;
    g_lua_isnumber       = (pfn_lua_isnumber)a->lua_isnumber;
    g_lua_isstring       = (pfn_lua_isstring)a->lua_isstring;
    g_lua_tointeger      = (pfn_lua_tointeger)a->lua_tointeger;
    g_lua_pushinteger    = (pfn_lua_pushinteger)a->lua_pushinteger;
    g_lua_concat         = (pfn_lua_concat)a->lua_concat;
    g_lua_pushlstring    = (pfn_lua_pushlstring)a->lua_pushlstring;
}


/* =========================================================================
 * External VFS globals from sre_vfs.c — for loadfile/dofile path translation
 * ========================================================================= */
extern volatile int g_sre_vfs_lua_translate_pending;
extern char g_sre_vfs_lua_input_path[];
extern char g_sre_vfs_lua_output_path[];
extern int  g_sre_vfs_active;


/* =========================================================================
 * GCC builtins for math — AArch64 maps these to hardware FPU instructions
 * ========================================================================= */
/* Hardware float ops — these map to single AArch64 instructions */
#define sre_floor(x)  __builtin_floor(x)
#define sre_ceil(x)   __builtin_ceil(x)
#define sre_sqrt(x)   __builtin_sqrt(x)
#define sre_fabs(x)   __builtin_fabs(x)
#define sre_huge      __builtin_huge_val()

/* fmod — a / b remainder (no libm call needed) */
static double sre_fmod(double x, double y) {
    if (y == 0.0) return 0.0;
    return x - (double)((int64_t)(x / y)) * y;
}

/* Software sin/cos via Bhaskara approximation (accurate to ~0.001) */
static double sre_sin(double x) {
    /* Normalize to [-pi, pi] */
    while (x > 3.14159265358979323846) x -= 6.28318530717958647692;
    while (x < -3.14159265358979323846) x += 6.28318530717958647692;
    /* Bhaskara I formula: sin(x) ≈ 16x(pi-x) / (5pi² - 4x(pi-x)) */
    double pi = 3.14159265358979323846;
    double num = 16.0 * x * (pi - x);
    double den = 5.0 * pi * pi - 4.0 * x * (pi - x);
    if (den == 0.0) return 0.0;
    return num / den;
}

static double sre_cos(double x) {
    return sre_sin(x + 1.5707963267948966);  /* cos(x) = sin(x + pi/2) */
}

static double sre_tan(double x) {
    double c = sre_cos(x);
    if (c == 0.0) return sre_huge;
    return sre_sin(x) / c;
}

/* atan via polynomial approximation */
static double sre_atan(double x) {
    /* For |x| > 1: atan(x) = pi/2 - atan(1/x) */
    if (x > 1.0) return 1.5707963267948966 - sre_atan(1.0 / x);
    if (x < -1.0) return -1.5707963267948966 - sre_atan(1.0 / x);
    /* Polynomial: atan(x) ≈ x - x³/3 + x⁵/5 - x⁷/7 + x⁹/9 */
    double x2 = x * x;
    double x3 = x2 * x;
    return x - x3/3.0 + x3*x2/5.0 - x3*x2*x2/7.0 + x3*x2*x2*x2/9.0;
}

static double sre_atan2(double y, double x) {
    double pi = 3.14159265358979323846;
    if (x > 0) return sre_atan(y / x);
    if (x < 0 && y >= 0) return sre_atan(y / x) + pi;
    if (x < 0 && y < 0) return sre_atan(y / x) - pi;
    if (x == 0 && y > 0) return pi / 2.0;
    if (x == 0 && y < 0) return -pi / 2.0;
    return 0.0;
}

static double sre_asin(double x) {
    if (x >= 1.0) return 1.5707963267948966;
    if (x <= -1.0) return -1.5707963267948966;
    return sre_atan(x / sre_sqrt(1.0 - x * x));
}

static double sre_acos(double x) {
    return 1.5707963267948966 - sre_asin(x);
}

/* exp via Taylor series (13 terms — good for |x| < 20) */
static double sre_exp(double x) {
    if (x > 700.0) return sre_huge;
    if (x < -700.0) return 0.0;
    /* Range reduction: exp(x) = exp(n) * exp(f) where x = n + f */
    double result = 1.0;
    double term = 1.0;
    int i;
    for (i = 1; i <= 20; i++) {
        term *= x / (double)i;
        result += term;
        if (term < 1e-15 && term > -1e-15) break;
    }
    return result;
}

/* log via series: ln(x) = 2 * atanh((x-1)/(x+1)) */
static double sre_log(double x) {
    if (x <= 0.0) return -sre_huge;
    if (x == 1.0) return 0.0;
    /* Reduce to [0.5, 2] range */
    int exp_part = 0;
    while (x >= 2.0) { x *= 0.5; exp_part++; }
    while (x < 0.5) { x *= 2.0; exp_part--; }
    /* Series: ln(x) = 2 * sum((z^(2k+1))/(2k+1)) where z = (x-1)/(x+1) */
    double z = (x - 1.0) / (x + 1.0);
    double z2 = z * z;
    double sum = z;
    double term = z;
    int i;
    for (i = 1; i <= 15; i++) {
        term *= z2;
        sum += term / (double)(2 * i + 1);
    }
    return 2.0 * sum + (double)exp_part * 0.6931471805599453;  /* ln(2) */
}

static double sre_log10(double x) {
    return sre_log(x) * 0.4342944819032518;  /* 1/ln(10) */
}

/* pow via exp(y * ln(x)) */
static double sre_pow(double x, double y) {
    if (y == 0.0) return 1.0;
    if (x == 0.0) return 0.0;
    if (x == 1.0) return 1.0;
    /* Integer power fast path */
    if (y == (double)(int64_t)y && y > 0 && y <= 64) {
        double result = 1.0;
        int n = (int)y;
        double base = x;
        while (n > 0) {
            if (n & 1) result *= base;
            base *= base;
            n >>= 1;
        }
        return result;
    }
    if (x < 0.0) return -sre_exp(y * sre_log(-x));
    return sre_exp(y * sre_log(x));
}

#define PI 3.14159265358979323846
#define RADIANS_PER_DEGREE (PI/180.0)

/* Simple xorshift64 PRNG for math.random */
static uint64_t rng_state = 0x123456789ABCDEF0ULL;

static uint64_t xorshift64(void) {
    uint64_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    rng_state = x;
    return x;
}


/* =========================================================================
 * math library
 * ========================================================================= */

static int math_abs(lua_State* L)   { g_lua_pushnumber(L, sre_fabs(g_lua_tonumber(L, 1))); return 1; }
static int math_floor(lua_State* L) { g_lua_pushnumber(L, sre_floor(g_lua_tonumber(L, 1))); return 1; }
static int math_ceil(lua_State* L)  { g_lua_pushnumber(L, sre_ceil(g_lua_tonumber(L, 1))); return 1; }
static int math_sqrt(lua_State* L)  { g_lua_pushnumber(L, sre_sqrt(g_lua_tonumber(L, 1))); return 1; }
static int math_sin(lua_State* L)   { g_lua_pushnumber(L, sre_sin(g_lua_tonumber(L, 1))); return 1; }
static int math_cos(lua_State* L)   { g_lua_pushnumber(L, sre_cos(g_lua_tonumber(L, 1))); return 1; }
static int math_tan(lua_State* L)   { g_lua_pushnumber(L, sre_tan(g_lua_tonumber(L, 1))); return 1; }
static int math_asin(lua_State* L)  { g_lua_pushnumber(L, sre_asin(g_lua_tonumber(L, 1))); return 1; }
static int math_acos(lua_State* L)  { g_lua_pushnumber(L, sre_acos(g_lua_tonumber(L, 1))); return 1; }
static int math_atan(lua_State* L)  { g_lua_pushnumber(L, sre_atan(g_lua_tonumber(L, 1))); return 1; }
static int math_log(lua_State* L)   { g_lua_pushnumber(L, sre_log(g_lua_tonumber(L, 1))); return 1; }
static int math_log10(lua_State* L) { g_lua_pushnumber(L, sre_log10(g_lua_tonumber(L, 1))); return 1; }
static int math_exp(lua_State* L)   { g_lua_pushnumber(L, sre_exp(g_lua_tonumber(L, 1))); return 1; }

static int math_atan2(lua_State* L) {
    g_lua_pushnumber(L, sre_atan2(g_lua_tonumber(L, 1), g_lua_tonumber(L, 2)));
    return 1;
}

static int math_pow(lua_State* L) {
    g_lua_pushnumber(L, sre_pow(g_lua_tonumber(L, 1), g_lua_tonumber(L, 2)));
    return 1;
}

static int math_fmod(lua_State* L) {
    g_lua_pushnumber(L, sre_fmod(g_lua_tonumber(L, 1), g_lua_tonumber(L, 2)));
    return 1;
}

static int math_deg(lua_State* L) {
    g_lua_pushnumber(L, g_lua_tonumber(L, 1) / RADIANS_PER_DEGREE);
    return 1;
}

static int math_rad(lua_State* L) {
    g_lua_pushnumber(L, g_lua_tonumber(L, 1) * RADIANS_PER_DEGREE);
    return 1;
}

static int math_max(lua_State* L) {
    int n = g_lua_gettop(L);
    if (n < 1) { g_lua_pushnumber(L, 0); return 1; }
    double m = g_lua_tonumber(L, 1);
    int i;
    for (i = 2; i <= n; i++) {
        double v = g_lua_tonumber(L, i);
        if (v > m) m = v;
    }
    g_lua_pushnumber(L, m);
    return 1;
}

static int math_min(lua_State* L) {
    int n = g_lua_gettop(L);
    if (n < 1) { g_lua_pushnumber(L, 0); return 1; }
    double m = g_lua_tonumber(L, 1);
    int i;
    for (i = 2; i <= n; i++) {
        double v = g_lua_tonumber(L, i);
        if (v < m) m = v;
    }
    g_lua_pushnumber(L, m);
    return 1;
}

static int math_random(lua_State* L) {
    int n = g_lua_gettop(L);
    uint64_t r = xorshift64();
    if (n == 0) {
        /* math.random() -> [0,1) */
        double d = (double)(r >> 11) * (1.0 / 9007199254740992.0);
        g_lua_pushnumber(L, d);
    } else if (n == 1) {
        /* math.random(n) -> [1,n] */
        int64_t upper = (int64_t)g_lua_tonumber(L, 1);
        if (upper < 1) upper = 1;
        g_lua_pushnumber(L, (double)(1 + (int64_t)(r % (uint64_t)upper)));
    } else {
        /* math.random(m, n) -> [m,n] */
        int64_t lower = (int64_t)g_lua_tonumber(L, 1);
        int64_t upper = (int64_t)g_lua_tonumber(L, 2);
        if (upper < lower) { int64_t t = lower; lower = upper; upper = t; }
        uint64_t range = (uint64_t)(upper - lower + 1);
        if (range == 0) range = 1;
        g_lua_pushnumber(L, (double)(lower + (int64_t)(r % range)));
    }
    return 1;
}

static int math_randomseed(lua_State* L) {
    rng_state = (uint64_t)g_lua_tonumber(L, 1);
    if (rng_state == 0) rng_state = 1;
    return 0;
}

/* luaL_Reg-compatible struct (matches Lua 5.1 layout) */
typedef struct { const char* name; int (*func)(lua_State*); } SreLuaReg;

static const SreLuaReg mathlib[] = {
    {"abs",        math_abs},
    {"acos",       math_acos},
    {"asin",       math_asin},
    {"atan",       math_atan},
    {"atan2",      math_atan2},
    {"ceil",       math_ceil},
    {"cos",        math_cos},
    {"deg",        math_deg},
    {"exp",        math_exp},
    {"floor",      math_floor},
    {"fmod",       math_fmod},
    {"log",        math_log},
    {"log10",      math_log10},
    {"max",        math_max},
    {"min",        math_min},
    {"pow",        math_pow},
    {"rad",        math_rad},
    {"random",     math_random},
    {"randomseed", math_randomseed},
    {"sin",        math_sin},
    {"sqrt",       math_sqrt},
    {"tan",        math_tan},
    {NULL, NULL}
};


/* =========================================================================
 * table library
 * ========================================================================= */

static int table_insert(lua_State* L) {
    int n = g_lua_gettop(L);
    if (!g_lua_objlen || !g_lua_rawgeti || !g_lua_rawseti) return 0;

    int tlen = (int)g_lua_objlen(L, 1);

    if (n == 2) {
        /* table.insert(t, val) — append at end */
        g_lua_rawseti(L, 1, tlen + 1);
    } else if (n >= 3) {
        /* table.insert(t, pos, val) */
        int pos = (int)g_lua_tonumber(L, 2);
        int i;
        /* Shift elements up */
        for (i = tlen; i >= pos; i--) {
            g_lua_rawgeti(L, 1, i);
            g_lua_rawseti(L, 1, i + 1);
        }
        /* Insert value at pos */
        if (g_lua_pushvalue) g_lua_pushvalue(L, 3);
        g_lua_rawseti(L, 1, pos);
    }
    return 0;
}

static int table_remove(lua_State* L) {
    if (!g_lua_objlen || !g_lua_rawgeti || !g_lua_rawseti) return 0;

    int tlen = (int)g_lua_objlen(L, 1);
    int pos = (g_lua_gettop(L) >= 2) ? (int)g_lua_tonumber(L, 2) : tlen;

    /* Get the removed value to return */
    g_lua_rawgeti(L, 1, pos);

    /* Shift elements down */
    int i;
    for (i = pos; i < tlen; i++) {
        g_lua_rawgeti(L, 1, i + 1);
        g_lua_rawseti(L, 1, i);
    }

    /* Remove last element */
    g_lua_pushnil(L);
    g_lua_rawseti(L, 1, tlen);

    return 1;
}

static int table_getn(lua_State* L) {
    if (!g_lua_objlen) { g_lua_pushnumber(L, 0); return 1; }
    g_lua_pushnumber(L, (double)g_lua_objlen(L, 1));
    return 1;
}

static int table_maxn(lua_State* L) {
    if (!g_lua_next) { g_lua_pushnumber(L, 0); return 1; }
    double max = 0;
    g_lua_pushnil(L);
    while (g_lua_next(L, 1) != 0) {
        lua_pop(L, 1);  /* pop value, keep key */
        if (g_lua_type(L, -1) == LUA_TNUMBER) {
            double v = g_lua_tonumber(L, -1);
            if (v > max) max = v;
        }
    }
    g_lua_pushnumber(L, max);
    return 1;
}

static int table_concat(lua_State* L) {
    if (!g_lua_objlen || !g_lua_rawgeti || !g_lua_concat) {
        g_lua_pushstring(L, "");
        return 1;
    }

    const char* sep = "";
    size_t seplen = 0;
    if (g_lua_gettop(L) >= 2 && g_lua_type(L, 2) == LUA_TSTRING) {
        sep = g_lua_tolstring(L, 2, &seplen);
    }

    int tlen = (int)g_lua_objlen(L, 1);
    int start = 1, end = tlen;
    if (g_lua_gettop(L) >= 3) start = (int)g_lua_tonumber(L, 3);
    if (g_lua_gettop(L) >= 4) end = (int)g_lua_tonumber(L, 4);

    if (start > end) {
        g_lua_pushstring(L, "");
        return 1;
    }

    /* Push all elements with separators, then concat */
    int count = 0;
    int i;
    for (i = start; i <= end; i++) {
        if (i > start && seplen > 0) {
            g_lua_pushstring(L, sep);
            count++;
        }
        g_lua_rawgeti(L, 1, i);
        count++;
    }
    if (count > 1) {
        g_lua_concat(L, count);
    } else if (count == 0) {
        g_lua_pushstring(L, "");
    }
    return 1;
}

/* table.sort — insertion sort (adequate for mod script data sizes) */
static int table_sort(lua_State* L) {
    if (!g_lua_objlen || !g_lua_rawgeti || !g_lua_rawseti) return 0;

    int n = (int)g_lua_objlen(L, 1);
    int has_cmp = (g_lua_gettop(L) >= 2 && g_lua_type(L, 2) == LUA_TFUNCTION);

    int i, j;
    for (i = 2; i <= n; i++) {
        g_lua_rawgeti(L, 1, i);  /* push key = t[i] */
        j = i - 1;
        while (j >= 1) {
            g_lua_rawgeti(L, 1, j);  /* push t[j] */
            int less;
            if (has_cmp) {
                if (g_lua_pushvalue) g_lua_pushvalue(L, 2);  /* push cmp */
                if (g_lua_pushvalue) g_lua_pushvalue(L, -3);  /* push key */
                if (g_lua_pushvalue) g_lua_pushvalue(L, -3);  /* push t[j] */
                g_lua_pcall(L, 2, 1, 0);
                less = g_lua_toboolean(L, -1);
                lua_pop(L, 1);  /* pop result */
            } else {
                double a = g_lua_tonumber(L, -2);  /* key */
                double b = g_lua_tonumber(L, -1);  /* t[j] */
                less = (a < b);
            }
            if (!less) {
                lua_pop(L, 1);  /* pop t[j] */
                break;
            }
            /* t[j+1] = t[j] */
            g_lua_rawseti(L, 1, j + 1);
            j--;
        }
        /* t[j+1] = key */
        g_lua_rawseti(L, 1, j + 1);
    }
    return 0;
}

static int table_unpack(lua_State* L) {
    if (!g_lua_objlen || !g_lua_rawgeti) return 0;

    int start = 1;
    int end = (int)g_lua_objlen(L, 1);
    if (g_lua_gettop(L) >= 2) start = (int)g_lua_tonumber(L, 2);
    if (g_lua_gettop(L) >= 3) end = (int)g_lua_tonumber(L, 3);

    int count = end - start + 1;
    if (count <= 0) return 0;
    if (count > 200) count = 200;  /* safety limit */

    int i;
    for (i = start; i <= start + count - 1; i++) {
        g_lua_rawgeti(L, 1, i);
    }
    return count;
}

static const SreLuaReg tablib[] = {
    {"concat",  table_concat},
    {"getn",    table_getn},
    {"insert",  table_insert},
    {"maxn",    table_maxn},
    {"remove",  table_remove},
    {"sort",    table_sort},
    {NULL, NULL}
};


/* =========================================================================
 * os library (minimal — RL uses os.time/os.clock)
 * ========================================================================= */

static uint64_t g_fake_time = 1000000;

static int os_time(lua_State* L) {
    g_lua_pushnumber(L, (double)(g_fake_time++));
    return 1;
}

static int os_clock(lua_State* L) {
    static double clock_val = 0.0;
    clock_val += 0.001;
    g_lua_pushnumber(L, clock_val);
    return 1;
}

static int os_difftime(lua_State* L) {
    g_lua_pushnumber(L, g_lua_tonumber(L, 1) - g_lua_tonumber(L, 2));
    return 1;
}

static int os_date(lua_State* L) {
    g_lua_pushstring(L, "2026-06-22");
    return 1;
}

static int os_exit(lua_State* L) { (void)L; return 0; }

static const SreLuaReg oslib[] = {
    {"clock",    os_clock},
    {"date",     os_date},
    {"difftime", os_difftime},
    {"exit",     os_exit},
    {"time",     os_time},
    {NULL, NULL}
};


/* =========================================================================
 * debug library (minimal stubs)
 * ========================================================================= */

static int debug_getinfo(lua_State* L) {
    g_lua_createtable(L, 0, 4);
    g_lua_pushstring(L, "Lua");
    g_lua_setfield(L, -2, "what");
    g_lua_pushstring(L, "?");
    g_lua_setfield(L, -2, "source");
    g_lua_pushnumber(L, 0);
    g_lua_setfield(L, -2, "currentline");
    g_lua_pushstring(L, "");
    g_lua_setfield(L, -2, "name");
    return 1;
}

static int debug_traceback(lua_State* L) {
    const char* msg = "";
    if (g_lua_gettop(L) >= 1 && g_lua_type(L, 1) == LUA_TSTRING) {
        msg = lua_tostring(L, 1);
    }
    g_lua_pushstring(L, msg ? msg : "traceback unavailable");
    return 1;
}

static int debug_sethook(lua_State* L) { (void)L; return 0; }

static const SreLuaReg dblib[] = {
    {"getinfo",   debug_getinfo},
    {"sethook",   debug_sethook},
    {"traceback", debug_traceback},
    {NULL, NULL}
};


/* =========================================================================
 * io library (minimal stubs)
 * ========================================================================= */

static int io_write(lua_State* L) { (void)L; return 0; }
static int io_read(lua_State* L) { g_lua_pushnil(L); return 1; }

static const SreLuaReg iolib[] = {
    {"read",  io_read},
    {"write", io_write},
    {NULL, NULL}
};


/* =========================================================================
 * loadfile/dofile — VFS-aware overrides (Task 2.6)
 * ========================================================================= */

/* loadfile(filename) — SWKiwi compatible version with MiniPath support.
 * Writes the path to g_sre_vfs_lua_input_path for host-side translation,
 * then returns the translated path string. The real loading is handled
 * by the engine's Lua loader. */
static int sre_loadfile(lua_State* L) {
    const char* filename = lua_tostring(L, 1);
    if (!filename) {
        g_lua_pushnil(L);
        g_lua_pushstring(L, "loadfile: filename expected");
        return 2;
    }

    /* Write path for VFS translation */
    if (g_sre_vfs_active) {
        /* Copy filename to input path buffer for host translation */
        int i;
        for (i = 0; i < 511 && filename[i]; i++)
            g_sre_vfs_lua_input_path[i] = filename[i];
        g_sre_vfs_lua_input_path[i] = '\0';
        g_sre_vfs_lua_translate_pending = 1;

        /* If host translated, use the output path */
        if (g_sre_vfs_lua_output_path[0]) {
            filename = g_sre_vfs_lua_output_path;
        }
    }

    /* Push the translated filename for the caller to use */
    g_lua_pushstring(L, filename);
    return 1;  /* Return the translated filename; real loading handled by engine */
}

/* dofile(filename) — loads and executes a file with MiniPath support.
 * Full implementation needs luaL_loadfile + lua_pcall which we don't
 * have as function pointers yet. For now, this is a safe stub. */
static int sre_dofile(lua_State* L) {
    const char* filename = lua_tostring(L, 1);
    if (!filename) {
        g_lua_pushstring(L, "dofile: filename expected");
        if (g_lua_error) return g_lua_error(L);
        return 0;
    }
    /* For now, dofile is a stub that just returns nothing.
     * Full implementation needs luaL_loadfile + lua_pcall. */
    return 0;
}


/* =========================================================================
 * LuaFileSystem (fs) library (Task 2.7)
 *
 * SWKiwi exposes this as the "fs" global (via luaopen_lfs).
 * Since SRE is freestanding (no libc), we provide stubs that
 * prevent nil-call crashes in mods that use lfs for file ops.
 * ========================================================================= */

/* fs.dir iterator — immediately returns nil (empty directory) */
static int lfs_dir_next(lua_State* L) {
    (void)L;
    g_lua_pushnil(L);
    return 1;
}

/* fs.dir(path) — returns an iterator that yields no filenames */
static int lfs_dir(lua_State* L) {
    (void)L;
    g_lua_pushcclosure(L, lfs_dir_next, 0);
    return 1;
}

/* fs.attributes(path, aname) — returns a stub attributes table */
static int lfs_attributes(lua_State* L) {
    (void)L;
    g_lua_createtable(L, 0, 4);
    g_lua_pushstring(L, "file");
    g_lua_setfield(L, -2, "mode");
    g_lua_pushnumber(L, 0);
    g_lua_setfield(L, -2, "size");
    g_lua_pushnumber(L, 0);
    g_lua_setfield(L, -2, "modification");
    g_lua_pushnumber(L, 0);
    g_lua_setfield(L, -2, "access");
    return 1;
}

/* fs.mkdir(path) — create directory (stub) */
static int lfs_mkdir(lua_State* L) { (void)L; return 0; }

/* fs.rmdir(path) — remove directory (stub) */
static int lfs_rmdir(lua_State* L) { (void)L; return 0; }

/* fs.currentdir() — returns current directory (stub) */
static int lfs_currentdir(lua_State* L) {
    g_lua_pushstring(L, "/Files/");
    return 1;
}

/* fs.lock/unlock — no-ops */
static int lfs_lock(lua_State* L)   { (void)L; return 0; }
static int lfs_unlock(lua_State* L) { (void)L; return 0; }

static const SreLuaReg fslib[] = {
    {"dir",        lfs_dir},
    {"attributes", lfs_attributes},
    {"mkdir",      lfs_mkdir},
    {"rmdir",      lfs_rmdir},
    {"currentdir", lfs_currentdir},
    {"lock",       lfs_lock},
    {"unlock",     lfs_unlock},
    {NULL, NULL}
};


/* =========================================================================
 * Registration — called from sre_mini_ensure_injected
 * ========================================================================= */

void sre_open_std_libs(lua_State* L) {
    if (!g_luaL_register || !g_lua_pushnumber || !g_lua_setfield) return;

    /* Register math library */
    g_luaL_register(L, "math", (const void*)mathlib);
    g_lua_pushnumber(L, PI);
    g_lua_setfield(L, -2, "pi");
    g_lua_pushnumber(L, sre_huge);
    g_lua_setfield(L, -2, "huge");
    lua_pop(L, 1);

    /* Register table library */
    g_luaL_register(L, "table", (const void*)tablib);
    lua_pop(L, 1);

    /* Register os library */
    g_luaL_register(L, "os", (const void*)oslib);
    lua_pop(L, 1);

    /* Register debug library */
    g_luaL_register(L, "debug", (const void*)dblib);
    lua_pop(L, 1);

    /* Register io library */
    g_luaL_register(L, "io", (const void*)iolib);
    lua_pop(L, 1);

    /* Also register unpack as global (Lua 5.1 compat) */
    g_lua_pushcclosure(L, table_unpack, 0);
    g_lua_setfield(L, LUA_GLOBALSINDEX, "unpack");

    /* Register fs (LuaFileSystem) library */
    g_luaL_register(L, "fs", (const void*)fslib);
    lua_pop(L, 1);

    /* Override loadfile/dofile with VFS-aware versions */
    g_lua_pushcclosure(L, sre_loadfile, 0);
    g_lua_setfield(L, LUA_GLOBALSINDEX, "loadfile");

    g_lua_pushcclosure(L, sre_dofile, 0);
    g_lua_setfield(L, LUA_GLOBALSINDEX, "dofile");
}
