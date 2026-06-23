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

SreHookEntry sre_hook_table[] = {
    /* CppString — eliminate atomic refcounting */
    { 0x566bb8, "sre_CppString_from_char_p" },  /* std::string(const char*) */
    { 0x56918c, "sre_CppString_assign"      },  /* std::string::assign()    */
    { 0x567254, "sre_CppString_append"      },  /* std::string::append()    */
    { 0x565220, "sre_CppString_release"     },  /* std::string destructor   */

    /* ProgramState — catch Lua errors instead of aborting */
    { 0, "sre_ProgramState_Execute" },  /* offset resolved dynamically by symbol */
    { 0, "sre_ProgramState_Resume"  },  /* offset resolved dynamically by symbol */
    /* ProgramState::Update — handles timer-based coroutine resume with
     * error recovery. On ARM64 the resume is inlined (unlike ARM32 where
     * Update calls Resume separately). After handling the resume safely,
     * sre_ProgramState_Update calls g_orig_ProgramState_Update (relay stub)
     * for the child ProgramState iteration loop.
     * The relay stub is set up by the host after trampoline installation.
     *
     * DISABLED FOR TESTING — let the native engine handle it to reduce
     * per-frame overhead. Lua resume errors will crash without this. */
    /* { 0, "sre_ProgramState_Update"  }, */

    /* luaD_throw — ROOT of all Lua error handling. Every Lua error goes
     * through luaD_throw(L, errcode). Original calls __cxa_throw or
     * ProgramPanic+exit — both crash in Unicorn. Our replacement uses
     * setjmp/longjmp recovery instead of C++ exceptions. */
    { 0x4eb814, "sre_luaD_throw" },

    /* ProgramPanic — safety net (should never fire now that luaD_throw
     * is hooked, but kept as backup). */
    { 0x5c0ab4, "sre_ProgramPanic" },

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
    /* ========= FULL GUI STACK HOOKS =========
     * Every GUI class's DrawRect goes through our code.
     * All use relay stubs to call original, so the game
     * works normally while we have total control. */

    /* Root window — top of the render tree */
    { 0x4a28bc, "sre_GUIWindow_DrawRect"    },

    /* Core view classes */
    { 0x49f310, "sre_GUIView_DrawRect"      },  /* base class: iterates subviews */
    { 0x49565c, "sre_GUIButton_DrawRect"    },  /* buttons (title + bg) */
    { 0x497aa0, "sre_GUILabel_DrawRect"     },  /* text labels */
    { 0x497658, "sre_GUIFrameView_DrawRect" },  /* styled frames */

    /* Interactive controls */
    { 0x491b54, "sre_GUIAlertView_DrawRect" },  /* modal dialogs */
    { 0x49cd40, "sre_GUISlider_DrawRect"    },  /* sliders */

    /* Game-specific views */
    { 0x42bae4, "sre_NewMenuView_DrawRect"  },  /* main menu */

    /* Options button — intercept Offers click at the delegate level.
     * When Offers is clicked, ButtonPressed calls the delegate method
     * MainMenuViewDidOpenShop. We hook THAT instead of ButtonPressed
     * to avoid PC-relative relay issues. */
    { 0x36f394, "sre_MainMenuVC_DidOpenShop" },

    /* CreditsVC hooks — DISABLED for v6, Options menu WIP.
     * { 0x38d604, "sre_CreditsVC_LoadView" },
     * { 0x38d904, "sre_CreditsVC_ButtonPressed" }, */

    /* Death/Respawn fix — desktop has no ad SDK, so ShowAdMaybe hangs.
     * Hook it to directly call GameOverViewDidContinue (respawn). */
    { 0x347efc, "sre_GameOverVC_ShowAdMaybe" },

    /* Text Input — fully intercept the text input chain.
     *
     * Layer 1: Hook StartTextInputWithDelegate / StopTextInputWithDelegate
     *          to capture the delegate pointer in our OWN global and clear
     *          DAT_007f3ca8. This prevents the draw cycle from dispatching
     *          through the corrupt ITextInputDelegate vtable.
     *
     * Layer 2: Hook the JNI textInput functions to read/write
     *          GUITextFieldImpl fields directly (no vtable calls).
     *
     * Layer 3: Host maps a safety RET-page at 0x2d6ce4c to catch any
     *          remaining wild vtable jumps from the PRIMARY vtable. */
    { 0x4792ac, "sre_StartTextInputWithDelegate" },  /* Caver::StartTextInputWithDelegate */
    { 0x4793dc, "sre_StopTextInputWithDelegate"  },  /* Caver::StopTextInputWithDelegate  */
    { 0x4790dc, "sre_textInputTextDidChange" },       /* Java_..._textInputTextDidChange   */
    { 0x479290, "sre_textInputDidFinish"     },       /* Java_..._textInputDidFinish       */

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
     * they call MusicPlayerJNI via JNI bridge (safe). SRE Update handles fading.
     *
     * However, AudioSystem::SetMusicVolume (0x47f5f0) is the HIGH-LEVEL setter
     * called by the engine UI. We hook THIS to route slider changes to OpenAL. */
    { 0x47f5f0, "sre_AudioSystem_SetMusicVolume" },
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

    /* NOTE: Death/Respawn hook at 0x347efc is already defined above
     * (sre_GameOverVC_ShowAdMaybe). Do NOT duplicate — the old
     * sre_ShowAdMaybe name was from the TVPG snapshot and doesn't
     * exist in the current codebase. */

    /* Mod Support — inject Mini/LNI/Components Lua tables.
     * DISABLED: relay stubs crash because original function uses PC-relative
     * instructions (ADRP) that break when relocated to 0x3000000.
     * Mini.* injection is done via sre_lua_call_safe piggyback instead. */
    /* { 0x4c0f18, "sre_RegisterProgramLibrary", 0 }, */

    /* Virtual Filesystem — mod asset layering.
     * DISABLED: Same trampoline issue — replaces FileExistsAtPath entirely.
     * Our stub returns 1 optimistically, breaking actual file checks.
     * Re-enable after implementing host-side file check delegation. */
    /* { 0x4b44b8, "sre_FileExistsAtPath" }, */

    /* Sentinel — end of table */
    { 0, 0, 0 }
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

    /* Fix ITextInputDelegate vtable — entries have unrelocated pointers.
     *
     * The vtable at binary offset 0x7e1688 contains function pointers to
     * the TextInputTextDidChange and TextInputDidFinish thunks. These
     * were never properly relocated by our ELF loader, causing wild jumps
     * (e.g. to 0x2d6ce4c) when the game's drawApplication dispatches
     * through the vtable during text field rendering.
     *
     * Fix: overwrite the vtable entries with pointers to our SRE handlers.
     * This is safe because Unicorn maps all guest memory as RWX.
     *
     * Vtable layout (ITextInputDelegate):
     *   [+0x00] TextInputTextDidChange(out_str, this, text_str)
     *   [+0x08] TextInputDidFinish(this)
     */
    extern void sre_TextInputTextDidChange_vtable(void*, void*, void*);
    extern void sre_TextInputDidFinish_vtable(void*);
    uint64_t* itid_vtable = (uint64_t*)(swordigo_base + 0x7e1688);
    itid_vtable[0] = (uint64_t)&sre_TextInputTextDidChange_vtable;
    itid_vtable[1] = (uint64_t)&sre_TextInputDidFinish_vtable;
}
