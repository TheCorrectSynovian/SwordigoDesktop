# Hypotheses - 2026-06-14

## Minimum Linux compatibility layer

- Hypothesis: A Linux bootstrap should first emulate the Vita strategy: load/relocate ARM `libswordigo.so`, provide a fake JNI environment, map Android asset APIs to local `assets/resources`, provide OpenAL, provide GLES/EGL calls, and call the JNI lifecycle directly.
- Confidence: Medium.
- Evidence: Vita successfully uses fake JNI tables and direct JNI calls in `docs/research/vita/vistaPort/swordigo-vita-master/loader/main.c:1703` through `docs/research/vita/vistaPort/swordigo-vita-master/loader/main.c:1817`; local `libswordigo.so` imports Android asset, OpenAL, GLES/EGL, pthread, zlib, libc, and logging APIs.
- Risk: Local `libswordigo.so` is ARM while host is x86_64, so the loader must include ARM emulation/translation or run in an ARM userspace. Direct native x86_64 `dlopen` is impossible.

## Local startup order

- Hypothesis: For the local Android library, the Java startup order is sufficient without `handleApplicationLaunch`: set dirs/assets in `MainActivity.onCreate`, then `MusicPlayer.initMusicPlayer`, `setupNativeInterface`, `setupApplication`, `setApplicationViewSize`, `updateApplication`, and `drawApplication`.
- Confidence: Medium-high for the local Java path, low for cross-version Vita parity.
- Evidence: Java never references `handleApplicationLaunch`; local native library does not export it; Vita source does call it, implying version-specific behavior.

## Asset format

- Hypothesis: `.gdata`, `.gstate`, and `.scene` are Protocol Buffers messages consumed by generated protobuf-lite code compiled into `libswordigo.so`.
- Confidence: High that the files are protobuf wire format; medium on exact generated schema until fields are mapped.
- Evidence: `protoc --decode_raw` produced structured output for all three file types; native symbols include `Caver::LoadProtobufMessageFromFile` and many `Caver::Proto::*` symbols.

## Lua/program scripting

- Hypothesis: Some scene behavior is represented as `Caver::Program` / Lua data embedded in protobuf scene components.
- Confidence: Medium-high for the existence of embedded scripts, low for exact field numbers and scope.
- Evidence: `assets/resources/plains_woodkeep3.scene` contains readable Lua source and `LuaQ` chunks; native symbols include `Caver::Program`, `Caver::ProgramState`, and `Caver::ProgramComponent`.
- Risk: Agent 2's stronger claim that field `158` holds script data has not been validated with controlled raw protobuf field tracing.
