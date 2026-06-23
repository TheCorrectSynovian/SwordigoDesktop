# Building Swordigo Desktop

> Build guide for the Swordigo Runtime (SRT) project — a Linux desktop port of
> Swordigo that runs the original ARM64 game binary inside a Unicorn Engine
> emulator with native SDL3 rendering.

---

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Source Directory Structure](#source-directory-structure)
3. [Build Targets](#build-targets)
4. [Executable Reference](#executable-reference)
5. [CLI Arguments](#cli-arguments)
6. [Runtime Data Layout](#runtime-data-layout)
7. [Build Script — `run_swordigo.sh`](#build-script--run_swordigosh)
8. [Important Notes](#important-notes)

---

## Prerequisites

### Required

| Dependency | Purpose |
|---|---|
| **Linux x86_64** (Fedora 39+ / Ubuntu 22.04+) | Host platform |
| **GCC / G++** with C++17 support | Compiles `swordigo_boot` and `asset_viewer` |
| **aarch64-linux-gnu-gcc** | Cross-compiles `libsre.so` for ARM64 guest |
| **SDL3** (dev + runtime) | Windowing, input, event loop |
| **SDL3_image** (dev + runtime) | PNG/PVR texture loading |
| **OpenGL** headers + drivers (`libGL`) | Primary graphics backend |
| **Unicorn Engine** (`libunicorn`) | ARM64 CPU emulation |
| **OpenAL Soft** (`libopenal`) | 3D positional audio |
| **libvorbisfile** | OGG Vorbis audio decoding |
| **zlib** | Compressed asset extraction |
| **pthreads** | I/O thread, emulator threading |

### Optional

| Dependency | Purpose |
|---|---|
| **Vulkan SDK** + headers | Vulkan graphics backend (compile-time `VULKAN_BACKEND` define) |
| **VMA** (Vulkan Memory Allocator) | Vulkan memory management |
| **volk** | Vulkan function loader |

### Bundled (no install needed)

| Component | Location |
|---|---|
| **Dear ImGui** (core + SDL3/OpenGL3 backends) | `src/imgui/` |

### Fedora Quick Install

```bash
sudo dnf install gcc gcc-c++ gcc-aarch64-linux-gnu \
    SDL3-devel SDL3_image-devel mesa-libGL-devel \
    unicorn-devel openal-soft-devel \
    libvorbis-devel zlib-devel pkg-config
```

### Ubuntu / Debian Quick Install

```bash
sudo apt install build-essential gcc-aarch64-linux-gnu \
    libsdl3-dev libsdl3-image-dev libgl-dev \
    libunicorn-dev libopenal-dev \
    libvorbis-dev zlib1g-dev pkg-config
```

> [!NOTE]
> SDL3 may not be in your distro's default repos yet. If needed, build from
> source: https://github.com/libsdl-org/SDL

---

## Source Directory Structure

```
~/SwordigoDesktop/
├── Makefile                    # GNU Make build system (no CMake)
├── run_swordigo.sh             # One-shot build + install + run script
├── swordigo_boot               # Output: main executable
├── libsre.so                   # Output: ARM64 guest library
├── asset_viewer                # Output: standalone asset browser
├── build/                      # Intermediate object files (.o, .d)
├── engine/                     # Engine binaries + manifest
├── src/
│   ├── main.cpp                # Main entry point (~4700 lines)
│   ├── jni/                    # JNI bridge: marshalling Android JNI calls
│   │   ├── jni_bridge.cpp
│   │   ├── jni_bridge_arm64.cpp
│   │   └── jni_marshaller.cpp
│   ├── platform/               # Desktop platform layer
│   │   ├── display.cpp         # SDL3 window + OpenGL/Vulkan context
│   │   ├── emulator.cpp        # Unicorn Engine setup
│   │   ├── emulator_arm64.cpp  # ARM64-specific emulation
│   │   ├── gui.cpp             # ImGui overlay rendering
│   │   ├── launcher_ui.cpp     # Launcher/binary selector GUI
│   │   ├── save_editor.cpp     # In-game save editor UI
│   │   ├── srt_overlay.cpp     # SRT debug overlay
│   │   ├── fbo_scaler.cpp      # Framebuffer scaling
│   │   ├── pvr_loader.cpp      # PVR texture format loader
│   │   ├── io_thread.cpp       # Async I/O thread
│   │   ├── data_path.cpp       # XDG data directory resolution
│   │   ├── binary_selector.cpp # Multi-version engine selector
│   │   └── input_config.cpp    # Input mapping configuration
│   ├── game/                   # Game-specific logic
│   │   ├── camera_override.cpp # Camera system modifications
│   │   ├── mod_tools.cpp       # Modding infrastructure
│   │   └── save_editor_logic.cpp
│   ├── loader/                 # ELF loader for ARM binaries
│   │   ├── elf_loader.cpp
│   │   └── elf_loader_arm64.cpp
│   ├── audio/                  # Audio subsystem
│   ├── render/                 # Rendering subsystem
│   ├── sre/                    # SRE guest library sources (C11 + ASM)
│   │   ├── sre.h               # SRE public API header
│   │   ├── sre_init.c          # Initialization + hook registration
│   │   ├── sre_string.c        # String utility replacements
│   │   ├── sre_lua.c           # Lua scripting bridge
│   │   ├── sre_lua_libs.c      # Extended Lua libraries
│   │   ├── sre_background.c    # Background rendering hooks
│   │   ├── sre_effects.c       # Visual effects overrides
│   │   ├── sre_music.c         # Music playback hooks
│   │   ├── sre_gui.c           # GUI system hooks
│   │   ├── sre_gui_native.c    # Native GUI overlay rendering
│   │   ├── sre_scene_update.c  # Scene update logic replacements
│   │   ├── sre_mini_api.c      # Minimal runtime API
│   │   ├── sre_vfs.c           # Virtual filesystem hooks
│   │   ├── sre_mod.c           # Mod loading support
│   │   ├── sre_setjmp.S        # ARM64 setjmp/longjmp (assembly)
│   │   ├── sre_setjmp.h
│   │   ├── sre_lua.h
│   │   └── sre_gui.h
│   ├── tools/                  # Standalone tools
│   │   ├── asset_viewer.cpp    # Asset browser/previewer entry point
│   │   ├── pod_loader.cpp      # PowerVR POD model loader
│   │   ├── av_renderer.cpp     # Asset viewer renderer
│   │   ├── av_audio.cpp        # Asset viewer audio preview
│   │   └── scene_loader.cpp    # Scene file parser
│   ├── android/                # Android compatibility shims
│   │   ├── asset_manager.c     # AAssetManager stub
│   │   └── log.c               # __android_log_print stub
│   ├── imgui/                  # Dear ImGui (bundled)
│   │   ├── imgui.cpp, imgui_draw.cpp, imgui_tables.cpp, imgui_widgets.cpp
│   │   └── backends/
│   │       ├── imgui_impl_sdl3.cpp
│   │       └── imgui_impl_opengl3.cpp
│   ├── assets/                 # Embedded asset resources
│   └── prototype/              # Experimental/prototype code
└── modapi/                     # Modding API documentation (you are here)
```

---

## Build Targets

All builds use **GNU Make**. The project does not use CMake, Meson, or any other
build system.

```bash
# Build all default targets (swordigo_boot + libsre.so)
make -j$(nproc)

# Build individual targets
make swordigo_boot      # Main desktop runtime executable
make libsre.so          # ARM64 guest-side SRE library (cross-compiled)
make asset_viewer       # Standalone asset browser tool

# Install libsre.so to the engine directory
make install-sre        # Copies to ~/.local/share/swordigo-desktop/engine/v1.4.12/arm64-v8a/

# Clean all build artifacts
make clean              # Removes objects, deps, and all output binaries
```

The default `all` target builds `swordigo_boot` and `libsre.so`. The
`asset_viewer` target must be built separately.

---

## Executable Reference

### `swordigo_boot` — Main Runtime

The primary executable. Loads the ARM64 `libswordigo.so` into a Unicorn Engine
emulator, bridges JNI calls to native SDL3/OpenGL, and runs the game.

| Property | Value |
|---|---|
| **Language** | C++17 |
| **Compiler** | `g++` |
| **Optimization** | `-g -O1` (debug + light optimization) |
| **Key dependencies** | SDL3, SDL3_image, OpenGL, Unicorn Engine, OpenAL Soft, libvorbisfile, zlib, pthreads |
| **Link flags** | `-lSDL3 -lSDL3_image -lGL -lunicorn -lopenal -lvorbisfile -lz -lpthread -lm` |
| **ImGui** | Bundled (compiled in-tree) |

### `libsre.so` — Swordigo Runtime Engine

An ARM64 shared library loaded into the emulated address space alongside
`libswordigo.so`. It hooks and replaces problematic game functions with clean
reimplementations.

| Property | Value |
|---|---|
| **Language** | C11 + ARM64 assembly |
| **Compiler** | `aarch64-linux-gnu-gcc` (cross-compiler) |
| **Flags** | `-shared -fPIC -O2 -nostdlib -fno-builtin` |
| **Dependencies** | **None** — fully freestanding, no libc |
| **Source count** | 14 C files + 1 assembly file |

> [!IMPORTANT]
> `libsre.so` is compiled with `-nostdlib`. It cannot call any standard library
> functions. All utilities (string ops, memory ops) are self-contained in
> `sre_string.c` and `sre_mini_api.c`.

### `asset_viewer` — Asset Browser

A standalone graphical tool for browsing and previewing game assets (textures,
models, scenes, audio) without running the full game.

| Property | Value |
|---|---|
| **Language** | C++17 |
| **Compiler** | `g++` |
| **Optimization** | `-g -O2` |
| **Key dependencies** | SDL3, SDL3_image, OpenGL |
| **ImGui** | Bundled (compiled in-tree) |
| **Notable** | No Unicorn, no OpenAL, no game code — lightweight |

---

## CLI Arguments

### `swordigo_boot`

```
swordigo_boot [options]
```

| Argument | Description |
|---|---|
| `--headless` | Run without creating a window (testing/CI) |
| `--vulkan` | Use the Vulkan rendering backend (requires `VULKAN_BACKEND` compile flag) |
| `--lib <path>` | Load a custom ARM64 `.so` binary instead of the default |
| `--test-lib` | Use the bundled v1.4.12 ARM32 binary (testing) |
| `--assets <dir>` | Override the game assets directory |
| `--no-launcher` | Skip the launcher GUI, launch directly with defaults |
| `--generate-manifest [engine_dir] [output]` | Generate `manifest.json` from an engine directory and exit |

When `--vulkan`, `--no-launcher`, or `--lib` is specified, the launcher GUI is
automatically skipped.

### `run_swordigo.sh`

```
./run_swordigo.sh [options]
```

| Argument | Description |
|---|---|
| `--clean` | Perform a full clean rebuild before launching |
| `--sre-only` | Only rebuild `libsre.so` (skip `swordigo_boot`) |
| `--no-build` | Skip the build step entirely, just install SRE and run |
| `--help` | Show usage information |

The script automatically installs `libsre.so` to **all** ARM64 engine
directories found under `~/.local/share/swordigo-desktop/engine/`.

---

## Runtime Data Layout

All game data resides in the user's home directory following XDG conventions:

```
~/.local/share/swordigo-desktop/
├── assets/resources/       # Game textures, models, scenes, Lua scripts
├── engine/                 # ARM binaries per version
│   ├── manifest.json       # Engine version registry
│   └── v1.4.12/
│       └── arm64-v8a/
│           ├── libswordigo.so   # Original game binary
│           └── libsre.so        # SRE hooks library
├── res/raw/                # Music files (.mp3)
└── save/                   # User save files (NEVER packaged)
```

> [!TIP]
> This is a **Minecraft-style** data layout. Users have full read/write access
> to all game files, making modding and backup straightforward.

On packaged installs (RPM/DEB), a post-install script copies from
`/usr/share/swordigo-desktop/` to the user directory on first run.

---

## Important Notes

> [!CAUTION]
> **Never build on an NTFS partition.** The project must be compiled on a native
> Linux filesystem (ext4). NTFS on Linux causes file locking errors, case
> sensitivity collisions, and degraded I/O performance that will break builds.
> See `AGENTS.md` for the full rationale.

- **No CMake.** The project uses a hand-written GNU `Makefile` exclusively.
- **Source edits** happen only in `~/SwordigoDesktop/` (ext4). The copy on the
  TVPG drive (`/run/media/quantumcreeper/TVPG/...`) is for git commits only —
  sync is always ext4 → TVPG, never the reverse.
- **Parallel builds** are fully supported. Use `make -j$(nproc)` for fastest
  compilation.
- **Incremental builds** work via auto-generated `.d` dependency files. Only
  changed sources are recompiled.
- **Dear ImGui** is vendored in `src/imgui/` and compiled as part of both
  `swordigo_boot` and `asset_viewer`. Do not install it system-wide.
- **`pkg-config`** is used to discover library paths at build time. Ensure all
  `-devel` packages are installed so `pkg-config --cflags/--libs` queries
  succeed.

---

*Last updated: 2026-06-23*
