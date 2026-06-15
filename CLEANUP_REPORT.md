# SwordigoDesktop — Cleanup & GitHub Publication Report

Generated: June 15, 2026

---

## A. Recommended Repository Structure

```
SwordigoDesktop/
├── src/
│   ├── android/        # Android API stubs & emulation
│   ├── jni/            # JNI bridge & marshalling
│   ├── loader/         # ELF loader
│   ├── main/           # Main entry points
│   ├── platform/       # Platform abstraction (display, emulator)
│   ├── prototype/      # Experimental code
│   ├── main.cpp        # Project entry point
│   └── loader_test.cpp # Isolated loader test
├── reference/
│   ├── src_java/        # Old Java stubs (moved here, gitignored)
│   ├── apk/             # Extracted APK contents
│   └── decompiled/      # Decompiled Java from original game
├── docs/
│   └── SCHEMA.md       # Project-authored schema documentation
├── Makefile            # Build system
├── run_swordigo.sh     # Launcher script
├── .gitignore          # Updated
└── README.md           # SHOULD BE CREATED
```


---

## B. Complete .gitignore

The file `.gitignore` has been completely rewritten. See `.gitignore` in the project root.

---

## C. Files Safe for GitHub (SAFE TO COMMIT)

### Source Code (Project-Authored)

| File | Category |
|------|----------|
| `src/android/asset_manager.c` | Original source — Android API stub |
| `src/android/asset_manager.h` | Original source |
| `src/android/log.c` | Original source — Android log stub |
| `src/android/log.h` | Original source |
| `src/jni/jni_bridge.cpp` | Original source |
| `src/jni/jni_bridge.h` | Original source |
| `src/jni/jni_layer.h` | Original source |
| `src/jni/jni_marshaller.cpp` | Original source |
| `src/jni/jni_shim.c` | Original source |
| `src/jni/jni_shim.h` | Original source |
| `src/loader/elf_loader.cpp` | Original source |
| `src/loader/elf_loader.h` | Original source |
| `src/loader/elf_types.h` | Original source |
| `src/loader/loader.c` | Original source |
| `src/loader/loader.h` | Original source |
| `src/loader_test.cpp` | Original source |
| `src/main.cpp` | Original source |
| `src/main/main.c` | Original source |
| `src/platform/display.cpp` | Original source |
| `src/platform/display.h` | Original source |
| `src/platform/emulator.cpp` | Original source |
| `src/platform/emulator.h` | Original source |
| `src/prototype/loader_arm.c` | Original source |


### Build & Project Configuration (Safe)

| File | Category |
|------|----------|
| `Makefile` | Build file — SAFE |
| `run_swordigo.sh` | Launcher script — SAFE |
| `Swordigo.desktop` | Desktop entry — NEEDS REVIEW (contains game name) |
| `SwordigoDesktop.iml` | IntelliJ module — SAFE (now gitignored) |

### Documentation (Project-Authored)

| File | Category |
|------|----------|
| `docs/SCHEMA.md` | Schema documentation — SAFE (original research) |

### Symbol Maps (Research Artifacts — PUBLIC)

| File | Size | Notes |
|------|------|-------|
| `symbols_1.1.txt` | 388 KB | Mangled C++ symbol table from v1.1 — RE research artifact |
| `symbols_1.4.6.txt` | 400 KB | Mangled C++ symbol table from v1.4.6 — RE research artifact |

### Research Notes (PUBLIC)

| File | Notes |
|------|-------|
| All 30 files in `research/` | Original reverse-engineering research documentation |

### Legacy / Moved

| File | Status |
|------|--------|
| `reference/src_java/` | Old Java stubs moved here; gitignored under `reference/` |

---


## D. Files That Must Not Be Uploaded

### Category 1: Proprietary Game Binary (⚠️ DO NOT DISTRIBUTE)

| File | Size | Reason |
|------|------|--------|
| `libswordigo.so` | 4.4 MB | Proprietary ARM native library from the original APK. Full copyrighted game engine binary. |

### Category 2: Extracted Game Assets (⚠️ DO NOT DISTRIBUTE)

Path: `assets/resources/` contains **919 files** including:

| Type | Count | Description |
|------|-------|-------------|
| `.pvr` (textures) | 506 | Character sprites, environment textures, UI elements |
| `.POD` (3D models) | 787 | Animated character models, props, environment geometry |
| `.png` (textures) | 259 | Additional textures |
| `.scene` (maps) | 232 | Game level definitions |
| `.wav` (audio) | 142 | Sound effects |
| `.scl` (scripts) | 94 | Game scripts |
| `.mp3` (music) | 18 | Background music tracks |
| `.fnt` (fonts) | 18 | Game fonts |
| `.atlas` (atlases) | 12 | Texture atlas definitions |
| `.gdata` / `.gstate` | 6 | Protobuf game data |

All of these are **copyrighted commercial game content** extracted from the original Swordigo APK.

### Category 3: Decompiled / Extracted Reference Materials (⚠️ DO NOT DISTRIBUTE)

Path: `reference/` contains **~12,600+ files** (307 MB total):

| Contents | Reason |
|----------|--------|
| Full extracted APK contents | AndroidManifest.xml, resources, native libs |
| Decompiled Java (jadx output) | Both Swordigo v1.1 and v1.4.6 decompiled code |
| Original `.so` libraries | Both versions (arm64-v8a + armeabi-v7a) |
| Android resources | Drawables, layouts, menus |

Includes **copyrighted decompiled code**, **third-party SDKs** (Google Play Services, AndroidX, Firebase), and **proprietary .so files**.

