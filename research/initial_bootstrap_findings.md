# Findings - 2026-06-14 - Agent 1

## Required prompt context

- `prompt/Phrase 1/main.md` and `prompt/Phrase 1/preFeed.md` were read before code work. They define this as a research/bootstrap task, not a gameplay rewrite or large renderer implementation.
- Shared research files were read before investigation: `research/findings.md`, `research/questions.md`, `research/hypotheses.md`, `research/progress_log.md`, and `research/agent_messages.txt`.

## Vita loader architecture

- The Vita port is a native Android `.so` loader, not an engine rewrite. Its README says it loads the official Android ARMv7 executable, resolves imports with native functions, patches it, and runs it inside a minimalist Android-like environment. Evidence: `docs/research/vita/vistaPort/swordigo-vita-master/README.md`.
- The loader reads `ux0:data/swordigo/libswordigo.so`, calls `so_file_load`, relocates, resolves imports from `default_dynlib`, applies `patch_game`, flushes caches, and runs `.init_array`. Evidence: `docs/research/vita/vistaPort/swordigo-vita-master/loader/main.c:1693`, `docs/research/vita/vistaPort/swordigo-vita-master/loader/main.c:1697`, `docs/research/vita/vistaPort/swordigo-vita-master/loader/main.c:1701`.
- `so_file_load` reads the shared object into memory and delegates to `_so_load`; `so_relocate` handles ARM `R_ARM_ABS32`, `R_ARM_RELATIVE`, `R_ARM_GLOB_DAT`, and `R_ARM_JUMP_SLOT`; `so_resolve` fills unresolved imports from loaded dependencies, the loader's default symbol table, or `vglGetProcAddress`; `so_initialize` calls each `.init_array` entry. Evidence: `docs/research/vita/vistaPort/swordigo-vita-master/loader/so_util.c:296`, `docs/research/vita/vistaPort/swordigo-vita-master/loader/so_util.c:321`, `docs/research/vita/vistaPort/swordigo-vita-master/loader/so_util.c:421`, `docs/research/vita/vistaPort/swordigo-vita-master/loader/so_util.c:516`.
- The loader fabricates JNI tables by writing function pointers into `fake_vm` and `fake_env`, then passes `&fake_env` to exported JNI functions. Evidence: `docs/research/vita/vistaPort/swordigo-vita-master/loader/main.c:1703`, `docs/research/vita/vistaPort/swordigo-vita-master/loader/main.c:1709`, `docs/research/vita/vistaPort/swordigo-vita-master/loader/main.c:1754`.

## Startup sequence evidence

