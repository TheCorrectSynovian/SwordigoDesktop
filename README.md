# Swordigo Desktop

Swordigo Desktop is a native Linux porting project that enables playing the classic action-adventure game Swordigo on modern desktop environments. It utilizes a custom ELF loader and an ARMv7 emulation layer to bridge the game's original native code with host system libraries (OpenGL, OpenAL, SDL2).

## Current State: Working Model Stage 1

The project has reached a major milestone where the game engine is successfully booting and rendering interactive content.

### What Works:
- **Core Engine:** Custom ARM ELF loading and relocation are fully functional.
- **Visuals:** Textures and 3D models are rendering correctly. The title screen and main menus are visible and interactive.
- **JNI Bridge:** A comprehensive JNI layer provides the necessary Android environment for the native engine.
- **Infrastructure:** Asset management via `AAssetManager` shims is complete.

### Known Issues & Work in Progress:
- **Controls:** WASD to touch coordinate translation is currently buggy and requires refinement for better playability.
- **Audio:** Sound effects and music support have been implemented but remain buggy (occasional crashes or missing tracks).
- **Persistence:** Save file support and settings persistence are not yet fully working (Filesystem shims are being improved).
- **Input:** Mouse click to touch mapping is functional but requires better scaling for different resolutions.

## Technical Architecture

- **Loader:** Surgical ELF loader for `libswordigo.so` with dynamic symbol resolution.
- **Emulation:** Powered by [Unicorn Engine](https://www.unicorn-engine.org/) for high-performance ARMv7 instruction execution.
- **Graphics:** Direct translation of guest GLES calls to host OpenGL.
- **Platform:** Uses SDL2 for window management and input handling.

## Build Instructions

```bash
# Ensure you have the dependencies: unicorn, sdl2, openal, libgl
make swordigo_boot
./swordigo_boot
```

*Note: This project does not distribute original game assets or binaries. Users must provide their own `libswordigo.so` and `assets/` from a legal copy of the game.*
