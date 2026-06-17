# 🎮 Swordigo Desktop v1.0.0 — Full Desktop Experience

> The first full-featured desktop release. Transforms the ARM emulation proof-of-concept into a polished, playable desktop game with native GUI, audio, modding tools, and Linux packaging.

---

## ✨ Highlights

- 🖥️ **Native Desktop GUI** — Full menu bar system with dropdown menus, settings panel, and About dialog
- 🎵 **Music & Audio** — OpenAL-powered WAV music playback with volume control
- ⚡ **Massive Performance Boost** — Targeted Unicorn hooks eliminate per-instruction overhead
- 🎯 **1920×1080 Native Rendering** — Pixel-perfect output, no more blur
- 🎮 **Full Keyboard Controls** — WASD movement, arrow key camera, F-key shortcuts
- 📦 **Linux Packaging** — RPM, DEB, and AppImage with bundled game data

---

## 🆕 New Features

### GUI System
- Complete menu bar: **File**, **Emulation**, **Config**, **Settings**, **Help**
- **Central Settings Panel** — click "Settings" to open a modal with:
  - ✅ Checkboxes: Pause Game, Free Camera, Smooth Camera Mode
  - 🔢 Value controls: GUI Scale (`< 125% >`), Game Speed (`< 1.0x >`)
  - 🔒 Greyed-out future options: God Mode, Infinite Mana, Fly Mode, Walk Speed, Jump Height, FPS Overlay, VSync
- **About Dialog** with project info and acknowledgements
- **Scalable UI** — `Ctrl+Plus`/`Ctrl+Minus` to resize the entire GUI (75%–200%)
- Hover highlights, dropdown menus, keyboard-aware layout

### Audio
- Full **OpenAL** audio pipeline with real-time WAV music playback
- Music auto-loads from `assets/resources/music/` with filename normalization
- Volume control via game engine bridge (MusicPlayer JNI)
- Works in AppImage via `SWORDIGO_DATA_DIR` path resolution

### SwMini Modloader Features
- **Game Speed Control** — `+`/`-` keys or Settings panel (`0.25x` to `4.0x`, `Backspace` to reset)
- **Pause/Resume** — `F8` key, Emulation menu, or Settings panel checkbox
- **Free Camera** — Arrow keys to fly around the scene, `Home` to reset
- **Smooth Camera Mode** — toggle for interpolated camera movement
- Camera controller pointer captured from `CameraController::CameraController()` at runtime

### Input System
- **WASD** for game movement (arrow keys freed for camera)
- **Backspace** for jump
- **F3** — Debug overlay (FPS, resolution, camera state, typing mode)
- **F7** — Toggle typing mode (FWKeyboard API: `SendKeyDownEvent`, `SendKeyUpEvent`, `SendKeyCharEvent`)
- **F8** — Pause/Resume emulation
- **F10** — Toggle on-screen touch controls (`handleMenuButtonPress`)
- **F1** — Toggle GUI overlay visibility
- On-screen controls hidden by default on startup

### FBO Upscaling Pipeline
- Three shader modes: **Sharp-Bilinear** (default), **Nearest-Neighbor**, **CRT Scanline**
- Letterbox/pillarbox with correct aspect ratio
- **Direct pixel-perfect blit** when game resolution matches window (skips shader entirely)

### Window & Display
- Default resolution: **1920×1080** (matches internal render resolution — no quality loss)
- **Resizable window** with automatic FBO scaling
- **Window icon** — `icon_gnome.png` loaded via SDL2_image (shows in taskbar, alt-tab, etc.)
- VSync enabled by default

---

## ⚡ Performance

### Unicorn Hook Optimization (HUGE)
**Before:** A single global `UC_HOOK_CODE` fired a C function on **every ARM instruction** — millions of calls per frame.

**After:** Three targeted ranged hooks:
| Hook | Address Range | Purpose |
|------|--------------|---------|
| Bridge | `0xFF000000–0xFF100000` | JNI bridge calls only |
| Camera | `0x002e35c4–0x002e35c6` | CameraController capture (once) |
| Memory | System | Unmapped memory faults |

Result: **Dramatically reduced CPU overhead** from hook dispatch.

### Rendering
- Skip FBO shader when game res = window res → direct `GL_NEAREST` blit
- No unnecessary texture filtering on 1:1 resolution match

---