- Android Java loads `libopenal-soft.so` and `libswordigo.so` in `MainActivity`'s static initializer. Evidence: `reference/decompiled/decompClasses/com.touchfoo.swordigo/MainActivity.java:273`.
- Android `MainActivity.onCreate` calls `Native.setFilesDir`, `Native.setCacheDir`, and `Native.setAssetManager` before creating the `GameView` and renderer. Evidence: `reference/decompiled/decompClasses/com.touchfoo.swordigo/MainActivity.java:31`, `reference/decompiled/decompClasses/com.touchfoo.swordigo/MainActivity.java:43`, `reference/decompiled/decompClasses/com.touchfoo.swordigo/MainActivity.java:46`, `reference/decompiled/decompClasses/com.touchfoo.swordigo/MainActivity.java:52`.
- Android `GameRenderer.onSurfaceCreated` constructs `MusicPlayer`, then calls `Native.setupNativeInterface()` and `Native.setupApplication()` once. Evidence: `reference/decompiled/decompClasses/com.touchfoo.swordigo/GameRenderer.java:30`, `reference/decompiled/decompClasses/com.touchfoo.swordigo/GameRenderer.java:32`, `reference/decompiled/decompClasses/com.touchfoo.swordigo/GameRenderer.java:33`.
- Android `GameRenderer.onSurfaceChanged` calls `Native.setApplicationViewSize(width, height, isPad)` after setting the GL viewport and matrices. Evidence: `reference/decompiled/decompClasses/com.touchfoo.swordigo/GameRenderer.java:46`, `reference/decompiled/decompClasses/com.touchfoo.swordigo/GameRenderer.java:52`.
- Android `GameRenderer.onDrawFrame` accumulates time, calls `Native.updateApplication(0.016666668f)` for fixed 60 Hz steps, then calls `Native.drawApplication()`. Evidence: `reference/decompiled/decompClasses/com.touchfoo.swordigo/GameRenderer.java:58`, `reference/decompiled/decompClasses/com.touchfoo.swordigo/GameRenderer.java:67`, `reference/decompiled/decompClasses/com.touchfoo.swordigo/GameRenderer.java:69`, `reference/decompiled/decompClasses/com.touchfoo.swordigo/GameRenderer.java:76`.
- Vita resolves function pointers for `setFilesDir`, `setCacheDir`, `setAssetManager`, `drawApplication`, `setupNativeInterface`, `setupApplication`, `setApplicationViewSize`, `handleApplicationLaunch`, `applicationDidBecomeActive`, `updateApplication`, `initMusicPlayer`, `handleTouchEvent`, and `googleSignInCompleted`. Evidence: `docs/research/vita/vistaPort/swordigo-vita-master/loader/main.c:1754`.
- Vita call order after load is: `setFilesDir`, `setCacheDir`, `setAssetManager`, `googleSignInCompleted`, `handleApplicationLaunch`, GL setup, `initMusicPlayer`, `setupNativeInterface`, `setupApplication`, `setApplicationViewSize`, `applicationDidBecomeActive`, then the loop calls `updateApplication(deltaSecond)` and `drawApplication()`. Evidence: `docs/research/vita/vistaPort/swordigo-vita-master/loader/main.c:1779`, `docs/research/vita/vistaPort/swordigo-vita-master/loader/main.c:1788`, `docs/research/vita/vistaPort/swordigo-vita-master/loader/main.c:1791`, `docs/research/vita/vistaPort/swordigo-vita-master/loader/main.c:1803`, `docs/research/vita/vistaPort/swordigo-vita-master/loader/main.c:1807`, `docs/research/vita/vistaPort/swordigo-vita-master/loader/main.c:1810`, `docs/research/vita/vistaPort/swordigo-vita-master/loader/main.c:1813`, `docs/research/vita/vistaPort/swordigo-vita-master/loader/main.c:1816`, `docs/research/vita/vistaPort/swordigo-vita-master/loader/main.c:1894`.

## Local native library symbol facts

- Local `reference/lib/armeabi-v7a/libswordigo.so` is ELF32 ARM EABI5, shared object, stripped. Evidence: `readelf -h reference/lib/armeabi-v7a/libswordigo.so`.
- Local dynamic dependencies are `libopenal-soft.so`, `libz.so`, `libGLESv1_CM.so`, `libEGL.so`, `libandroid.so`, `liblog.so`, `libstdc++.so`, `libm.so`, `libc.so`, and `libdl.so`. Evidence: `readelf -d reference/lib/armeabi-v7a/libswordigo.so`.
- Local exported JNI functions include `setupNativeInterface`, `setupApplication`, `setFilesDir`, `setCacheDir`, `setAssetManager`, `setApplicationViewSize`, `updateApplication`, and `drawApplication`. Evidence: `readelf -Ws reference/lib/armeabi-v7a/libswordigo.so`.
- Local `reference/lib/armeabi-v7a/libswordigo.so` does not export `Java_com_touchfoo_swordigo_Native_handleApplicationLaunch` or `Java_com_touchfoo_swordigo_Native_googleSignInCompleted`; both `readelf -Ws` and `strings -a` checks returned no matches. This is a version mismatch risk relative to the Vita loader source. Evidence: local symbol/string checks performed 2026-06-14.
- Native symbol table exposes engine-level C++ symbols near the JNI wrappers, including `Caver::NewCaverShell`, `Caver::CaverShell::InitApplication`, `Caver::CaverShell::Update`, `Caver::CaverShell::Render`, `Caver::SetResourcesPath`, `Caver::LoadProtobufMessageFromFile`, and `Caver::RenderingContext::RenderingContext`. Evidence: `readelf -Ws reference/lib/armeabi-v7a/libswordigo.so | c++filt`.

