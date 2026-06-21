/*
 * sre_init.c — libsre.so initialization and hook table
 *
 * This file is compiled as part of libsre.so (ARM64).
 * It provides:
 *   1. sre_init() — called by host after loading to set up globals
 *   2. sre_init_lua() — sets up Lua API function pointers
 *   3. sre_hook_table — array of {offset, symbol} pairs read by host
 *      to write trampolines in libswordigo.so
 */

#include "sre.h"
#include "sre_lua.h"

/* =========================================================================
 * Globals
 * ========================================================================= */

/* Set by sre_init() — base address of libswordigo.so in guest memory.
 * Non-static: accessed by sre_gui.c for computing function pointers. */
uint64_t g_swordigo_base = 0;

/* External: empty sentinel pointer (defined in sre_string.c) */
extern char* g_empty_sentinel;

/* =========================================================================
 * Hook Table
 * =========================================================================
 * The host reads this table after loading libsre.so.
 * For each entry, it writes a trampoline at swordigo_base + target_offset
 * that redirects to the symbol in libsre.so.
 *
 * Offsets are for v1.4.12 ARM64 (from SwMini modloader research).
 * Set target_offset = 0 to mark end of table.
 */

const SreHookEntry sre_hook_table[] = {
    /* CppString — eliminate atomic refcounting */
    { 0x566bb8, "sre_CppString_from_char_p" },  /* std::string(const char*) */
    { 0x56918c, "sre_CppString_assign"      },  /* std::string::assign()    */
    { 0x567254, "sre_CppString_append"      },  /* std::string::append()    */
    { 0x565220, "sre_CppString_release"     },  /* std::string destructor   */

    /* ProgramState — catch Lua errors instead of aborting */
    { 0, "sre_ProgramState_Execute" },  /* offset resolved dynamically by symbol */
    { 0, "sre_ProgramState_Resume"  },  /* offset resolved dynamically by symbol */
    /* DISABLED — sre_ProgramState_Update skips calling the original, breaking
     * ALL entity AI/behavior logic. g_orig_ProgramState_Update is never set,
     * and the trampoline destroys the original function bytes.
     * Re-enable only after implementing proper trampoline chaining. */
    /* { 0, "sre_ProgramState_Update"  }, */

    /* Background rendering — our own sky renderer
     * Addresses verified via: aarch64-linux-gnu-nm -D libswordigo.so | grep BackgroundComponent
     * Note: Draw is const (ZNK mangling) */
    { 0x21ded4, "sre_BackgroundComponent_Draw"             },  /* BackgroundComponent::Draw (const) */
    { 0x2b6760, "sre_RotatingBackgroundComponent_Draw"     },  /* RotatingBackgroundComponent::Draw (const) */
    { 0x2b66f8, "sre_RotatingBackgroundComponent_Update"   },  /* RotatingBackgroundComponent::Update */

    /* Visual effects — DATA EXTRACTION ONLY (hooks disabled).
     * Vanilla rendering now works after STXR patches — let the game draw.
     * Our SRE implementations stay in source for future use:
     *   - Portal proximity sound effects
     *   - Glow-based lighting in FBO composite
     *   - Weapon trail particle enhancements
     * Re-enable by uncommenting when we add passthrough trampolines. */
    /* { 0x2ae884, "sre_PortalEffectComponent_Draw"    }, */  /* Portal swirl */
    /* { 0x2b0e90, "sre_SimpleGlowComponent_Draw"      }, */  /* Glow FX */
    /* { 0x2cb828, "sre_WeaponGlowComponent_Draw"      }, */  /* Weapon glow */

    /* Lua error safety — wraps ALL lua_call with pcall
     * Installed as LATE trampoline in main.cpp (after sre_init_lua) */
    /* { 0, "sre_lua_call_safe" }, */

    /* C++ exception handling — hook __cxa_throw to prevent broken unwind.
     * When a C++ exception is thrown (e.g. Lua error), the unwind machinery
     * fails because Unicorn can't capture guest registers. Instead of
     * letting it abort(), we longjmp to the nearest recovery point
     * (set by sre_lua_call_safe). */
    { 0x51e108, "sre_cxa_throw" },

    /* MusicPlayer — FULL native replacement.
     * The original uses boost::shared_ptr + C++ exceptions for playlist
     * management, all of which break under Unicorn. Our SRE version
     * writes commands to shared globals, host executes via OpenAL. */
    { 0x4811a0, "sre_PlayMusicWithName"                },  /* PlayMusicWithName(string&, bool) */
    { 0x4814a8, "sre_MusicPlayer_FadeIn"               },  /* FadeIn(float) */
    { 0x4815d8, "sre_MusicPlayer_FadeOut"               },  /* FadeOut(float) */
    { 0x482090, "sre_MusicPlayer_Update"                },  /* Update(float) */
    /* NOTE: SetVolume(0x482064) and SetLooping(0x48206c) are only 8 bytes apart.
     * Our 16-byte trampoline would clobber one from the other. Leave originals —
     * they call MusicPlayerJNI via JNI bridge (safe). SRE Update handles fading. */
    { 0x481e88, "sre_MusicPlayer_SetEnabled"            },  /* SetEnabled(bool) */
    { 0x481fc0, "sre_MusicPlayer_SetSuspended"          },  /* SetSuspended(bool) */
    /* NOTE: We do NOT hook AddPlaylist (0x48093c) or RegisterProgramLibrary (0x4821c8).
     * RegisterProgramLibrary registers Lua bindings ("MusicPlayer:PlayMusicWithName" etc.)
     * that scripts call. Those bindings invoke the C++ methods which we've already hooked.
     * AddPlaylist populates the playlist list; harmless since we bypass playlists. */

    /* GUI System — game state extraction.
     * Hooks GameSceneView::Update to read HP/mana/coins from GameState
     * every frame. The native HUD won't animate (we own the display). */
    { 0x34ed2c, "sre_GameSceneView_Update" },  /* GameSceneView::Update(float) */

    /* Death/Respawn System — skip ads, directly respawn.
     * When player taps "Continue" on game over screen, ShowAdMaybe is called.
     * Our hook skips all ad logic and calls GameOverViewDidContinue directly,
     * which reloads from the last checkpoint save. */
    { 0x347efc, "sre_ShowAdMaybe" },  /* GameOverViewController::ShowAdMaybe() */

    /* Sentinel — end of table */
    { 0, 0 }
};

