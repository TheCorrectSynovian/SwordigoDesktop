# Vita Correlation & Boot Equivalence Map

This document uses the PlayStation Vita port (`swordigo-vita-master`) as the ground truth to map the engine subsystems, compile their equivalents, and establish the shortest path to first-frame rendering on Linux.

---

## 1. Engine Subsystem Equivalence Map

| Android Engine System / Function | Vita Port Equivalent | Proposed Linux Equivalent |
| :--- | :--- | :--- |
| **JavaVM / JNIEnv Handshake** | Pre-allocated memory tables (`fake_vm`, `fake_env`) populated with mock function pointers at JNI offsets. | Simulating vtables in host memory populated with C++ shims/bridges. |
| **NDK AssetManager** (`AAssetManager_open`, etc.) | Mocked wrappers mapping relative asset paths (e.g. `resources/gamedata.gdata`) to standard `open()`, returning raw file descriptors. | Mocked wrappers mapping asset requests to standard file I/O on the host directory `assets/`. |
| **Asset seeking** (`AAsset_openFileDescriptor`) | Returns the raw file descriptor and sets a global `skip_close = 1` bypass flag. | Returns the raw file descriptor and sets a global `skip_close = 1` bypass flag. |
| **Music Playback** (`com.touchfoo.swordigo.MusicPlayer`) | Intercepted in `CallVoidMethodV` and routed to a local SoLoud wrapper (`audio_player.cpp`). | Intercepted in JNI shim and routed to a host audio player (e.g., SDL2_mixer or SoLoud). |
| **Sound Effects** (OpenAL) | Imports linked via `default_dynlib` directly to the Vita OpenAL wrapper. | Imports linked directly to host-installed `libopenal.so`. |
| **Graphics Context** (EGL/GL) | Handled by `vitaGL` initialization (`vglInitWithCustomThreshold`). | Handled by host windowing context (GLFW / SDL2 GL Context). |
| **GLES 1.1 Pipeline** (`glVertexPointer`, etc.) | Translated by `vitaGL` to the PS Vita's GXM API. | Forwarded directly to host OpenGL compatibility drivers. |
| **Shader Location Hooks** (`glGetUniformLocation`) | Fake wrapper replacing uniform `"texture"` with `"_texture"`. | Hooked or omitted if host OpenGL driver handles exact name matches. |
| **Attribute Location Hooks** (`glBindAttribLocation`) | Fake wrapper mapping index `2` to `"extents"` and `"vertcol"`. | Hooked or omitted if host OpenGL driver handles exact index bindings. |
| **Hardware Input** | Translates touch panel coordinates and gamepad buttons to touch points. | Translates mouse clicks and keyboard/gamepad buttons to touch points. |
| **Input Delivery** (`handleTouchEvent`) | Invokes native `handleTouchEvent` using simulated coordinates. | Invokes native `handleTouchEvent` using simulated coordinates. |
| **Achievements & Trophies** | Intercepted in JNI void method and forwarded to native Vita trophies. | Bypassed / stubbed (no-op). |
| **Cloud Save Syncing** | Completely ignored; save commands are skipped. | Bypassed / stubbed (no-op). |
| **Local Save Files** | Standard filesystem writes directly to folder path set in `setFilesDir`. | Standard filesystem writes directly to folder path set in `setFilesDir`. |

---

## 2. Vita Subsystem Classification

### Bypassed (Ignored Completely)
*   **Google Play Games Cloud Saves**: Cloud sync routines (`loadSnapshot`, `saveSnapshot`, etc.) are ignored.
*   **Ad Networks**: Native ad call (`ShowInterstitialAd`) is patched out with `ret0` to prevent freezes.
*   **In-app Purchases**: Shop triggers and purchases are ignored.
*   **Local Reference Frame Tracking**: `PushLocalFrame`, `PopLocalFrame`, `DeleteLocalRef`, and `DeleteGlobalRef` are stubbed to do nothing.

### Mocked / Stubbed
*   **JNI Environmental Functions**: `ThrowNew`, `RegisterNatives`, and string release functions are dummy wrappers.
*   **Google Sign-In**: `googleSignInCompleted` and `isGoogleGameServicesAvailable` return success (`1`) immediately to bypass loading overlays.
*   **Android System Properties**: `__system_property_get` is stubbed to do nothing.

### Fully Implemented
*   **Dynamic Relocation & Import Resolution**: A custom ELF loader parses `libswordigo.so`, maps segments, and resolves dynamic symbols.
*   **Local Music Player**: Playback of background themes uses a custom SoLoud music wrapper.
*   **Local Save Files**: Player slot progress is saved natively into local `.gstate` files in the save folder.
*   **Input Conversion**: Gamepad buttons are mapped directly to touchscreen coordinates (e.g. CROSS maps to Jump coordinates, SQUARE to Attack coordinates).

---

## 3. Shortest Path to First Frame (Success Path)

The absolute minimum set of operations required to transition the guest code from `setupApplication()` to `drawApplication()` without crashing is as follows:

```
[Start]
   │
   ▼
[JNI: setFilesDir(&fake_env, 0, "path")]
   │
   ▼
[JNI: setCacheDir(&fake_env, 0, "path")]
   │
   ▼
[JNI: setAssetManager(&fake_env, 0, NULL)]
   │
   ▼
[JNI: googleSignInCompleted(&fake_env, 0, 0)]
   │
   ▼
[JNI: handleApplicationLaunch(&fake_env, 0)]
   │
   ▼
[Bind OpenGL Context]
   │
   ▼
[JNI: initMusicPlayer(&fake_env, 0)]
   │
   ▼
[JNI: setupNativeInterface(&fake_env, 0)]
   │
   ▼
[JNI: setupApplication(&fake_env, 0)]
   │
   ▼
[JNI: setApplicationViewSize(&fake_env, 0, W, H, 1)]
   │
   ▼
[JNI: applicationDidBecomeActive(&fake_env, 0)]
   │
   ▼
[Loop: updateApplication(&fake_env, 0, dt)]
   │
   ▼
[Loop: drawApplication(&fake_env, 0)]
   │
   ▼
[Swap Buffers]
```

### Minimum Required Subsystems for First Frame
1.  **Fake JNI Environment**: Essential to support VM/Env callbacks.
2.  **Asset Manager Wrapper**: Essential to read `menu.scene` and asset templates.
3.  **Host Window & GL Context**: Essential to execute fixed-function draw instructions.
4.  **PVRTC-to-RGBA Decompressor**: Essential to load and draw title screen textures.