## JNI requirements observed from Vita

- Minimum fake JNI for initialization includes `GetEnv`, `FindClass`, `NewGlobalRef`, `DeleteGlobalRef`, `NewObjectV`, `GetObjectClass`, `GetMethodID`, `CallObjectMethodV`, `CallBooleanMethodV`, `CallIntMethodV`, `CallLongMethodV`, `CallVoidMethodV`, field accessors, static method accessors, string functions, array functions, `RegisterNatives`, `GetJavaVM`, and `GetStringUTFRegion`. Evidence: `docs/research/vita/vistaPort/swordigo-vita-master/loader/main.c:1703`, `docs/research/vita/vistaPort/swordigo-vita-master/loader/main.c:1709`.
- Vita maps Java method names to small internal IDs for music and achievements: `<init>`, `isAgeKnown`, `getPlatformConsentState`, `loadFile`, `play`, `pause`, `stop`, `setLooping`, `setVolume`, and `reportAchievementProgress`. Evidence: `docs/research/vita/vistaPort/swordigo-vita-master/loader/main.c:1334`.
- Vita routes `loadFile`, `play`, `pause`, `stop`, `setLooping`, and `setVolume` calls to its own music implementation. Evidence: `docs/research/vita/vistaPort/swordigo-vita-master/loader/main.c:1501`, `docs/research/vita/vistaPort/swordigo-vita-master/loader/main.c:1526`.

## Android API dependencies

- Native imports require `AAssetManager_fromJava`, `AAssetManager_open`, `AAsset_openFileDescriptor`, `AAsset_read`, `AAsset_getLength`, and `AAsset_close`. Evidence: `readelf -Ws reference/lib/armeabi-v7a/libswordigo.so`.
- Vita implements assets by mapping `AAssetManager_open` to `ux0:data/swordigo/assets/<name>` and treating assets as file descriptors. Evidence: `docs/research/vita/vistaPort/swordigo-vita-master/loader/main.c:824`, `docs/research/vita/vistaPort/swordigo-vita-master/loader/main.c:833`, `docs/research/vita/vistaPort/swordigo-vita-master/loader/main.c:848`, `docs/research/vita/vistaPort/swordigo-vita-master/loader/main.c:854`.
- Native imports require OpenGL ES 1.x / EGL symbols such as `glBindTexture`, `glDrawArrays`, `glDrawElements`, `glTexImage2D`, `glCompressedTexImage2D`, `glViewport`, `glGetString`, and `eglGetProcAddress`. Evidence: `readelf -Ws reference/lib/armeabi-v7a/libswordigo.so`.
- Native imports require OpenAL symbols such as `alBufferData`, `alGenSources`, `alSourcePlay`, `alSourceStop`, `alcOpenDevice`, `alcCreateContext`, and related context/device calls. Evidence: `readelf -Ws reference/lib/armeabi-v7a/libswordigo.so`.
- Native imports require libc/pthread/zlib style services including file I/O, directory iteration, `pthread_*`, `gz*`, `inflate*`, and `deflate*`. Evidence: `readelf -Ws reference/lib/armeabi-v7a/libswordigo.so`.

## Asset formats

