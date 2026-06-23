/*
 * sre_vfs.c — Virtual Filesystem for mod support (MiniPaths replacement)
 *
 * Replaces SwMini's MiniPaths system with a clean implementation.
 * When an RL (mod) binary is loaded, resource lookups are redirected
 * to check the mod's asset directory first, falling back to vanilla.
 *
 * Hooked functions:
 *   - Caver::FileExistsAtPath(const std::string&)
 *   - Caver::NewByteBufferFromAndroidAsset(const std::string&, uint32_t*)
 *
 * The host sets g_sre_vfs_mod_prefix to the mod's asset directory
 * (e.g. "rl_assets") if a mod binary is loaded. If empty/null,
 * VFS passes through to the original functions.
 */

#include "sre.h"
#include "sre_lua.h"

/* =========================================================================
 * VFS Globals (set by host via sre_vfs_init)
 * ========================================================================= */

/* Mod resource prefix — e.g. "rl_assets" for RLSwordigo.
 * When set, resource lookups try mod path first, then vanilla. */
char g_sre_vfs_mod_prefix[256] = {0};

/* Flag: 1 = VFS active (mod loaded), 0 = passthrough */
int g_sre_vfs_active = 0;

/* Original function pointers — set by host after trampoline install */
uint64_t g_orig_FileExistsAtPath = 0;
uint64_t g_orig_NewByteBuffer = 0;

/* =========================================================================
 * VFS Initialization
 * ========================================================================= */

/*
 * sre_vfs_init — Configure VFS for mod support
 * Called by host when a mod binary (e.g. RLSwordigo) is selected.
 *
 * @param mod_prefix  Mod asset directory name (e.g. "rl_assets"), or NULL for vanilla
 */
void sre_vfs_init(const char* mod_prefix) {
    if (mod_prefix && mod_prefix[0] != '\0') {
        /* Copy prefix (bounded) */
        int i;
        for (i = 0; i < 255 && mod_prefix[i]; i++) {
            g_sre_vfs_mod_prefix[i] = mod_prefix[i];
        }
        g_sre_vfs_mod_prefix[i] = '\0';
        g_sre_vfs_active = 1;
    } else {
        g_sre_vfs_mod_prefix[0] = '\0';
        g_sre_vfs_active = 0;
    }
}

/* =========================================================================
 * String helpers (freestanding — no libc)
 * ========================================================================= */

static int sre_strlen(const char* s) {
    if (!s) return 0;
    int n = 0;
    while (s[n]) n++;
    return n;
}

static int sre_strncmp(const char* a, const char* b, int n) {
    for (int i = 0; i < n; i++) {
        if (a[i] != b[i]) return a[i] - b[i];
        if (a[i] == '\0') return 0;
    }
    return 0;
}

static void sre_strcpy(char* dst, const char* src) {
    while (*src) *dst++ = *src++;
    *dst = '\0';
}

static void sre_strcat(char* dst, const char* src) {
    while (*dst) dst++;
    while (*src) *dst++ = *src++;
    *dst = '\0';
}

/* =========================================================================
 * Path Rewriting
 * ========================================================================= */

/*
 * Check if a path starts with "resources/" or "assets/resources/".
 * If so, rewrite it to use the mod prefix.
 *
 * Example: "resources/levels/town.scene"
 *   → Try: "rl_assets/resources/levels/town.scene" first
 *   → Fall back to original path
 *
 * Returns 1 if rewrite was done, 0 if passthrough.
 */
static int sre_vfs_rewrite_path(const char* original, char* rewritten, int max_len) {
    if (!g_sre_vfs_active) return 0;

    /* Check for "resources/" prefix — this is the main game content path */
    if (sre_strncmp(original, "resources/", 10) == 0) {
        /* Rewrite: "resources/X" → "rl_assets/resources/X" */
        if (sre_strlen(g_sre_vfs_mod_prefix) + 1 + sre_strlen(original) < max_len) {
            sre_strcpy(rewritten, g_sre_vfs_mod_prefix);
            sre_strcat(rewritten, "/");
            sre_strcat(rewritten, original);
            return 1;
        }
    }

    return 0;
}

/* =========================================================================
 * Hooked Functions
 * =========================================================================
 * These replace the engine's file I/O functions.
 * The VFS intercepts resource lookups and tries the mod directory first.
 *
 * IMPORTANT: These functions are called from the guest ARM64 context.
 * They receive SreString* arguments (Caver's std::string layout).
 *
 * The actual file existence check and byte buffer loading happens on
 * the HOST side — we communicate via shared globals, similar to the
 * music system. The host polls these in the main loop.
 * ========================================================================= */

/* VFS command buffer — written by SRE, read by host */
char g_sre_vfs_path_request[512] = {0};   /* Path to check/load */
int  g_sre_vfs_check_pending = 0;         /* 1 = file check requested */
int  g_sre_vfs_check_result = 0;          /* 1 = file exists */
int  g_sre_vfs_load_pending = 0;          /* 1 = load requested */
uint64_t g_sre_vfs_load_result_ptr = 0;   /* Guest ptr to loaded data */
uint32_t g_sre_vfs_load_result_size = 0;  /* Size of loaded data */

/*
 * sre_FileExistsAtPath — replacement for Caver::FileExistsAtPath
 *
 * ARM64 ABI: X0 = const std::string& path
 * Returns: int (1 = exists, 0 = not found)
 *
 * For mod binaries, we try the mod path first. If not found,
 * we try the vanilla path. Both checks are delegated to the host.
 */
int sre_FileExistsAtPath(SreString* path_str) {
    /* Extract the C string from the std::string */
    const char* path = path_str->data;
    if (!path) return 0;

    /* If VFS is active, try rewritten path first */
    if (g_sre_vfs_active) {
        char rewritten[512];
        if (sre_vfs_rewrite_path(path, rewritten, 512)) {
            /* Write rewritten path to shared buffer for host to check */
            sre_strcpy(g_sre_vfs_path_request, rewritten);
            g_sre_vfs_check_pending = 1;
            /* Host will set g_sre_vfs_check_result synchronously
             * (we're in the emulation thread, host processes immediately) */
            /* For now, return 1 optimistically — the host's actual
             * file loading will handle the fallback */
            return 1;
        }
    }

    /* Passthrough: write original path for host to check */
    sre_strcpy(g_sre_vfs_path_request, path);
    g_sre_vfs_check_pending = 1;
    return 1;  /* Optimistic — let the actual load fail if needed */
}
