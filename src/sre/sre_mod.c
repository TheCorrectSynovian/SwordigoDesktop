/* =====================================================================
 * sre_mod.c — SRE Mod System
 * =====================================================================
 * Guest-side mod configuration reader. The host (launcher) writes mod
 * config data to a shared memory block at SRE_MOD_CONFIG_ADDR before
 * game boot. SRE reads this during sre_init_mods() and stores
 * replacement tables that other hooks (music, VFS, scene) consult.
 *
 * Memory layout of mod config block (at 0x49000):
 *   [0x000] uint32_t magic          = 0x4D4F4453 ("MODS")
 *   [0x004] uint32_t version        = 1
 *   [0x008] uint32_t music_count    = number of music replacements
 *   [0x00C] uint32_t scene_count    = number of scene replacements
 *   [0x010] uint32_t bg_count       = number of background replacements
 *   [0x014] uint32_t flags          = reserved
 *   [0x018] uint32_t total_mods     = number of active mods
 *   [0x01C] uint32_t _reserved[9]
 *
 *   [0x040] Music replacement entries (64 bytes each):
 *           [+0x00] char original[32]   e.g. "wastelands"
 *           [+0x20] char replacement[32] e.g. "my_wastelands"
 *
 *   [0x840] Scene replacement entries (128 bytes each):
 *           [+0x00] char original[64]   e.g. "plains1"
 *           [+0x40] char replacement[64] e.g. "custom_plains"
 *
 *   [0x1040] Background replacement entries (128 bytes each):
 *            [+0x00] char original[64]
 *            [+0x40] char replacement[64]
 *
 * Max entries: 32 music, 16 scene, 16 background = well within 8KB.
 * =====================================================================
 */

#include "sre.h"

/* ========== Mod Config Constants ========== */
#define SRE_MOD_MAGIC       0x4D4F4453  /* "MODS" */
#define SRE_MOD_VERSION     1
#define SRE_MAX_MUSIC_REPL  32
#define SRE_MAX_SCENE_REPL  16
#define SRE_MAX_BG_REPL     16
#define SRE_MOD_NAME_LEN    32
#define SRE_MOD_PATH_LEN    64

/* ========== Replacement Entry Structs ========== */

typedef struct {
    char original[SRE_MOD_NAME_LEN];
    char replacement[SRE_MOD_NAME_LEN];
} SreModMusicEntry;

typedef struct {
    char original[SRE_MOD_PATH_LEN];
    char replacement[SRE_MOD_PATH_LEN];
} SreModSceneEntry;

typedef struct {
    char original[SRE_MOD_PATH_LEN];
    char replacement[SRE_MOD_PATH_LEN];
} SreModBgEntry;

/* ========== Mod Config Header ========== */
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t music_count;
    uint32_t scene_count;
    uint32_t bg_count;
    uint32_t flags;
    uint32_t total_mods;
    uint32_t _reserved[9];
    /* 64 bytes total header */
} SreModConfigHeader;

/* ========== Global Mod State ========== */

/* Music replacement table — consulted by sre_PlayMusicWithName */
static SreModMusicEntry g_music_replacements[SRE_MAX_MUSIC_REPL];
static int g_music_repl_count = 0;

/* Scene replacement table — consulted by scene loader hook (future) */
static SreModSceneEntry g_scene_replacements[SRE_MAX_SCENE_REPL];
static int g_scene_repl_count = 0;

/* Background replacement table — consulted by background draw hook (future) */
static SreModBgEntry g_bg_replacements[SRE_MAX_BG_REPL];
static int g_bg_repl_count = 0;

/* Overall mod state — host-visible for diagnostics */
volatile int g_sre_mod_count = 0;        /* Number of active mods */
volatile int g_sre_mod_music_count = 0;  /* Number of music replacements */
volatile int g_sre_mod_scene_count = 0;  /* Number of scene replacements */
volatile int g_sre_mod_initialized = 0;  /* 1 after sre_init_mods completes */

/* ========== String helpers (no libc) ========== */

static int mod_strcmp(const char* a, const char* b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return *a - *b;
}

static void mod_strcpy(char* dst, const char* src, int max) {
    int i;
    for (i = 0; i < max - 1 && src[i]; i++) {
        dst[i] = src[i];
    }
    dst[i] = 0;
}

/* ========== Public API: Query Music Replacement ========== */

/*
 * sre_mod_get_music_replacement — check if a music track has a mod override
 *
 * @param original_name  The track name the engine wants to play (e.g. "wastelands")
 * @return               Pointer to replacement name, or NULL if no replacement
 *
 * Called by sre_PlayMusicWithName() before loading a track.
 */
