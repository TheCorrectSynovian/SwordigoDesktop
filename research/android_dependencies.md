# Android Dependencies - 2026-06-14 - Agent 1

## Native shared-library dependencies

`reference/lib/armeabi-v7a/libswordigo.so` declares these `DT_NEEDED` libraries:

- `libopenal-soft.so`
- `libz.so`
- `libGLESv1_CM.so`
- `libEGL.so`
- `libandroid.so`
- `liblog.so`
- `libstdc++.so`
- `libm.so`
- `libc.so`
- `libdl.so`

Evidence: `readelf -d reference/lib/armeabi-v7a/libswordigo.so`.

## Android NDK asset APIs

Imported by local native library:

- `AAssetManager_fromJava`
- `AAssetManager_open`
- `AAsset_openFileDescriptor`
- `AAsset_read`
- `AAsset_getLength`
- `AAsset_close`

Vita also implements `AAsset_seek` for its target library. Evidence: `docs/research/vita/vistaPort/swordigo-vita-master/loader/main.c:859`.

Minimum Linux layer: map `setAssetManager` to a fake manager object, map asset open/read/length/close/seek to files under `assets/resources`, and support file-descriptor access for APIs that call `AAsset_openFileDescriptor`.

## JNI APIs

Observed Vita fake `JNIEnv` entries include:

- Class/object: `FindClass`, `NewGlobalRef`, `DeleteGlobalRef`, `GetObjectClass`, `NewObjectV`
- Methods: `GetMethodID`, `GetStaticMethodID`, `CallObjectMethodV`, `CallBooleanMethodV`, `CallIntMethodV`, `CallLongMethodV`, `CallVoidMethodV`, `CallStaticObjectMethodV`, `CallStaticBooleanMethodV`, `CallStaticIntMethodV`, `CallStaticLongMethodV`, `CallStaticFloatMethodV`, `CallStaticVoidMethodV`
- Fields: `GetFieldID`, `GetBooleanField`, `GetIntField`, `GetFloatField`, `GetStaticFieldID`, `GetStaticObjectField`
- Strings: `NewStringUTF`, `GetStringUTFLength`, `GetStringUTFChars`, `ReleaseStringUTFChars`, `GetStringUTFRegion`
- Arrays: `GetArrayLength`, `GetObjectArrayElement`, `GetIntArrayElements`, `ReleaseIntArrayElements`, `SetIntArrayRegion`
- VM/native registration: `GetJavaVM`, `RegisterNatives`

Evidence: `docs/research/vita/vistaPort/swordigo-vita-master/loader/main.c:1703` through `docs/research/vita/vistaPort/swordigo-vita-master/loader/main.c:1752`.

## Graphics APIs

Local native imports include:

- EGL: `eglGetProcAddress`
- GLES 1.x/fixed function: `glMatrixMode`, `glLoadIdentity`, `glTexEnvi`, `glLightf`, `glLightfv`, `glMaterialfv`, `glColorPointer`, `glVertexPointer`, `glNormalPointer`
- GLES common rendering: `glBindTexture`, `glTexImage2D`, `glCompressedTexImage2D`, `glDrawArrays`, `glDrawElements`, `glViewport`, `glScissor`, `glBlendFunc`, `glGetString`, `glGetError`
- Buffer APIs: `glBindBuffer`, `glBufferData`, `glBufferSubData`, `glGenBuffers`, `glDeleteBuffers`

Evidence: `readelf -Ws reference/lib/armeabi-v7a/libswordigo.so`.

Minimum Linux layer: provide a current GLES-compatible context before `setupApplication` or at least before GL resource creation; route GLES calls to host GL/GLES or a translation layer. Vita uses vitaGL and `vglGetProcAddress` to resolve additional GL functions. Evidence: `docs/research/vita/vistaPort/swordigo-vita-master/loader/so_util.c:455`.

## Audio APIs

Local native imports include OpenAL functions and ALC device/context functions. Java-side music also uses Android `MediaPlayer`, but Vita replaces music calls with `init_soloud`, `load_music`, `play_music`, `set_music_loop`, `set_music_volume`, and `stop_music`.

Evidence: `reference/decompiled/decompClasses/com.touchfoo.swordigo/MusicPlayer.java:20`, `reference/decompiled/decompClasses/com.touchfoo.swordigo/MusicPlayer.java:27`, `docs/research/vita/vistaPort/swordigo-vita-master/loader/main.c:1501`, `docs/research/vita/vistaPort/swordigo-vita-master/loader/main.c:1526`.

Minimum Linux layer: provide OpenAL for effects and either implement the Java `MusicPlayer` JNI callback surface or directly replace the native calls as Vita does.

## Logging and libc/Bionic surface

Local native imports include Android logging through `liblog`; Vita implements `__android_log_print`, `__android_log_write`, and `__android_log_vprint` as log stubs. Evidence: `docs/research/vita/vistaPort/swordigo-vita-master/loader/main.c:127`, `docs/research/vita/vistaPort/swordigo-vita-master/loader/main.c:143`, `docs/research/vita/vistaPort/swordigo-vita-master/loader/main.c:159`.

The loader also maps many Bionic/libc/pthread/zlib symbols, including static mutex/cond wrappers for Android pthread initializer values. Evidence: `docs/research/vita/vistaPort/swordigo-vita-master/loader/main.c:883`, `docs/research/vita/vistaPort/swordigo-vita-master/loader/main.c:930`, `docs/research/vita/vistaPort/swordigo-vita-master/loader/main.c:1093`.
