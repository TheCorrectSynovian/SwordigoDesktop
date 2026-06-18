<div align="center">

# ⚔️ Swordigo Desktop

### The Swordigo Runtime Environment (SRE)

*The classic 2012 action-adventure — running natively on Linux through a custom ARM compatibility runtime.*

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://github.com/TheCorrectSynovian/SwordigoDesktop/blob/master/LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Linux%20x86__64-purple.svg)](#)
[![Version](https://img.shields.io/badge/Version-v2.0r-00e5ff.svg)](https://github.com/TheCorrectSynovian/SwordigoDesktop/releases)
[![Engine](https://img.shields.io/badge/Engine-SRE%20v2.0-8b3dff.svg)](#-sre-architecture)

[Website](https://thecorrectsynovian.github.io/SwordigoDesktop_web/) · [Download](https://github.com/TheCorrectSynovian/SwordigoDesktop/releases) · [Research](https://thecorrectsynovian.github.io/SwordigoDesktop/web/research.html) · [Discord](https://discord.gg/HsmDZHffCX)

</div>

---

**Swordigo Desktop** is a native Linux port of the beloved mobile action-adventure platformer by Touch Foo. Rather than running through Android emulation layers, this project uses the **Swordigo Runtime Environment (SRE)** — a surgical ARM ELF loader powered by Unicorn Engine that executes the game's original native code directly, bridging it with host-native OpenGL, OpenAL, and SDL2 for a true desktop experience.

v2.0r introduces a multi-pass rendering pipeline with SSAO, God Rays, Volumetric Light Shafts, and 7 visual presets.

---

## ⚡ SRE Architecture

```
┌─────────────────────────────────────────────────────────┐
│  🎮  Your Linux Desktop                                │
│      SDL2 window · OpenGL context · OpenAL audio        │
├─────────────────────────────────────────────────────────┤
│  🔧  SRE Bridge         │  🎨  SRE PostFX              │
│      JNI compat layer   │      SSAO, God Rays, FX      │
│      200+ functions     │      7 visual presets         │
├─────────────────────────┼───────────────────────────────┤
│  ⚙️  SRE Core           │  📦  SRE Loader              │
│      Unicorn Engine     │      ELF parser + relocator   │
│      ARMv7 execution    │      Symbol resolution        │
├─────────────────────────────────────────────────────────┤
│  📜  libswordigo.so (Original ARM Binary)               │
│      Caver Engine · Lua 5.1 · Protobuf · PowerVR       │
└─────────────────────────────────────────────────────────┘
```

| Component | Technology | Role |
|-----------|-----------|------|
| **SRE Loader** | Custom ELF parser | Surgical loader for `libswordigo.so` with full relocation |
| **SRE Core** | [Unicorn Engine](https://www.unicorn-engine.org/) | High-performance ARMv7 instruction emulation |
| **SRE Bridge** | Custom JNI bridge | 200+ bridged functions (libc, math, OpenGL, file I/O) |
| **SRE PostFX** | OpenGL 2.1 | Multi-pass rendering with SSAO, God Rays, color grading |
| **SRE Launcher** | SDL2 + ImGui | Pre-launch config with binary selection + graphics API |

---

## ✨ Features

### 🎮 Full Gameplay
- Complete game loop — explore, fight, solve puzzles, defeat bosses
- Save system with persistent progress (`~/.local/share/swordigo-desktop/save/`)
- Full audio: music tracks + sound effects through OpenAL

### 🖥️ Desktop-Native Experience
- **1920×1080 internal rendering** with FBO-based scaling (Sharp Bilinear, Nearest, CRT Scanline)
- **Keyboard controls** — fully remappable via the in-game Controls Editor (F2)
- **Gamepad support** — Xbox/PlayStation controllers with analog stick + D-pad
- **Multi-touch support** — 10 independent touch inputs for touchscreen laptops
- **FWKeyboard API** — direct integration with the Caver engine's native keyboard system

### 🎨 SRE PostFX Pipeline (NEW in v2.0r)
- **Post-Processing Presets** (F6) — Cinematic, Retro, Fantasy, Noir, Ethereal, Atmospheric
- **SSAO** — Screen Space Ambient Occlusion with 16-sample hemisphere + bilateral blur
- **God Rays** — 64-sample radial blur from configurable sun position
- **Volumetric Light Shafts** — Depth-masked crepuscular rays
- **Color Effects** — Vignette, Film Grain, Chromatic Aberration, Color Grading, Sharpen
- **Depth Buffer as Texture** — Enables all depth-based shader effects

### 🛠️ Engine Features
| Key | Feature |
|-----|---------|
| **F1** | Toggle GUI menu bar (File/Emulation/Config/Misc/Help) |
| **F2** | Controls Editor (drag to reposition buttons) |
| **F3** | Debug overlay (FPS, draw calls, vertices, textures, binary info, PostFX) |
| **F4** | Cycle scaling modes (Sharp Bilinear → Nearest → CRT Scanline) |
| **F5** | Camera override toggle |
| **F6** | Cycle PostFX presets (Off → Cinematic → Retro → Fantasy → Noir → Ethereal → Atmospheric) |
| **F7** | Typing mode (keyboard → FWKeyboard events for text input) |
| **F10** | Toggle game's native on-screen controls |
| **F12** | Fullscreen toggle |

### 🚀 SRE Launcher (NEW in v2.0r)
- **Unified Launcher GUI** — Pre-launch configuration with binary selection + graphics API picker
- **Multi-Binary Support** — SHA-256 validated, auto-detect v1.4.6 and v1.4.12 game binaries
- **Binary Registry** — JSON-based version tracking with tested/untested status

### 🧪 Advanced
- Custom camera system with 6-axis control + zoom + smooth interpolation
- Async I/O thread for non-blocking save/load
- Speed control (0.25× to 4×), frame stepping, pause
- Comprehensive SRE Bridge with SharedPreferences persistence

---

## 📦 Build Instructions

### Dependencies
```bash
# Fedora / RHEL
sudo dnf install unicorn-devel SDL2-devel openal-soft-devel mesa-libGL-devel zlib-devel

# Ubuntu / Debian
sudo apt install libunicorn-dev libsdl2-dev libopenal-dev libgl-dev zlib1g-dev
```

### Build & Run
```bash
make clean && make
./swordigo_boot          # Main build with SRE Launcher
./swordigo_headless      # No display (testing)
```

---

## 🎯 Controls

### Default Keyboard Layout
| Key | Action | Alt Key |
|-----|--------|---------|
| ← / **A** | Move Left | |
| → / **D** | Move Right | |
| **Space** / **W** | Jump | |
| **J** / **Z** | Attack | |
| **K** / **X** | Magic | |
| **I** | Use Item | |
| **Escape** | Menu / Settings | |
| **P** | Pause | |

### Gamepad
| Button | Action |
|--------|--------|
| D-Pad / Left Stick | Movement |
| A / Cross | Jump |
| X / Square | Attack |
| Y / Triangle | Magic |
| B / Circle | Use Item |
| Start | Menu |
| Back / Select | Pause |

All controls are fully remappable — press **F2** to open the Controls Editor and drag buttons to reposition them. Config is saved to `controls.ini`.

---

## 📦 v2.0r Packages

| Format | Platform | Size |
|--------|----------|------|
| `.deb` | Debian/Ubuntu x86_64 | ~48 MB |
| `.rpm` | Fedora/RHEL x86_64 | ~51 MB |
| Binary | Raw executable | ~1 MB |

Packages available in the `build/v2/` directory.

---

## 👥 Credits

### Core Team

| Role | Name | GitHub |
|------|------|--------|
| **Lead Developer** | TheMegineBraine | [@TheMegineBraine](https://github.com/TheMegineBraine) |
| **Developer** | TheCorrectSynovian | [@TheCorrectSynovian](https://github.com/TheCorrectSynovian) |

### Research & Community

| Contribution | Name | GitHub |
|-------------|------|--------|
| **SwMini Mod Loader** — Reverse engineering | ItsJustSomeDude | [@ItsJustSomeDude](https://github.com/ItsJustSomeDude) |
| **SwMini Mod Loader** — Reverse engineering | Kiziyon | [@Kiziyon](https://github.com/Kiziyon) |
| **Swordigo Vita Port** — Original ARM→desktop porting research & VitaGL bridge | Rinnegatamante | [@Rinnegatamante](https://github.com/Rinnegatamante) |

### Original Game

> **Swordigo** — Copyright © 2012-2025 Ville Mäkynen / Touch Foo  
> All Rights Reserved — [touchfoo.com/swordigo](http://www.touchfoo.com/swordigo)

### Open Source Dependencies

| Project | License | Purpose |
|---------|---------|---------|
| [Unicorn Engine](https://www.unicorn-engine.org/) | GPL-2.0 | ARMv7 CPU emulation (SRE Core) |
| [SDL2](https://www.libsdl.org/) | Zlib | Window management, input, gamepad |
| [OpenAL Soft](https://openal-soft.org/) | LGPL-2.1 | Audio playback |
| [GlossHook](https://github.com/XMDS/GlossHook) | MIT | Function hooking (reference from SwMini) |

### Acknowledgments

- The **SwMini** project by ItsJustSomeDude and Kiziyon for their incredible reverse engineering work on the Caver engine, Lua scripting interface, and virtual filesystem architecture.
- **Rinnegatamante** for the Swordigo PS Vita port which provided crucial insights into the engine's JNI interface, touch event system, and native symbol layout.
- The **Swordigo modding community, SwordiForge** for keeping this 2012 gem alive and well-documented.

---

## ⚖️ Legal Notice

This project **does not include or distribute** any original game assets, binaries, music, or copyrighted content from Swordigo or Touch Foo. Users must provide their own `libswordigo.so` and `assets/` directory extracted from a legally obtained copy of the game.

This is a personal research and preservation project. Swordigo is the property of Ville Mäkynen / Touch Foo.

---

<div align="center">

*Powered by the Swordigo Runtime Environment (SRE)*

Built with ❤️ for game preservation

</div>
