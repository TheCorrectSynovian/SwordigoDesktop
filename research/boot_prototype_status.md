# Swordigo Boot Prototype Status

## Overview
This document tracks the implementation progress of the Linux boot prototype for Swordigo. The goal is to reach `setupNativeInterface()` and `setupApplication()`.

## Status: 2026-06-14

### Completed Work
- [x] Initial research on Vita loader architecture.
- [x] Initial research on Swordigo JNI exports and Android dependencies.
- [x] Designed high-level Linux loader architecture.
- [x] Created project structure in `src/`.
- [x] Implemented custom ELF loader in `src/loader/` with ARMv7 relocation support.
- [x] Implemented minimal JNI shims in `src/jni/` with standard function table offsets.
- [x] Implemented Android API shims (`log`, `AssetManager`) in `src/android/`.
- [x] Developed prototype launcher `swordigo_boot` that successfully loads the library and resolves JNI exports.

### Current Implementation Phase: E (First Boot Attempt)
- **Milestone Reached**: The launcher successfully loads `libswordigo.so`, applies relocations, and locates `setupNativeInterface` and `setupApplication`.
- **Milestone Blocked**: Execution of the native code is pending emulator integration.

### Blockers
- **ARM Execution**: The host is x86_64; `libswordigo.so` is ARMv7. Direct execution is impossible.
- **Emulator Integration**: Need to integrate a library like **Unicorn** to execute the functions from the host environment.

### Next Actions
1. Integrate **Unicorn Engine** to handle ARMv7 instruction execution.
2. Implement a "Bridge" between x86_64 host calls and Unicorn ARM state.
3. Map the JNI environment and `AssetManager` shims into the emulated memory space.
