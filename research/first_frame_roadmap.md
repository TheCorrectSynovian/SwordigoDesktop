# First Frame Roadmap & Recon

This document defines the roadmap to first-frame rendering after `setupApplication()` completes, tracing the scene bootstrap chain, comparing against the Vita loader implementation, and identifying the next immediate blockers.

---

## 1. Scene Bootstrap Chain

The chain of events from invoking `setupApplication()` to displaying the first frame is structured as follows:

```
[Host Loader]
   │
   ▼
[JNI: setupApplication()]
   │
   ▼
[Caver::NewCaverShell(0, NULL)]   <─── Instantiates the CaverShell singleton (0x001cd5f1)
   │
   ▼
[CaverShell::InitApplication()]   <─── Orchestrates resource loading
   │
   ├─── Loads global databases: 'sounds.sounds', 'gamedata.gdata' via Caver::LoadProtobufMessageFromFile
   ├─── Populates FontLibrary, ModelLibrary, and SoundLibrary
   ├─── Loads main menu world: 'menu.scene'
   │
   ▼
[Build Scene Graph]
   │
   ├─── Creates 'Background', 'DirectionalLight', 'darkhero', 'sword', and 'obj1-5' environment objects
   ├─── Allocates 'obj3' (UtilityShape) with Program component executing embedded Lua script
   │
   ▼
[Lua VM Execution]
   │
   ├─── ObjectLinkController links 'sword' to 'darkhero' weapon bone
   ├─── MusicPlayer JNI requests playback of 'menu' theme
   ├─── Camera focuses, jumps to, and tracks 'obj3' coordinates
   │
   ▼
[UI View Overlay Creation]
   │
   └─── Instantiates Caver::MainMenuView overlay (allocating Play, Options, and Audio buttons)
   
================================[ setupApplication Completes ]================================

   │
   ▼
[setApplicationViewSize(W, H)]    <─── Sets GL viewport and configures GUI scaling parameters
   │
   ▼
[applicationDidBecomeActive()]   <─── Active state flag set; resumes game clocks and tickers
   │
   ▼
[Frame Loop: drawApplication()]
   │
   ▼
[CaverShell::Render()]            <─── Orchestrates draw pipeline via Caver::RenderingContext
   │
   ├─── 1. Background sky render (Background entity -> menu_back.POD / menu_back.pvr)
   ├─── 2. Terrain geometry render (obj2 -> GroundPolygon / GroundMesh using menu_back texture)
   ├─── 3. Character model render (darkhero entity -> char_evil.POD / hiro_stand.POD)
   ├─── 4. UI view overlay draw (MainMenuView -> game_common_atlas_2x.pvr button sprites & text)
   │
   ▼
[Host GL Buffer Swap]             <─── Graphics context swaps buffers, producing first visible frame
```

---

## 2. Vita Correlation & Checklist

The PlayStation Vita port serves as the ideal blueprint for designing the compatibility layer of the desktop loader.

### Systems Bypassed
*   **Google Play Games (Saves & Login)**: Bypasses all cloud sync methods (`loadSnapshot`, `saveSnapshot`, etc.). All progress is written directly to the host filesystem as `.gstate` profiles using standard file I/O.
*   **Ad Networks**: Patches out advertisement handlers (`Caver::OnlineController_Android::ShowInterstitialAd` replaced with `ret0`) to prevent overlays.
*   **In-App Purchases**: Store products and billing loops are skipped.

### Systems Mocked
*   **JNI VM & Env Tables**: Pre-allocates fake vtables mapping only the required VM methods (`GetEnv`, `GetJavaVM`) and Env methods (`FindClass`, `GetMethodID`, `CallVoidMethodV`, etc.) to host shims.
*   **Android NDK `AAssetManager`**: Intercepts `AAssetManager_open` to open files directly from the host filesystem. `AAsset_openFileDescriptor` returns raw file descriptors and uses a `skip_close` bypass to keep streaming files open.
*   **GLES Shader Translations**: translates specific mobile shader uniforms and attributes inside `glGetUniformLocation` and `glBindAttribLocation` to maintain compatibility with the host GPU drivers.

### Systems Fully Implemented
*   **Dynamic Relocation & Symbol Resolution**: A custom ELF loader parses `libswordigo.so`, applies relocations, and resolves dependencies to the host environment.
*   **SoLoud Audio Bridge**: Routes music play, pause, stop, loop, and volume callbacks into a fully functional local SoLoud audio wrapper.
*   **Touch Event Translation**: Polls hardware input devices (buttons/analog sticks) and translates them into simulated tap coordinates forwarded to `handleTouchEvent`.

---

## 3. Success Criteria: Next Immediate Blockers

If `setupApplication()` returned successfully right now, the next issues most likely to stop the game from showing the main menu are:

1.  **Missing/Invalid Host OpenGL Context & GLES 1.1 Bindings**:
    *   *Why*: The engine calls OpenGL ES 1.1 fixed-function functions (`glVertexPointer`, `glTexCoordPointer`, `glLoadMatrixf`, etc.) to render geometry. If the host runner does not have a bound graphics context (SDL/GLFW) or if GLES 1.1 functions are not successfully resolved to host OpenGL drivers, the application will segfault or crash.
2.  **PVRTC Texture Incompatibility on Desktop GPUs**:
    *   *Why*: *Swordigo*'s textures (like `game_common_atlas_2x.pvr` and `menu_back.pvr`) are compressed in the `PVRTC` format. Desktop GPUs (Nvidia, AMD, Intel) do not support PVRTC natively. Trying to upload them directly using `glTexImage2D` will fail, rendering the main menu completely black or crashing the graphics driver. The textures must be decompressed to `RGBA` format at load time.
3.  **Uninitialized Audio System**:
    *   *Why*: As soon as `setupApplication()` finishes loading `menu.scene`, the embedded Lua script runs and immediately invokes `MusicPlayer.PlayMusic("menu")`. If the host OpenAL device or SoLoud context is not initialized, this call will cause a null pointer dereference or crash.
4.  **Lua Bridge Argument Marshalling Errors**:
    *   *Why*: The embedded camera-orbit script depends on Lua-to-C++ calls like `Camera.FocusAtShape()`, `Scene.Find()`, and `ObjectLinkController.LinkToBone()`. If the argument marshalling (translating guest registers R0-R3 to C++ objects) is not fully implemented, the Lua script will error out, causing the camera to remain static at origin `(0,0,0)`.