/* Number of entries (excluding sentinel) */
const int sre_hook_count = (sizeof(sre_hook_table) / sizeof(sre_hook_table[0])) - 1;

/* =========================================================================
 * Initialization
 * ========================================================================= */

/*
 * sre_init — Called by host after loading both .so files
 *
 * @param swordigo_base  Guest virtual address where libswordigo.so is loaded
 * @param empty_bss_off  BSS offset of the empty string sentinel (0x14880 for v1.4.12)
 *
 * The host passes these values from main.cpp after loading both libraries.
 */
void sre_init(uint64_t swordigo_base, uint64_t empty_bss_off) {
    g_swordigo_base = swordigo_base;

    /* Calculate the guest address of the empty string sentinel.
     * The empty sentinel is in libswordigo.so's BSS at the given offset.
     * Its _Rep has refcount = -1 (static, never free).
     * The data pointer is: base + bss_offset + sizeof(SreStringRep) 
     * because the sentinel stores [_Rep][data] and we want the data part.
     *
     * Actually, the BSS offset might already point to the data portion.
     * SwMini accesses it as: engine_load_bias + BOFF_CPP_STRING_EMPTY_SENTINEL
     * Let's store the base + offset and check at runtime.
     */
    g_empty_sentinel = (char*)(swordigo_base + empty_bss_off);
}
