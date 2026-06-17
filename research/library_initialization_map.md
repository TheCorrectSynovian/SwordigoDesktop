# Engine Library Initialization Map

This document maps the core subsystems and library managers inside `libswordigo.so` (version 1.4.6), identifying their singletons, constructors, accessors, initialization, and shutdown functions.

---

## 1. AudioSystem

The top-level manager for sound effects and OpenAL context setup.

*   **Constructor (Allocation)**: `002f662d` (`_ZN5Caver11AudioSystemC1Ev`)
*   **Singleton Accessor (`sharedSystem()`)**: `002f65bd` (`_ZN5Caver11AudioSystem12sharedSystemEv`)
*   **Singleton Mutator (`SetSharedSystem()`)**: `002f65f5` (`_ZN5Caver11AudioSystem15SetSharedSystemEPS0_`)
*   **Existence Checker (`hasSharedSystem()`)**: `002f65a9` (`_ZN5Caver11AudioSystem15hasSharedSystemEv`)
*   **Device Creation (`CreateDevice()`)**: `002f69c5` (`_ZN5Caver11AudioSystem12CreateDeviceEv`)
*   **Initialization (`Setup()`)**: `002f6ac1` (`_ZN5Caver11AudioSystem5SetupEv`)
    *   *Role*: Allocates sound sources, configures listener properties, and initializes sound effect buffers.
*   **Shutdown / Termination**: `002f6725` (`_ZN5Caver11AudioSystem8ShutdownEv`)
    *   *Role*: Destroys all playing sound channels, clears buffers, and releases the OpenAL device.
*   **Destructor**: `002f6d61` (`_ZN5Caver11AudioSystemD1Ev`)

---

## 2. TextureLibrary

Manages compressed textures, atlases, and GL texture allocations.

*   **Constructor (Allocation)**: `0031ef7d` (`_ZN5Caver14TextureLibraryC1Ev`)
*   **Singleton Accessor (`sharedLibrary()`)**: `0031eebd` (`_ZN5Caver14TextureLibrary13sharedLibraryEv`)
*   **Singleton Mutator (`SetSharedLibrary()`)**: `0031ef19` (`_ZN5Caver14TextureLibrary16SetSharedLibraryEPS0_`)
*   **Atlas Loading (`LoadTextureAtlasWithName()`)**: `0031f439` (`_ZN5Caver14TextureLibrary24LoadTextureAtlasWithNameERKSs`)
*   **Initialization / Clear (`Clear()`)**: `0031f5e1` (`_ZN5Caver14TextureLibrary5ClearEv`)
    *   *Role*: Unloads all loaded textures from memory and resets descriptors.
*   **Shutdown**: Handled by destructor and dynamic clearing when context changes.

---

## 3. FontLibrary

Loads bitmap fonts and parses characters.

*   **Singleton Accessor (`sharedLibrary()`)**: `0031aba9` (`_ZN5Caver11FontLibrary13sharedLibraryEv`)
*   **Singleton Mutator (`SetSharedLibrary()`)**: `0031abe1` (`_ZN5Caver11FontLibrary16SetSharedLibraryEPS0_`)
*   **Font Retrieval (`FontWithName()`)**: `0031ac39` (`_ZN5Caver11FontLibrary12FontWithNameERKSs`)
*   **Initialization / Clear (`Clear()`)**: `0031ac1d` (`_ZN5Caver11FontLibrary5ClearEv`)
    *   *Role*: Purges loaded fonts.

---

## 4. ModelLibrary

Loads 3D models (`.POD`) and animations.

*   **Singleton Accessor (`sharedLibrary()`)**: `002dc159` (`_ZN5Caver12ModelLibrary13sharedLibraryEv`)
*   **Singleton Mutator (`SetSharedLibrary()`)**: `002dc1ad` (`_ZN5Caver12ModelLibrary16SetSharedLibraryEPS0_`)
*   **Model Retrieval (`ModelForName()`)**: `002dc261` (`_ZN5Caver12ModelLibrary12ModelForNameERKSs`)
*   **Initialization / Clear (`Clear()`)**: `002dc219` (`_ZN5Caver12ModelLibrary5ClearEv`)
    *   *Role*: Frees model meshes and animation structures from heap memory.

---

## 5. MusicPlayer

Handles background music tracks and interfaces with JNI.

*   **JNI Constructor (`initMusicPlayer`)**: `002f58d1` (`Java_com_touchfoo_swordigo_MusicPlayer_initMusicPlayer`)
    *   *Role*: Allocates the native player and links it to the Java `MusicPlayer` wrapper.
*   **Native Constructor**: `002f751d` (`_ZN5Caver11MusicPlayerC1Ev`)
*   **Library Registry (`RegisterProgramLibrary`)**: `002f7d75` (`_ZN5Caver11MusicPlayer22RegisterProgramLibraryEPNS_12ProgramStateE`)
    *   *Role*: Exposes music player functions (`MusicPlayer.PlayMusic`, etc.) to the Lua script VM.
*   **JNI Bridges (Called by C++ to request Java-side audio operations)**:
    *   `MusicPlayerJNI::LoadFile(std::string const&)`: `002f5a05`
    *   `MusicPlayerJNI::Play()`: `002f5aa5`
    *   `MusicPlayerJNI::Pause()`: `002f5b1d`
    *   `MusicPlayerJNI::Stop()`: `002f5b45`
    *   `MusicPlayerJNI::SetLooping(bool)`: `002f5b6d`
    *   `MusicPlayerJNI::SetVolume(float)`: `002f5b99`