### Category 4: Symbol Files (PUBLIC — SAFE TO COMMIT)

| File | Size | Notes |
|------|------|-------|
| `symbols_1.1.txt` | 388 KB | Mapped symbol table from v1.1 — Research artifact |
| `symbols_1.4.6.txt` | 400 KB | Mapped symbol table from v1.4.6 — Research artifact |

These contain mangled C++ symbol names from the game library. They are reverse-engineering research outputs and are intentionally kept public as part of the project's research documentation.


### Category 5: Crash Dumps / Logs (Generated)

| File | Size | Reason |
|------|------|--------|
| `boot.log` | 122 MB | Generated log — DO NOT COMMIT |
| `boot2.log` | 41 KB | Generated log |
| `display_diag.log` | 44 KB | Generated log |
| `display_test.log` | 40 KB | Generated log |
| `mission12_14.log` | 36 KB | Generated log |
| `stability_test.log` | 1.9 MB | Generated log |
| `qemu_poc_loader_*.core` (×3) | 8.1 MB each | QEMU crash dumps |

### Category 6: Build Outputs / Object Files (Generated)

`swordigo_boot`, `swordigo_headless`, `loader_test`, `src/**/*.o`, `build/`

### Category 7: Internal / Debugging Tools

`analyze_syms`, `check_relocations`, `find_bad_ptr`, `verify_0x10000000`, `poc_loader`, `generate_poc.py`

### Category 8: Third-Party Content

`android-ndk-r26b-linux.zip` (457 MB NDK), `docs/research/vita/vistaPort/` (third-party Vita port), `docs/research/vita/*.zip`

### Category 9: Internal Prompts & Agent Config

`prompt/`, `research/` (needs review), `.tools/` (jadx decompiler binary)


---

## E. Legal & Copyright Concerns

### HIGH CONCERN

1. **`libswordigo.so`** — This is the entire compiled game engine (copyrighted by Touch Foo / Ville Mäkynen). **Must never be distributed.**

2. **`assets/resources/`** — Contains all game textures, audio, 3D models, scenes and game data. These are the game's copyrighted assets. **Must never be distributed.**

3. **`reference/`** — Contains decompiled Java code from the original APK. Even though it's been decompiled, it's a derivative work of the copyrighted application. Also contains full APK contents. **Must never be distributed.**

### MEDIUM CONCERN

4. **`research/native_symbol_map.md`** — Contains mapped symbol names from the game binary with address offsets. This is a RE artifact but may be considered derivative. (Kept public by author's decision.)

5. **Extracted `.so` files in `reference/lib/`** — Original game libraries. **Must not be distributed.**

6. **`symbols_*.txt`** — Symbol tables extracted from the proprietary `.so`. (Kept public by author's decision as research documentation.)

### LOW CONCERN (original research)

7. **Research files (`research/*.md`)** — Original observations, analysis, and documentation. Under fair use, these are likely publishable. (Kept public by author's decision.)

8. **`src/` and `src_java/`** — Original compatibility/stub code written by the project authors. **Safe to publish.**

9. **`docs/SCHEMA.md`** — Original schema documentation. **Safe to publish.**

### THIRD-PARTY CODE

10. **Vita port source (`docs/research/vita/vistaPort/`)** — Separate third-party project (swordigo-vita-master). Its copyright belongs to its respective authors. **Should not be redistributed without attribution.**


---

## F. Recommended Pre-Publication Actions

| # | Action | Details |
|---|--------|---------|
| 1 | **Delete** | Remove `libswordigo.so` from the working directory (4.4 MB) |
| 2 | **Delete** | Remove `reference/` directory entirely (307 MB of copyrighted content) |
| 3 | **Delete** | Remove `assets/resources/` directory (commercial game assets) |
| 4 | **Delete** | Remove all `*.log` and `*.core` files |
| 5 | **Delete** | Remove compiled binaries: `swordigo_boot`, `swordigo_headless`, `loader_test` |
| 6 | **Delete** | Remove forensic tools: `analyze_syms`, `check_relocations`, `find_bad_ptr`, `verify_0x10000000`, `poc_loader`, `generate_poc.py` |
| 7 | **Keep** | Keep `symbols_*.txt` — research artifacts made public |
| 8 | **Delete** | Remove `prompt/` |
| 9 | **Delete** | Remove `android-ndk-r26b-linux.zip` (457 MB) |
| 10 | **Delete** | Remove `docs/research/vita/` (third-party project archive) |
| 11 | **Keep** | Keep `research/` — research notes made public |
| 12 | **Done** | `src_java/` moved to `reference/src_java/` (covered by reference/ gitignore) |
| 13 | **Create** | Add a `README.md` with project description, build instructions, and legal attribution |

---

## G. Summary Statistics

| Metric | Value |
|--------|-------|
| Total directory size | **1.9 GB** |
| Total files (non-git) | **~13,596 files** |
| Source files (C/C++/Java/Python/sh) | **~30 files** (~244 KB) |
| Game assets (copyrighted) | **919 files** (59 MB) |
| Reference materials (copyrighted) | **~12,600 files** (307 MB) |
| Logs (generated) | **6 files** (123+ MB) |
| Binaries (generated/compiled) | **~10 files** (4.4+ MB) |
| Crash dumps | **3 files** (24 MB) |
| Safe-to-commit source | **~0.5 MB** |

**After cleanup, the repository size for public GitHub would be approximately < 1 MB** (just source code, Makefile, and a README).

---

*Generated by welcomecleaner pipeline — June 15, 2026*

