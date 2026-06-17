# Tasks — SDL2 → SDL3 Migration

## Phase 1: Headers & Build System
- [x] Update `CMakeLists.txt` — SDL3 packages, link targets
- [x] Update all `#include` paths to `SDL3/` format

## Phase 2: Window & Init
- [x] `display.cpp` — `SDL_Init` bool return, `SDL_CreateWindow` signature, `SDL_DestroySurface`
- [x] `launcher.cpp` — `SDL_Init` bool return, `SDL_CreateWindow` signature

## Phase 3: Events
- [x] `main.cpp` — All event constants renamed (`SDL_EVENT_*`), window events restructured
- [x] `display.cpp` — Event constants in poll loop
- [x] `launcher.cpp` — Event constants in poll loop

## Phase 4: Gamepad API
- [x] `main.cpp` — `SDL_GameController*` → `SDL_Gamepad*`, function renames, button constants
- [x] `input_config.cpp` — `SDL_CONTROLLER_BUTTON_*` → `SDL_GAMEPAD_BUTTON_*`
- [x] `input_config.h` — Comment updates

## Phase 5: Timers & Misc
- [x] `main.cpp` — `Uint32` → `Uint64` for `SDL_GetTicks()`, fullscreen flag

## Phase 6: Vulkan Backend
- [x] `vulkan_backend.cpp` — `SDL_Vulkan_GetDrawableSize` → `SDL_GetWindowSizeInPixels`, surface creation
- [x] `vulkan_backend.h` — Include path update

## Verification
- [ ] Install SDL3 + SDL3_image packages
- [ ] Clean rebuild (`rm -rf build && cmake -B build && cmake --build build`)
- [ ] Generate walkthrough