const char* sre_mod_get_music_replacement(const char* original_name) {
    int i;
    for (i = 0; i < g_music_repl_count; i++) {
        if (mod_strcmp(original_name, g_music_replacements[i].original) == 0) {
            return g_music_replacements[i].replacement;
        }
    }
    return NULL;
}

/* ========== Public API: Query Scene Replacement ========== */

/*
 * sre_mod_get_scene_replacement — check if a scene has a mod override
 *
 * @param original_name  Scene name (e.g. "plains1")
 * @return               Replacement name, or NULL if no replacement
 */
const char* sre_mod_get_scene_replacement(const char* original_name) {
    int i;
    for (i = 0; i < g_scene_repl_count; i++) {
        if (mod_strcmp(original_name, g_scene_replacements[i].original) == 0) {
            return g_scene_replacements[i].replacement;
        }
    }
    return NULL;
}

/* ========== Public API: Query Background Replacement ========== */

const char* sre_mod_get_bg_replacement(const char* original_name) {
    int i;
    for (i = 0; i < g_bg_repl_count; i++) {
        if (mod_strcmp(original_name, g_bg_replacements[i].original) == 0) {
            return g_bg_replacements[i].replacement;
        }
    }
    return NULL;
}

/* ========== Initialization ========== */

/*
 * sre_init_mods — read mod config from shared memory
 *
 * @param config_addr  Guest virtual address of the mod config block
 *                     (written by host before game boot)
 *
 * Called by host (main.cpp) after sre_init() and other init calls.
 * If config_addr is 0 or the magic doesn't match, mods are disabled.
 */
void sre_init_mods(uint64_t config_addr) {
    g_music_repl_count = 0;
    g_scene_repl_count = 0;
    g_bg_repl_count = 0;
    g_sre_mod_count = 0;
    g_sre_mod_music_count = 0;
    g_sre_mod_scene_count = 0;

    if (config_addr == 0) {
        g_sre_mod_initialized = 1;
        return;
    }

    /* Read header */
    SreModConfigHeader* hdr = (SreModConfigHeader*)config_addr;
    if (hdr->magic != SRE_MOD_MAGIC || hdr->version != SRE_MOD_VERSION) {
        /* Invalid or no mod config — proceed without mods */
        g_sre_mod_initialized = 1;
        return;
    }

    g_sre_mod_count = hdr->total_mods;

    /* Read music replacements (at offset 0x40) */
    int mc = hdr->music_count;
    if (mc > SRE_MAX_MUSIC_REPL) mc = SRE_MAX_MUSIC_REPL;
    SreModMusicEntry* music_entries = (SreModMusicEntry*)(config_addr + 0x40);
    int i;
    for (i = 0; i < mc; i++) {
        mod_strcpy(g_music_replacements[i].original,
                   music_entries[i].original, SRE_MOD_NAME_LEN);
        mod_strcpy(g_music_replacements[i].replacement,
                   music_entries[i].replacement, SRE_MOD_NAME_LEN);
    }
    g_music_repl_count = mc;
    g_sre_mod_music_count = mc;

    /* Read scene replacements (at offset 0x840) */
    int sc = hdr->scene_count;
    if (sc > SRE_MAX_SCENE_REPL) sc = SRE_MAX_SCENE_REPL;
    SreModSceneEntry* scene_entries = (SreModSceneEntry*)(config_addr + 0x840);
    for (i = 0; i < sc; i++) {
        mod_strcpy(g_scene_replacements[i].original,
                   scene_entries[i].original, SRE_MOD_PATH_LEN);
        mod_strcpy(g_scene_replacements[i].replacement,
                   scene_entries[i].replacement, SRE_MOD_PATH_LEN);
    }
    g_scene_repl_count = sc;
    g_sre_mod_scene_count = sc;

    /* Read background replacements (at offset 0x1040) */
    int bc = hdr->bg_count;
    if (bc > SRE_MAX_BG_REPL) bc = SRE_MAX_BG_REPL;
    SreModBgEntry* bg_entries = (SreModBgEntry*)(config_addr + 0x1040);
    for (i = 0; i < bc; i++) {
        mod_strcpy(g_bg_replacements[i].original,
                   bg_entries[i].original, SRE_MOD_PATH_LEN);
        mod_strcpy(g_bg_replacements[i].replacement,
                   bg_entries[i].replacement, SRE_MOD_PATH_LEN);
    }
    g_bg_repl_count = bc;

    g_sre_mod_initialized = 1;
}
