# Swordigo Desktop — Modding Guide

> **Version:** 1.0  
> **Target:** Swordigo Desktop (SRT) v1.4.12 ARM64  
> **Last updated:** 2026-06-23

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Mod Structure](#2-mod-structure)
3. [Music Mods](#3-music-mods)
4. [Scene Mods](#4-scene-mods)
5. [Background Mods](#5-background-mods)
6. [Text Mods (Compile-time)](#6-text-mods-compile-time)
7. [GUI Mods (Compile-time)](#7-gui-mods-compile-time)
8. [Mod Config Memory Protocol](#8-mod-config-memory-protocol)
9. [Advanced: Lua Scripting](#9-advanced-lua-scripting)
10. [Installing Mods](#10-installing-mods)
11. [Mod Examples](#11-mod-examples)

---

## 1. Introduction

Swordigo Desktop ships with a layered modding system that lets you customize the
game without touching the original engine binary. There are two categories of
mods:

| Category | Changed at… | Requires rebuild? | Hot-reload? |
|---|---|---|---|
| **Runtime mods** | Game launch | No | Yes (launcher detects) |
| **Compile-time mods** | SRE build | Yes (`make`) | No |

### What Can Be Modded

| Aspect | Mod type | Method |
|---|---|---|
| **Music** | Runtime | Replace any music track by playlist name |
| **Scenes** | Runtime | Swap `.scene` files |
| **Backgrounds** | Runtime | Replace background textures |
| **Textures / Assets** | Runtime | Replace files in `assets/resources/` |
| **Text / Strings** | Compile-time | SRE string replacement table |
| **GUI Elements** | Compile-time | Hide buttons, override label text |
| **Gameplay Logic** | Compile-time + Lua | Lua scripting via `Mini.*` / `LNI.*` APIs |

### How It Works — High Level

```
┌──────────────┐       ┌──────────────────┐       ┌───────────────┐
│  Launcher    │──────▶│ Mod Config Block │──────▶│  libsre.so    │
│  (host C++)  │ write │ Guest RAM 0x49000│ read  │  (guest ARM64)│
└──────────────┘       └──────────────────┘       └───────────────┘
       │                                                │
       │ reads mod.json                                 │ hooks engine
       │ from disk mods                                 │ functions at
       ▼                                                │ runtime
  ~/.local/share/                                       ▼
  swordigo-desktop/                              sre_mod_get_*()
  mods/                                          replacements
```

1. The **launcher** scans `~/.local/share/swordigo-desktop/mods/` for mod
   folders containing `mod.json`.
2. Enabled mods are serialized into an **8 KB shared-memory block** at guest
   address `0x49000`.
3. On boot, **libsre.so** calls `sre_init_mods()` which parses the block and
   populates replacement tables.
4. When the engine requests a music track, scene, or background, SRE's hooks
   consult the tables and transparently substitute the modded asset.

---

## 2. Mod Structure

Every mod lives in its own directory under:

```
~/.local/share/swordigo-desktop/mods/<mod-name>/
```

### Directory Layout

```
mods/
└── my-music-mod/
    ├── mod.json                    # Required — mod manifest
    └── assets/
        └── music/
            ├── custom_plains.mp3   # Replacement track files
            └── epic_boss.ogg
```

### `mod.json` Format

```json
{
  "id":          "com.example.my-music-mod",
  "name":        "Epic Soundtrack",
  "version":     "1.0.0",
  "author":      "Your Name",
  "description": "Replaces overworld and boss music with custom tracks.",
  "type":        "music",
  "replace": {
    "plains":    "custom_plains",
    "boss":      "epic_boss"
  }
}
```

#### Field Reference

| Field | Type | Required | Description |
|---|---|---|---|
| `id` | string | **Yes** | Unique identifier (reverse-domain recommended). Must be unique across all installed mods. |
| `name` | string | **Yes** | Human-readable display name shown in the launcher's Mods panel. |
| `version` | string | No | Semantic version string (e.g. `"1.2.0"`). Informational only. |
| `author` | string | No | Author name or handle. |
| `description` | string | No | Short description of what the mod does. |
| `type` | string | No | One of `"music"`, `"scene"`, `"asset"`, `"background"`. Determines how the launcher processes the `replace` map. |
| `replace` | object | No | Key-value pairs mapping **original** asset names to **replacement** asset basenames. See sections below for type-specific details. |

### Enable / Disable Convention

Mods use a **dot-prefix** convention for toggling:

| State | Directory name | Example |
|---|---|---|
| **Enabled** | `my-music-mod/` | Launcher processes `mod.json` |
| **Disabled** | `.my-music-mod/` | Launcher ignores the folder |

The launcher's **Mods panel** toggles this automatically via a checkbox.
You can also rename the folder manually on disk.

---

## 3. Music Mods

Music mods replace the game's background music tracks. Swordigo uses named
playlists internally — each playlist name maps to a `.mp3` file under
`res/raw/`.

### Supported Formats

| Format | Extension | Notes |
|---|---|---|
| MP3 | `.mp3` | Recommended — matches original game format |
| Ogg Vorbis | `.ogg` | Fully supported by OpenAL |
| WAV | `.wav` | Supported but large file sizes |

### File Placement

Place replacement audio files in your mod's `assets/music/` directory:

```
mods/my-music-mod/
├── mod.json
└── assets/
    └── music/
        ├── custom_plains.mp3
        └── epic_boss.ogg
```

### The `replace` Object

Each key is the **original playlist name** (see table below). Each value is the
**basename** (without extension) of your replacement file:

```json
{
  "replace": {
    "plains":     "custom_plains",
    "boss":       "epic_boss",
    "wastelands": "my_wastelands"
  }
}
```

When the engine calls `MusicPlayer::PlayMusicWithName("plains")`, SRE intercepts
the call, looks up `"plains"` in the replacement table, and substitutes
`"custom_plains"`. The host then loads `custom_plains.mp3` (or `.ogg`/`.wav`)
from the mod's `assets/music/` directory.

### Limits

- **Maximum 32 music replacement entries** per mod config block.
- Track name strings are limited to **31 characters** (32 bytes including NUL).

### Complete Playlist Name Reference

| Category | Playlist Name | Used In |
|---|---|---|
| **Menu** | `menu` | Main menu |
| | `menu2` | Secondary menu / level select |
| **Overworld** | `plains` | Overworld plains areas |
| | `forest` | Forest areas |
| | `graveyard` | Graveyard / undead areas |
| | `caves` | Underground / cave areas |
| | `fortress` | Fortress / castle areas |
| | `wastelands` | Wastelands / late-game areas |
| **Boss** | `boss` | Standard boss encounters |
| | `boss2` | Secondary boss theme |
| | `final_boss` | Final boss fight |
| **Special** | `intro` | Intro / opening sequence |
| | `ending` | Ending / credits roll |
| | `shop` | In-game shop / merchant |

### How Music Replacement Works Internally

The replacement chain in the SRE source (`sre_music.c` → `sre_mod.c`):

```c
// In sre_PlayMusicWithName() — intercepts engine's MusicPlayer call
extern const char* sre_mod_get_music_replacement(const char*);
const char* mod_repl = sre_mod_get_music_replacement(new_name);
if (mod_repl) {
    // Use modded track name instead of original
    sre_strcpy(new_name, mod_repl, 256);
}
```

The lookup scans the global `g_music_replacements[]` table populated at boot
by `sre_init_mods()`.

---

## 4. Scene Mods

Scene mods replace `.scene` files that define the game's levels, enemy
placements, and world geometry.

### Structure

```
mods/my-scene-mod/
├── mod.json
└── scenes/
    ├── custom_plains.scene
    └── custom_fortress.scene
```

### `mod.json`

```json
{
  "id":      "com.example.scene-overhaul",
  "name":    "Plains Overhaul",
  "type":    "scene",
  "replace": {
    "plains1":    "custom_plains",
    "fortress3":  "custom_fortress"
  }
}
```

### Details

- **Maximum 16 scene replacement entries** per mod config block.
- Scene name strings are limited to **63 characters** (64 bytes including NUL).
- Scene names match the **base filename** without the `.scene` extension.
- The `replace` key is the original scene name, the value is the replacement
  scene name (which maps to your file in `scenes/`).

### Internal API

```c
// sre_mod.c
const char* sre_mod_get_scene_replacement(const char* original_name);
// Returns replacement name or NULL if no mod override
```

---

## 5. Background Mods

Background mods replace the parallax background textures rendered behind the
game world.

### Structure

```
mods/my-bg-mod/
├── mod.json
└── backgrounds/
    ├── custom_sky.png
    └── custom_mountain.png
```

### `mod.json`

```json
{
  "id":      "com.example.dark-backgrounds",
  "name":    "Dark Backgrounds",
  "type":    "background",
  "replace": {
    "sky_default":  "custom_sky",
    "mountains":    "custom_mountain"
  }
}
```

### Details

- **Maximum 16 background replacement entries** per mod config block.
- Background name strings are limited to **63 characters** (64 bytes including
  NUL).

### Internal API

```c
// sre_mod.c
const char* sre_mod_get_bg_replacement(const char* original_name);
// Returns replacement name or NULL if no mod override
```

---

## 6. Text Mods (Compile-time)

Every string the game creates passes through `sre_CppString_from_char_p()` in
`sre_string.c`. A **replacement table** lets you change ANY text in the game —
menu buttons, dialog, item names, UI labels — simply by adding entries.

### Source File

```
src/sre/sre_string.c
```

### The Replacement Table

```c
static const char* g_sre_string_replacements[][2] = {
    /* ---- Main Menu Buttons ---- */
    /* { "Start",          "Singleplayer" }, */
    /* { "Achievements",   "Online"       }, */
    /* { "Offers",         "Settings"     }, */
    /* { "Credits",        "About"        }, */
    /* { "Privacy Policy", "SRE v1.0"     }, */

    { 0, 0 }  /* sentinel — do NOT remove */
};
```

### Adding Entries

Uncomment or add `{ "original", "replacement" }` pairs before the sentinel:

```c
static const char* g_sre_string_replacements[][2] = {
    { "Start",          "Play"          },
    { "Privacy Policy", "SRE v1.0"     },
    { "Offers",         "Settings"     },

    { 0, 0 }  /* sentinel — do NOT remove */
};
```

### How It Works

The replacement happens inside `sre_CppString_from_char_p`:

```c
void sre_CppString_from_char_p(SreString* self, const char* src) {
    // ...
    /* Apply string replacement table */
    for (i = 0; g_sre_string_replacements[i][0] != 0; i++) {
        if (sre_strcmp(src, g_sre_string_replacements[i][0]) == 0) {
            src = g_sre_string_replacements[i][1];
            break;
        }
    }
    // ... creates string with replaced text
}
```

> [!IMPORTANT]
> This replaces **ALL** occurrences of the original text game-wide. If multiple
> UI elements share the same label text, they will all change.

### Rebuild Required

After editing the table, rebuild SRE:

```bash
make          # rebuilds libsre.so with the new string table
```

---

## 7. GUI Mods (Compile-time)

SRE's native GUI renderer (`sre_gui_native.c`) provides two mechanisms
for modifying the game's UI at the rendering level.

### 7.1 Hiding Buttons

Add button titles to the `g_hidden_buttons[]` array in `sre_gui_native.c`:

```c
static const char* g_hidden_buttons[] = {
    "Privacy Policy",    /* useless on desktop */
    "Achievements",      /* hide achievements button */
    0  /* sentinel — do not remove */
};
```

Buttons with matching titles are **permanently hidden** — they don't render
and don't occupy visual space. The match is exact and case-sensitive.

Additionally, SRE automatically hides icon-only buttons (Twitter, Facebook,
music/SFX toggles) that are descendants of the `MainMenuView`. This keeps
in-game controls like back buttons and D-pad intact.

### 7.2 Overriding Label Text

Add entries to the `g_text_overrides[]` table:

```c
static const SreTextOverride g_text_overrides[] = {
    /* ======== MAIN MENU ======== */
    { "Offers",          "Options"      },  /* repurpose Offers button */
    
    /* ======== ADD YOUR OVERRIDES BELOW ======== */
    { "Credits",         "About"        },
    { "Achievements",    "Online"       },

    { 0, 0 }  /* sentinel — do not remove */
};
```

### How Label Overrides Work

The override is applied **once** during the first render of each `GUILabel`:

```c
// In native_GUILabel_DrawRect:
const char* text = sre_read_label_text(self);
const char* override = sre_find_override(text);
if (override) {
    sre_apply_text_override(self, override);
    // Permanently changes the label's internal std::string
    // Rebuilds the FontText so the new text renders correctly
}
```

After the first frame, the label's internal string is permanently changed —
**zero per-frame overhead**.

### Text Overrides vs. String Replacements

| Feature | `g_sre_string_replacements` | `g_text_overrides` |
|---|---|---|
| **Source file** | `sre_string.c` | `sre_gui_native.c` |
| **Scope** | ALL strings (menus, dialogs, items) | GUI labels only |
| **When applied** | String construction time | First render frame |
| **Overhead** | Per-string-creation check | One-time per label |
| **Best for** | Dialog text, item names | Button/label text |

---

## 8. Mod Config Memory Protocol

The launcher communicates mod data to the guest engine via a **shared memory
block** written to guest address `0x49000` before boot. This block is exactly
**8 KB** and follows a strict binary layout.

### Overview

```
Guest Address: 0x49000  (SRE_MOD_CONFIG_ADDR)
Total Size:    8192 bytes (8 KB)
Magic:         0x4D4F4453 ("MODS" in little-endian ASCII)
```

### Header Layout (64 bytes at offset 0x000)

| Offset | Size | Field | Description |
|---|---|---|---|
| `0x000` | 4 | `magic` | `0x4D4F4453` — validation magic |
| `0x004` | 4 | `version` | `1` — protocol version |
| `0x008` | 4 | `music_count` | Number of music replacement entries |
| `0x00C` | 4 | `scene_count` | Number of scene replacement entries |
| `0x010` | 4 | `bg_count` | Number of background replacement entries |
| `0x014` | 4 | `flags` | Reserved (0) |
| `0x018` | 4 | `total_mods` | Total number of active mods |
| `0x01C` | 36 | `_reserved[9]` | Reserved for future use |

### Music Entries (at offset 0x040)

Each entry is **64 bytes** (32 + 32):

| Offset in entry | Size | Field | Example |
|---|---|---|---|
| `+0x00` | 32 | `original` | `"wastelands\0"` |
| `+0x20` | 32 | `replacement` | `"my_wastelands\0"` |

Maximum **32 entries** × 64 bytes = 2048 bytes (0x800).

### Scene Entries (at offset 0x840)

Each entry is **128 bytes** (64 + 64):

| Offset in entry | Size | Field | Example |
|---|---|---|---|
| `+0x00` | 64 | `original` | `"plains1\0"` |
| `+0x40` | 64 | `replacement` | `"custom_plains\0"` |

Maximum **16 entries** × 128 bytes = 2048 bytes (0x800).

### Background Entries (at offset 0x1840)

Each entry is **128 bytes** (64 + 64):

| Offset in entry | Size | Field | Example |
|---|---|---|---|
| `+0x00` | 64 | `original` | `"sky_default\0"` |
| `+0x40` | 64 | `replacement` | `"custom_sky\0"` |

Maximum **16 entries** × 128 bytes = 2048 bytes (0x800).

### Memory Map Summary

```
0x49000 ┌──────────────────────────────┐
        │  Header (64 bytes)           │  magic, version, counts
0x49040 ├──────────────────────────────┤
        │  Music entries               │  32 × 64 bytes = 2048
        │  (original[32]+repl[32])     │
0x49840 ├──────────────────────────────┤
        │  Scene entries               │  16 × 128 bytes = 2048
        │  (original[64]+repl[64])     │
0x4A040 ├──────────────────────────────┤
        │  Background entries          │  16 × 128 bytes = 2048
        │  (original[64]+repl[64])     │
0x4A840 └──────────────────────────────┘
```

### Initialization

`sre_init_mods()` is called by the host after all other SRE init functions.
It validates the magic and version, then copies entries into static tables:

```c
void sre_init_mods(uint64_t config_addr) {
    // Reset all counts
    g_music_repl_count = 0;
    g_scene_repl_count = 0;
    g_bg_repl_count = 0;

    if (config_addr == 0) { /* no mods */ return; }

    SreModConfigHeader* hdr = (SreModConfigHeader*)config_addr;
    if (hdr->magic != 0x4D4F4453 || hdr->version != 1) {
        return;  // Invalid — proceed without mods
    }

    // Parse music entries at offset 0x40...
    // Parse scene entries at offset 0x840...
    // Parse background entries at offset 0x1840...

    g_sre_mod_initialized = 1;
}
```

---

## 9. Advanced: Lua Scripting

Swordigo uses **Lua 5.1** for gameplay scripting. SRE injects custom API tables
into every `lua_State` the engine creates, giving mods and scripts access to
extended functionality.

### API Injection

SRE uses lazy injection — on every `lua_call`, it checks whether the current
`lua_State` has been initialized. If not, it registers all APIs:

```
sre_lua_call_safe(L, nargs, nresults)
  └─▶ sre_mini_ensure_injected(L)
        ├─▶ sre_open_std_libs(L)       // math, table, os, debug, io
        └─▶ sre_register_mini_api(L)   // Mini.*, LNI.*, Components.*
```

### 9.1 Mini.* API

The `Mini` global table provides mod-oriented game functions. Originally from
the **SwMini** modloader for Android, reimplemented natively in SRE.

| Function | Signature | Description |
|---|---|---|
| `Mini.Arch()` | `→ string` | Returns architecture: `"arm64-v8a"` |
| `Mini.GetProfileID()` | `→ string` | Returns current save profile UUID |
| `Mini.SetControlsHidden(bool)` | `→ void` | Show/hide on-screen controls |
| `Mini.SetCoinLimit(n)` | `→ void` | Set max coin count (1–65535) |
| `Mini.ToggleDebug()` | `→ void` | Toggle debug mode |
| `Mini.RecreateHero()` | `→ void` | Respawn the hero (placeholder) |
| `Mini.SceneFindAll()` | `→ table` | List scene objects (placeholder) |
| `Mini.ExecuteLNI(name, ...)` | `→ void` | Bridge to host LNI system |
| `Mini.BindLNI(name)` | `→ function` | Returns a closure that calls ExecuteLNI |
| `Mini.map(collection, fn)` | `→ table` | Functional map over table/string/number |

#### Mini.map Overloads

```lua
-- Map over a table (array)
Mini.map({1, 2, 3}, function(v) return v * 2 end)  -- {2, 4, 6}

-- Map over a number range
Mini.map(5, function(i) return i * i end)           -- {1, 4, 9, 16, 25}

-- Map over characters in a string
Mini.map("hello", function(c) return c:upper() end) -- {"H","E","L","L","O"}
```

### 9.2 LNI.* API

The `LNI` (Lua Native Interface) table provides direct host actions. All
functions have **case-insensitive aliases** (e.g., `LNI.setSpeed` and
`LNI.SetSpeed` both work).

| Function | Signature | Description |
|---|---|---|
| `LNI.getSpeed()` / `LNI.GetSpeed()` | `→ number` | Get current game speed multiplier |
| `LNI.setSpeed(n)` / `LNI.SetSpeed(n)` | `→ void` | Set game speed (0.0 – 10.0) |
| `LNI.quit()` / `LNI.Quit()` | `→ void` | Signal host to exit |
| `LNI.copyToClipboard(text)` | `→ void` | Copy text to system clipboard |
| `LNI.openUrl(url)` / `LNI.OpenURL(url)` | `→ void` | Open URL in system browser |

### 9.3 Components.* Table (Stub)

Provides stub sub-tables for compatibility with RL scripts:

- `Components.Health`
- `Components.Physics`
- `Components.Entity`

### 9.4 Standard Libraries

SRE implements the following Lua 5.1 standard libraries (the vanilla engine only
ships `base` + `string`):

| Library | Functions |
|---|---|
| **math** | `abs`, `acos`, `asin`, `atan`, `atan2`, `ceil`, `cos`, `deg`, `exp`, `floor`, `fmod`, `log`, `log10`, `max`, `min`, `pow`, `rad`, `random`, `randomseed`, `sin`, `sqrt`, `tan` + `math.pi`, `math.huge` |
| **table** | `concat`, `getn`, `insert`, `maxn`, `remove`, `sort` |
| **os** | `clock`, `date`, `difftime`, `exit`, `time` |
| **debug** | `getinfo`, `sethook`, `traceback` |
| **io** | `read`, `write` (stubs) |
| **global** | `unpack` (Lua 5.1 compat) |

### 9.5 Lua Console

SRE includes a built-in Lua console. The host writes Lua code to a shared
buffer, and SRE executes it inside the engine's `lua_State`:

```
Host writes → g_lua_console_buf[4096]     // Lua source code
              g_lua_console_pending = 1   // trigger flag

SRE reads  → sre_run_console(L)          // pcall(loadstring(code))

SRE writes → g_lua_console_result[4096]  // output / error message
              g_lua_console_status        // 0=idle, 1=success, 2=error
```

This lets you test Lua commands interactively without rebuilding.

---

## 10. Installing Mods

### Step-by-Step

1. **Copy** your mod folder to the mods directory:
   ```bash
   cp -r my-music-mod/ ~/.local/share/swordigo-desktop/mods/
   ```

2. **Launch the game** — the launcher automatically scans the mods directory.

3. **Open the Mods panel** in the launcher sidebar. Your mod will appear with
   its name, author, and description from `mod.json`.

4. **Toggle** the checkbox to enable or disable the mod.

5. **Click Play** — the launcher serializes all enabled mods into the config
   block and boots the game.

### Hot Detection

The launcher watches the `mods/` directory for changes. You can:

- Add new mod folders while the launcher is open
- Remove mod folders
- Rename folders to toggle the dot-prefix enable/disable

The launcher's mod list updates automatically — **no restart needed** for the
launcher itself. However, mod changes only take effect on the **next game
launch** (the config block is written at boot time).

### Manual Toggle

You can enable/disable mods from the command line:

```bash
# Disable a mod (add dot prefix)
mv ~/.local/share/swordigo-desktop/mods/my-mod \
   ~/.local/share/swordigo-desktop/mods/.my-mod

# Enable a mod (remove dot prefix)
mv ~/.local/share/swordigo-desktop/mods/.my-mod \
   ~/.local/share/swordigo-desktop/mods/my-mod
```

---

## 11. Mod Examples

### 11.1 Example: Music Replacement Mod

Replace the plains and boss music with custom tracks.

#### Directory Structure

```
~/.local/share/swordigo-desktop/mods/
└── epic-ost/
    ├── mod.json
    └── assets/
        └── music/
            ├── adventure_plains.mp3
            └── intense_battle.ogg
```

#### `mod.json`

```json
{
  "id":          "com.modder.epic-ost",
  "name":        "Epic OST Replacement",
  "version":     "1.0.0",
  "author":      "SwordigoModder",
  "description": "Replaces the plains overworld and boss battle music with epic orchestral tracks.",
  "type":        "music",
  "replace": {
    "plains": "adventure_plains",
    "boss":   "intense_battle"
  }
}
```

#### What Happens

1. Launcher reads `mod.json`, sees `type: "music"` with 2 replace entries.
2. At boot, writes to guest memory at `0x49040`:
   - Entry 0: `original = "plains\0"`, `replacement = "adventure_plains\0"`
   - Entry 1: `original = "boss\0"`, `replacement = "intense_battle\0"`
3. `sre_init_mods()` populates `g_music_replacements[0..1]`.
4. When the engine enters a plains area and calls
   `PlayMusicWithName("plains")`, SRE looks up `"plains"` →
   `"adventure_plains"`, and the host loads `adventure_plains.mp3` from the
   mod's `assets/music/` directory.

---

### 11.2 Example: Scene Replacement Mod

Replace the first plains level with a custom version.

#### Directory Structure

```
~/.local/share/swordigo-desktop/mods/
└── custom-plains/
    ├── mod.json
    └── scenes/
        └── my_plains1.scene
```

#### `mod.json`

```json
{
  "id":          "com.modder.custom-plains",
  "name":        "Custom Plains Level",
  "version":     "0.1.0",
  "author":      "LevelDesigner",
  "description": "Reimagined first plains area with new enemy placements and geometry.",
  "type":        "scene",
  "replace": {
    "plains1": "my_plains1"
  }
}
```

---

### 11.3 Example: Text Override (Compile-time)

Rename main menu buttons without any runtime mod files.

#### Edit `src/sre/sre_string.c`

```c
static const char* g_sre_string_replacements[][2] = {
    /* ---- Main Menu ---- */
    { "Start",          "Singleplayer" },
    { "Achievements",   "Online"       },
    { "Credits",        "About"        },
    { "Privacy Policy", "SRE v1.0"     },

    /* ---- In-Game ---- */
    { "You Died",       "Game Over"    },
    { "Tap to Resume",  "Press Enter"  },

    { 0, 0 }  /* sentinel — do NOT remove */
};
```

#### Edit `src/sre/sre_gui_native.c`

```c
/* Override label text (applied once per label, zero per-frame cost) */
static const SreTextOverride g_text_overrides[] = {
    { "Offers",  "Options" },
    { 0, 0 }
};

/* Hide buttons completely */
static const char* g_hidden_buttons[] = {
    "Privacy Policy",
    0
};
```

#### Rebuild

```bash
make    # rebuilds libsre.so — deploy to engine directory
```

---

## Appendix: Source File Reference

| File | Purpose |
|---|---|
| `src/sre/sre_mod.c` | Mod config parser + replacement table lookups |
| `src/sre/sre_music.c` | Native MusicPlayer — handles music replacement |
| `src/sre/sre_string.c` | String constructor — text replacement table |
| `src/sre/sre_gui_native.c` | Native GUI renderer — button hiding, text overrides |
| `src/sre/sre_mini_api.c` | Mini.* and LNI.* Lua API implementation |
| `src/sre/sre_lua.c` | Lua hook layer — pcall protection, console |
| `src/sre/sre_lua_libs.c` | Standard Lua library implementations |
| `src/sre/sre.h` | Core type definitions, hook table |
| `src/game/mod_tools.h` | Host-side mod tools (speed, pause, toast) |
| `src/game/mod_tools.cpp` | Host-side mod tools implementation |
