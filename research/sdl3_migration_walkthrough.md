# Walkthrough — SDL2 → SDL3 Migration

## Summary

Successfully migrated the SwordigoDesktop runtime from **SDL 2.x** to **SDL 3.x** across **9 files**. The project compiles and links cleanly against SDL3 + SDL3_image on Fedora Linux.

---

## Files Modified

### Build System
| File | Changes |
|---|---|
| [CMakeLists.txt](file:///run/media/quantumcreeper/TVPG/Prenxy%20Packages/SwordigoDesktop/CMakeLists.txt) | `find_package(SDL2)` → `SDL3`, all 6 link targets updated, `SDL3_image` via CMake config |

### Headers
| File | Changes |
|---|---|
| [display.h](file:///run/media/quantumcreeper/TVPG/Prenxy%20Packages/SwordigoDesktop/src/platform/display.h) | `<SDL2/SDL.h>` → `<SDL3/SDL.h>` |
| [vulkan_backend.h](file:///run/media/quantumcreeper/TVPG/Prenxy%20Packages/SwordigoDesktop/src/platform/vulkan_backend.h) | `<SDL2/SDL.h>` + `<SDL2/SDL_vulkan.h>` → SDL3 |
| [input_config.h](file:///run/media/quantumcreeper/TVPG/Prenxy%20Packages/SwordigoDesktop/src/platform/input_config.h) | Comment updates (`SDL_GameControllerButton` → `SDL_GamepadButton`) |

### Source Files
| File | Key Changes |
|---|---|
| [display.cpp](file:///run/media/quantumcreeper/TVPG/Prenxy%20Packages/SwordigoDesktop/src/platform/display.cpp) | Init bool return, window creation (removed x,y + SHOWN), `SDL_GL_DestroyContext`, `SDL_DestroySurface`, event renames |
| [launcher.cpp](file:///run/media/quantumcreeper/TVPG/Prenxy%20Packages/SwordigoDesktop/src/platform/launcher.cpp) | Same + `SDL_ConvertSurface` (removed flags), `IMG_Init`/`IMG_Quit` removed (auto in SDL3) |
| [main.cpp](file:///run/media/quantumcreeper/TVPG/Prenxy%20Packages/SwordigoDesktop/src/main.cpp) | **~50 changes**: gamepad API (`SDL_Gamepad*`), gamepad enumeration (`SDL_GetGamepads`), 20+ event constants, timer types (`Uint64`), fullscreen toggle, text input (window param), finger events (`fingerID`), axis/button event members (`gaxis`/`gbutton`) |
| [input_config.cpp](file:///run/media/quantumcreeper/TVPG/Prenxy%20Packages/SwordigoDesktop/src/platform/input_config.cpp) | 8 gamepad button constants (`SDL_GAMEPAD_BUTTON_*`) |
| [vulkan_backend.cpp](file:///run/media/quantumcreeper/TVPG/Prenxy%20Packages/SwordigoDesktop/src/platform/vulkan_backend.cpp) | `SDL_Vulkan_GetInstanceExtensions` (new API), surface creation (allocator param), `SDL_GetWindowSizeInPixels` |

---

## Key API Changes Applied

| SDL2 | SDL3 | Files |
|---|---|---|
| `SDL_Init(X) < 0` | `!SDL_Init(X)` | display, launcher |
| `SDL_CreateWindow(title, x, y, w, h, flags)` | `SDL_CreateWindow(title, w, h, flags)` | display, launcher |
| `SDL_WINDOW_SHOWN` | Removed (default) | display, launcher |
| `SDL_FreeSurface()` | `SDL_DestroySurface()` | display, launcher |
| `SDL_GL_DeleteContext()` | `SDL_GL_DestroyContext()` | display, launcher |
| `SDL_ConvertSurfaceFormat(s, fmt, 0)` | `SDL_ConvertSurface(s, fmt)` | launcher |
| `IMG_Init()` / `IMG_Quit()` | Removed (automatic) | launcher |
| `SDL_QUIT` | `SDL_EVENT_QUIT` | all 3 event loops |
| `SDL_WINDOWEVENT` + sub-events | Top-level `SDL_EVENT_WINDOW_*` | main, display |
| `SDL_KEYDOWN` / `SDL_KEYUP` | `SDL_EVENT_KEY_DOWN` / `SDL_EVENT_KEY_UP` | main, display, launcher |
| `event.key.keysym.sym` | `event.key.key` | main, display, launcher |
| `SDL_GameController*` | `SDL_Gamepad*` | main |
| `SDL_INIT_GAMECONTROLLER` | `SDL_INIT_GAMEPAD` | main |
| `SDL_NumJoysticks()` loop | `SDL_GetGamepads()` array | main |
| `SDL_CONTROLLER_BUTTON_A/B/X/Y` | `SDL_GAMEPAD_BUTTON_SOUTH/EAST/WEST/NORTH` | main, input_config |
| `event.caxis` / `event.cbutton` | `event.gaxis` / `event.gbutton` | main |
| `event.cdevice` | `event.gdevice` | main |
| `event.tfinger.fingerId` | `event.tfinger.fingerID` | main |
| `SDL_StartTextInput()` | `SDL_StartTextInput(window)` | main |
| `Uint32 SDL_GetTicks()` | `Uint64 SDL_GetTicks()` | main |
| `SDL_WINDOW_FULLSCREEN_DESKTOP` | `SDL_WINDOW_FULLSCREEN` | main |
| `SDL_SetWindowFullscreen(w, flag)` | `SDL_SetWindowFullscreen(w, bool)` | main |
| `SDL_Vulkan_GetInstanceExtensions(w, &c, e)` | `SDL_Vulkan_GetInstanceExtensions(&c)` returns array | vulkan_backend |
| `SDL_Vulkan_CreateSurface(w, i, &s)` | `SDL_Vulkan_CreateSurface(w, i, NULL, &s)` | vulkan_backend |
| `SDL_Vulkan_GetDrawableSize()` | `SDL_GetWindowSizeInPixels()` | vulkan_backend |

---

## Verification

- ✅ **Compiles** — clean build with no warnings on Fedora (GCC 16.1.1)
- ✅ **Links** — against SDL3, SDL3_image, OpenGL, OpenAL, Unicorn, vorbisfile
- ⬜ **Runtime** — pending user testing (launch game, test gamepad, test fullscreen)
