# Swordigo Linux Loader Design

## 1. Overview
The goal is to load and execute the ARMv7 `libswordigo.so` on an x86_64 Linux host. Since the host architecture differs, this requires a combination of **ELF loading**, **Relocation**, **Symbol Resolution**, and **ARM Emulation**.

## 2. Loader Architecture (Based on Vita Loader)
The Vita loader provides a proven blueprint for loading Android `.so` files.

### 2.1 Core Components
- **ELF Loader (`so_util.c`)**: Manages reading the ELF header, program headers, and sections.
- **Relocator**: Handles ARM-specific relocation types (`R_ARM_ABS32`, `R_ARM_RELATIVE`, `R_ARM_GLOB_DAT`, `R_ARM_JUMP_SLOT`).
- **Fake JNI Environment**: Fabricates a `JavaVM` and `JNIEnv` struct with function pointers pointing to native compatibility shims.
- **Static Hooking**: Uses a jump table (trampolines) to redirect native ARM calls to host-native (or emulated) C functions.

### 2.2 Linux Adaptation Requirements
- **Emulation Layer**: On x86_64, a user-mode ARM emulator (like **QEMU User** or a custom **Unicorn**-based runner) is mandatory. 
- **Memory Mapping**: The `.so` expects a 32-bit address space. The loader must `mmap` segments within the low 4GB if possible, or handle 64-bit pointers if the emulator supports it.

## 3. Implementation Details

### 3.1 Memory Mapping
- **Segments**: `PT_LOAD` segments must be mapped with correct permissions (RX for code, RW for data).
- **Alignment**: segment alignment (typically 4KB) must be respected.

### 3.2 Relocations & Symbol Resolution
- **Internal Symbols**: Resolved by adding the base load address to the symbol's `st_value`.
- **External Imports**: Must be redirected to "bridge" functions that handle the transition between the engine's ARM code and the host's x86_64 environment.

### 3.3 JNI Requirements (Minimum for Boot)
| JNI Function | Usage in Swordigo |
|---|---|
| `GetEnv` | Entry point for JNI environment. |
| `FindClass` | Resolves `com/touchfoo/swordigo/Native`. |
| `GetStaticMethodID` | Used for utility calls (e.g., `startTextInput`). |
| `NewStringUTF` | Passing paths and IDs to the engine. |

### 3.4 Android Compatibility Layer
| API | Linux Implementation |
|---|---|
| **AssetManager** | Map `AAssetManager_*` to standard `fopen`/`fread` on the `assets/` directory. |
| **Log (`liblog`)** | Map `__android_log_print` to `printf` or a file logger. |
| **OpenGL ES** | Use **libepoxy** or **SDL_GL_GetProcAddress** to bridge GLES 1.1 calls to host OpenGL. |
| **Audio (`libopenal`)** | The engine already uses OpenAL; link to host `libopenal.so`. |

## 4. Minimum Viable Boot Path
To reach `setupApplication()`, the loader must:
1. Load `libswordigo.so` and `libopenal-soft.so`.
2. Resolve relocations.
3. Call `Java_com_touchfoo_swordigo_Native_setFilesDir`.
4. Call `Java_com_touchfoo_swordigo_Native_setCacheDir`.
5. Call `Java_com_touchfoo_swordigo_Native_setAssetManager` (passing a dummy pointer).
6. Call `Java_com_touchfoo_swordigo_Native_setupNativeInterface`.
7. Call `Java_com_touchfoo_swordigo_Native_setupApplication`.

## 5. Potential Roadblocks
- **ARM vs x86_64 Calling Convention**: The bridge must handle register mapping (e.g., R0-R3 to RDI, RSI, etc.) if calling between host and emulated code.
- **Version Mismatches**: Symbols like `handleApplicationLaunch` found in Vita but missing in local `.so`.