## 📦 Packaging

### RPM (Fedora/RHEL)
```bash
SWORDIGO_SRCDIR="$PWD" rpmbuild -bb packaging/swordigo-desktop.spec
sudo dnf install ~/rpmbuild/RPMS/x86_64/swordigo-desktop-*.rpm
```
- Max zstd compression (`w19.zstdio`)
- ARM `libswordigo.so` excluded from dependency scanning
- Full game assets bundled

### DEB (Debian/Ubuntu)
```bash
rm -rf packaging/deb/usr && make install DESTDIR=packaging/deb PREFIX=/usr
dpkg-deb --build packaging/deb swordigo-desktop-1.0.0-amd64.deb
```

### AppImage
```bash
bash builder/appimage/build_appimage.sh
```
- Real game icon (`icon_gnome.png`) as `.DirIcon` and in hicolor
- Music path resolution via `SWORDIGO_DATA_DIR`

### All Packages Include
| Component | Install Path |
|-----------|-------------|
| Binary | `/usr/bin/swordigo-desktop` |
| Game library (ARM) | `/usr/share/swordigo/libswordigo.so` |
| Game assets | `/usr/share/swordigo/assets/resources/` |
| Music | `/usr/share/swordigo/assets/resources/music/` |
| Desktop entry | `/usr/share/applications/swordigo-desktop.desktop` |
| Icon (hicolor) | `/usr/share/icons/hicolor/128x128/apps/swordigo-desktop.png` |
| Icon (pixmaps) | `/usr/share/pixmaps/swordigo-desktop.png` |

---

## 🔧 Technical Changes

### Data Path Resolution
Binary auto-discovers game data with priority:
1. `SWORDIGO_DATA_DIR` env var (AppImage)
2. `./` current directory (development)
3. `/usr/share/swordigo/` (system install)
4. `/usr/local/share/swordigo/` (manual install)

### FWKeyboard API Integration
Resolved native Caver engine symbols:
| Symbol | Address | Purpose |
|--------|---------|---------|
| `FWKeyboard::sharedKeyboard()` | `0x30cc6d` | Get keyboard singleton |
| `FWKeyboard::SendKeyDownEvent()` | `0x30cced` | Send key down |
| `FWKeyboard::SendKeyUpEvent()` | `0x30cd2d` | Send key up |
| `FWKeyboard::SendKeyCharEvent()` | `0x30cd6d` | Send typed character |
| `handleMenuButtonPress` | `0x2f4131` | Toggle on-screen controls |

### Build System
- Added `-lSDL2_image` dependency
- `make install` / `make uninstall` targets
- RPM spec with ARM binary dep exclusion
- DEB control file

---

## 🐛 Bug Fixes
- **Emulation Pause desync** — GUI's internal `paused` state and `g_game_paused` could get out of sync when using F8 vs menu. Fixed: GUI now reads `g_game_paused` directly for label display.
- **AppImage music silence** — Music path was hardcoded relative (`assets/resources/music/...`). Fixed: now resolves via `SWORDIGO_DATA_DIR`.
- **Arrow key conflict** — Arrow keys were bound to both movement and camera. Fixed: WASD for movement, arrows for camera only.
- **Blur at native resolution** — Sharp-bilinear shader was processing a 1:1 copy unnecessarily. Fixed: direct `GL_NEAREST` blit when resolutions match.

---

## 🙏 Acknowledgements
- **Rinnegatamante** — Vita port reference
- **Touchfoo** — Creator of Swordigo
- **SwMini (Swordigo Mini)** — Modloader reference for engine offsets and feature inspiration

---

## 📋 Requirements
- SDL2, SDL2_image, OpenAL, zlib, Unicorn Engine
- OpenGL 2.1+ compatible GPU
- Fedora: `sudo dnf install SDL2-devel SDL2_image-devel unicorn-devel openal-soft-devel zlib-devel`

---

## ⌨️ Controls Quick Reference
| Key | Action |
|-----|--------|
| WASD | Move character |
| Backspace | Jump |
| Arrow Keys | Free camera |
| Home | Reset camera |
| +/- | Game speed |
| Backspace (speed) | Reset speed to 1.0x |
| F1 | Toggle GUI |
| F3 | Debug overlay |
| F7 | Typing mode |
| F8 | Pause/Resume |
| F10 | Toggle on-screen controls |
