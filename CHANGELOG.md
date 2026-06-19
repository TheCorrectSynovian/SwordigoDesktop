# Changelog

All notable changes to Swordigo Desktop.

## [v4.5r] — 2026-06-19

### Added
- **Save Editor in Launcher** — browse and edit `.gplayer` save files directly from the launcher window
  - Lists all saves with name, area, level, progress percentage, and playtime
  - Editable fields: Coins, Health, Mana, XP, Equipped Weapon, Keys
  - Text input fields with keyboard navigation (TAB/ESC/ENTER)
  - Creates `.bak` backup before writing
  - Success/failure status feedback

### Changed
- Launcher version bumped to v4.5r
- ARM64 `pthread_create` now discards thread functions (matching ARM32 behavior)

### Fixed
- ARM64 entity processing: NOP'd setup call at `0x580708` with `MOV X0, XZR` to prevent spinlock on empty entity lists

### Removed
- In-game save editor (Mods menu) — replaced by the launcher-integrated version

### Known Issues
- **ARM64**: Game freezes (spinlock) when entering Wastelands
- **ARM32**: Timer-based repeating spikes and boss gate triggers don't work
- **Launcher**: Instance icons show placeholder in .deb package installs

---

## [v4.0r] — 2026-06-18

### Added
- **ARM64 (AArch64) emulation** via Unicorn Engine ARM64 backend
- ARM64 ELF loader with full RELA relocations
- ARM64 JNI bridge (200+ functions)
- Dual-arch launcher — ARM32 and ARM64 instances side-by-side
- **GPU draw call batcher** — streaming VBO reduces draw calls from 80-140 to 15-25 per frame
- **FSR 1.0 upscaling** option
- **PolyMC-inspired launcher** — instance card grid with icons, version badges, arch labels
- Custom instance import via file dialog
- GLSL 330 shader migration

### Changed
- Launcher redesigned with card grid + detail panel layout
- Threading bridges now functional (mutex, cond, create, once)

---

## [v3.0r] — 2026-06-17

### Added
- **HiDPI / native resolution rendering**
- **SDL2 → SDL3 migration**
- Standalone RPM and DEB packaging
- Binary Selector v2 with v1.4.12 as default

### Fixed
- Death screen hang resolved
- Improved FBO scaler with Sharp Bilinear mode

---

## [v2.0r] — 2026-06-17

### Added
- **SRE PostFX pipeline** — SSAO, God Rays, Volumetric Light Shafts
- 7 visual presets (Cinematic, Retro, Fantasy, Noir, Ethereal, Atmospheric)
- **Unified Launcher GUI** with binary selection + graphics API picker
- SHA-256 binary validation
- Depth buffer as texture for shader effects

---

## [v1.0r] — 2026-06-16

### Added
- Initial release
- Custom ARM ELF loader with Unicorn Engine
- JNI bridge layer (200+ functions)
- OpenGL rendering, OpenAL audio
- Keyboard + gamepad controls
- Save system persistence
