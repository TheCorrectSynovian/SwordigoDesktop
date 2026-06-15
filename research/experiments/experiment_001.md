# Experiment 001 - Native and Asset Evidence Baseline

Date: 2026-06-14

## Purpose

Establish source-backed facts for the bootstrap path without writing engine code:

- Verify local `libswordigo.so` architecture, dependencies, and exported JNI functions.
- Check whether Vita-only symbols `handleApplicationLaunch` and `googleSignInCompleted` exist in the local library.
- Verify whether `.gdata`, `.gstate`, and `.scene` files are protobuf wire format.
- Check whether the local environment can disassemble ARM code.

## Files modified

- `research/findings.md`
- `research/questions.md`
- `research/hypotheses.md`
- `research/boot_sequence.md`
- `research/android_dependencies.md`
- `research/protobuf_schema.md`
- `research/progress_log.md`
- `research/agent_messages.txt`
- `research/experiments/experiment_001.md`

## Commands

- `readelf -h reference/lib/armeabi-v7a/libswordigo.so`
- `readelf -d reference/lib/armeabi-v7a/libswordigo.so`
- `readelf -Ws reference/lib/armeabi-v7a/libswordigo.so`
- `strings -a reference/lib/armeabi-v7a/libswordigo.so`
- `file assets/resources/gamedata.gdata assets/resources/player.gstate assets/resources/newplayer.gstate assets/resources/menu.scene assets/resources/hero.scene`
- `xxd -l 96 assets/resources/gamedata.gdata`
- `xxd -l 96 assets/resources/player.gstate`
- `xxd -l 96 assets/resources/menu.scene`
- `protoc --decode_raw < assets/resources/gamedata.gdata`
- `protoc --decode_raw < assets/resources/menu.scene`
- `protoc --decode_raw < assets/resources/player.gstate`
- `protoc --decode_raw < assets/resources/newplayer.gstate`
- `objdump -d --start-address=0x2807ef --stop-address=0x281300 reference/lib/armeabi-v7a/libswordigo.so`
- `command -v llvm-objdump || command -v arm-linux-gnueabi-objdump || command -v arm-none-eabi-objdump || true`

## Expected outcome

- Native metadata should confirm ARM shared object and Android-style dependencies.
- JNI export list should confirm the target lifecycle functions.
- Protobuf decode should produce structured output for known game data assets.
- If ARM disassembly tools are installed, JNI wrappers can be inspected.

## Observed outcome

- `libswordigo.so` is ELF32, little-endian, ARM, EABI5, shared object, stripped.
- `DT_NEEDED` includes `libopenal-soft.so`, `libz.so`, `libGLESv1_CM.so`, `libEGL.so`, `libandroid.so`, `liblog.so`, `libstdc++.so`, `libm.so`, `libc.so`, and `libdl.so`.
- Exported local JNI symbols include:
  - `Java_com_touchfoo_swordigo_Native_setupNativeInterface`
  - `Java_com_touchfoo_swordigo_Native_setupApplication`
  - `Java_com_touchfoo_swordigo_Native_setFilesDir`
  - `Java_com_touchfoo_swordigo_Native_setCacheDir`
  - `Java_com_touchfoo_swordigo_Native_setAssetManager`
  - `Java_com_touchfoo_swordigo_Native_setApplicationViewSize`
  - `Java_com_touchfoo_swordigo_Native_updateApplication`
  - `Java_com_touchfoo_swordigo_Native_drawApplication`
- `Java_com_touchfoo_swordigo_Native_handleApplicationLaunch` and `Java_com_touchfoo_swordigo_Native_googleSignInCompleted` were not found by `readelf -Ws` or `strings -a` in the local library.
- `protoc --decode_raw` produced structured output for `gamedata.gdata`, `menu.scene`, `player.gstate`, and `newplayer.gstate`.
- Host `objdump` failed with `can't disassemble for architecture UNKNOWN`.
- No `llvm-objdump`, `arm-linux-gnueabi-objdump`, or `arm-none-eabi-objdump` was found in `PATH`.

## Next steps

- Install or provide an ARM-capable objdump and disassemble the local JNI wrappers.
- Compare the local library version against the Vita README's tested version.
- Build a tiny logging-only ARM execution experiment after choosing an emulation/translation strategy.
