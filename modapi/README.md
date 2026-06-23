# Swordigo Desktop — Modding & API Documentation

Welcome to the **Swordigo Desktop** modding and API reference. This documentation covers everything you need to create mods, understand the runtime architecture, and extend the game.

## 📖 Table of Contents

### Getting Started
- [**Modding Guide**](modding-guide.md) — How to create, install, and manage mods
- [**Architecture Overview**](architecture.md) — How the runtime works (SRT/SRE/Host)

### API Reference
- [**SRE Hook API**](sre-hooks.md) — All 34+ engine hooks with offsets, signatures, and purposes
- [**Lua Scripting API**](lua-api.md) — Mini.*, LNI.*, and standard library extensions
- [**GUI API**](gui-api.md) — Creating custom UI elements from SRE code
- [**Mod Config Protocol**](mod-config.md) — How mods communicate with the engine via shared memory
- [**Music System**](music-api.md) — Music playback, replacement, and modding
- [**Save File Format**](save-format.md) — .gplayer protobuf format and Save Editor API
- [**VFS (Virtual Filesystem)**](vfs-api.md) — Asset layering and file interception

### File Formats
- [**Scene Format (.scene)**](formats/scene-format.md) — Protobuf scene file structure
- [**POD Model Format (.POD)**](formats/pod-format.md) — PowerVR model format (vertices, meshes, materials)
- [**PVR Texture Format (.pvr)**](formats/pvr-format.md) — PowerVR compressed texture format (ETC1)
- [**Protobuf Wire Format**](formats/protobuf-wire.md) — The protobuf reader/writer utility

### Runtime Reference
- [**Data Layout**](data-layout.md) — Directory structure and file locations
- [**Binary Selector**](binary-selector.md) — Instance management, manifests, and custom binaries
- [**Input System**](input-system.md) — Keybindings, macros, and touch zone emulation
- [**PostFX System**](postfx.md) — Post-processing effects and presets
- [**FBO Scaler**](fbo-scaler.md) — Rendering pipeline and scaling modes
- [**Overlay System**](overlay.md) — SRT runtime overlay and inventory editor

### For Developers
- [**Building from Source**](building.md) — Makefile targets and dependencies
- [**Debug Overlay (F3)**](debug-overlay.md) — Runtime debug information
- [**Keyboard Shortcuts**](shortcuts.md) — All function key shortcuts

---

## Quick Start — Creating Your First Mod

```bash
# 1. Create a mod directory
mkdir -p ~/.local/share/swordigo-desktop/mods/my-first-mod

# 2. Create mod.json
cat > ~/.local/share/swordigo-desktop/mods/my-first-mod/mod.json << 'EOF'
{
  "id": "my-first-mod",
  "name": "My First Mod",
  "version": "1.0",
  "author": "YourName",
  "description": "Replaces the wastelands music",
  "type": "music",
  "replace": {
    "wastelands": "my_custom_track"
  }
}
EOF

# 3. Add your replacement music file
cp my_custom_track.mp3 ~/.local/share/swordigo-desktop/mods/my-first-mod/assets/music/

# 4. Launch the game — the launcher will detect and list your mod!
```

## Architecture at a Glance

```
┌──────────────────────────────────────────────────┐
│                    HOST (x86_64)                  │
│  ┌──────────┐  ┌───────────┐  ┌────────────────┐ │
│  │ Launcher │  │ FBO/PostFX│  │   JNI Bridge   │ │
│  │ (ImGui)  │  │  Scaler   │  │ (~400 bridges) │ │
│  └──────────┘  └───────────┘  └───────┬────────┘ │
│                                       │          │
│  ┌──────────────────────────┐        │          │
│  │     Unicorn Emulator     │←───────┘          │
│  │      (ARM64 → x86)      │                    │
│  └──────────┬───────────────┘                    │
│             │                                    │
│  ┌──────────▼───────────────┐                    │
│  │    Guest Memory (512MB)  │                    │
│  │  ┌─────────────────────┐ │                    │
│  │  │  libswordigo.so     │ │                    │
│  │  │  (ARM64 game binary)│ │                    │
│  │  ├─────────────────────┤ │                    │
│  │  │  libsre.so (SRE)    │ │                    │
│  │  │  hooks + mod APIs   │ │                    │
│  │  ├─────────────────────┤ │                    │
│  │  │  Shared Config      │ │                    │
│  │  │  (0x48000-0x4AFFF)  │ │                    │
│  │  └─────────────────────┘ │                    │
│  └──────────────────────────┘                    │
└──────────────────────────────────────────────────┘
```

## License

This documentation is part of the Swordigo Desktop project.
Swordigo is a trademark of Touch Foo. This project is an unofficial desktop port.