- `assets/resources/gamedata.gdata` decodes with `protoc --decode_raw` and begins with repeated field `1` records containing item-like fields: field `2` resource id such as `brasssword`, field `3` display name such as `Brass Sword`, field `4` stat text, field `5` description, and numeric fields `1`, `6`, `7`, `8`, `9`. Evidence: `assets/resources/gamedata.gdata`; experiment `research/experiments/experiment_001.md`.
- `assets/resources/menu.scene` decodes with `protoc --decode_raw` and begins with repeated field `1` scene objects. Observed object names include `Background`, `DirectionalLight`, and `darkhero`; component-like submessages appear in field `3`, with high-numbered extension fields such as `101`, `102`, `130`, and `200`. Evidence: `assets/resources/menu.scene`; experiment `research/experiments/experiment_001.md`.
- `assets/resources/player.gstate` and `assets/resources/newplayer.gstate` decode with `protoc --decode_raw`. Both include `town_herohouse`, `spawn_default`, and `map`; `player.gstate` additionally contains inventory/spell strings such as `legendsword`, `platearmor`, `bolt`, `bomb`, `hookshot`, and `dimension`. Evidence: `assets/resources/player.gstate`, `assets/resources/newplayer.gstate`; experiment `research/experiments/experiment_001.md`.
- Follow-up verification found embedded Lua source and bytecode-like `LuaQ` data in at least `assets/resources/plains_woodkeep3.scene`, plus native symbols for `Caver::Program`, `Caver::ProgramState`, `Caver::ProgramComponent`, `Caver::EntityControllerComponent`, and `Caver::MonsterEntityComponent` symbols. This confirms Lua/program scripting exists, but exact protobuf field numbers still need controlled decode evidence. Evidence: `rg -a "Program.Wait|EntityController|LuaQ" assets/resources/plains_woodkeep3.scene`; `readelf -Ws reference/lib/armeabi-v7a/libswordigo.so | c++filt`.

### Addendum: 2026-06-14 (Agent 2) - Asset Evidence

- **.scl Format Verification**: `assets/resources/monsters.scl` decodes as a protobuf collection. Field `1` is the collection name (`"monsters"`), and field `2` contains repeated records. Each record (field `2`) represents an entity class (e.g., `"Cave Bat"` in field `2.1.2`). Field `2.1.3` contains repeated component definitions (e.g., `"EntityInfo"`, `"KeyframeAnimation"`). Evidence: `protoc --decode_raw < "assets/resources/monsters.scl"`.
- **Lua/Program Scripting Evidence**: In `assets/resources/plains_woodkeep3.scene`, components of type `"Program"` (field `3.1`) contain nested fields for scripts. 
    - Field `3.157.2.1`: Lua source code string.
    - Field `3.157.2.2`: `LuaQ` bytecode.
    - Observed globals/modules in scripts: `Scene.CreateObject`, `Scene.Find`, `Math.RandomInt`, `Program.Wait`, `CollisionShape.SetEnabled`, `Game.SetCinematicMode`, `DoorController.Close`, `SoundLibrary.PlayEffect`, `Camera.FocusAtPoint`, `EntityController.SetMoveSpeed`, `MusicPlayer.PlayMusic`.
    - Evidence: `protoc --decode_raw < "assets/resources/plains_woodkeep3.scene"`.
- **Quest & Trigger Data**: `assets/resources/gamedata.gdata` contains dedicated messages for quests and triggers.
    ### Quest & Trigger Data
    - Message Type `3`: Quests. Field `1` is internal ID, `2` is display name, `4` is target scene.
    - Message Type `5`: Quest Triggers. Field `2` is quest ID, `3` is scene name, `4` is trigger/object ID.
    - Evidence: `protoc --decode_raw < "assets/resources/gamedata.gdata"`.

    ## Architecture & Dependencies
    See [Data Dependency Map](data_dependency_map.md) for a comprehensive overview of how game resources interact.


## Tooling limits found

- Host `objdump` cannot disassemble the local ARM shared object and reports `can't disassemble for architecture UNKNOWN`; no `llvm-objdump`, `arm-linux-gnueabi-objdump`, or `arm-none-eabi-objdump` was found in `PATH`. Evidence: experiment `research/experiments/experiment_001.md`.
