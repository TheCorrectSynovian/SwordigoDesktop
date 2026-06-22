# Research Findings — ARM32 Gate Bug + ARM64 Freeze + JNI Gaps

## 🔴🔴 CRITICAL FINDING 0: Missing Double-Precision Math in ARM32

**This is most likely the root cause of the ARM32 gate/obstacle bug.**

ARM64 registers **15 double-precision math functions** that ARM32 does NOT:

| Function | ARM32 | ARM64 | Used By |
|----------|-------|-------|---------|
| `floor` | ❌ | ✅ | Lua timer comparisons |
| `ceil` | ❌ | ✅ | Lua math |
| `sqrt` | ❌ | ✅ | Distance checks |
| `fmod` | ❌ | ✅ | Timer cycling |
| `fabs` | ❌ | ✅ | Trigger distances |
| `log` | ❌ | ✅ | Lua math |
| `log10` | ❌ | ✅ | Lua math |
| `log2` | ❌ | ✅ | Lua math |
| `exp` | ❌ | ✅ | Lua math |
| `ldexp` | ❌ | ✅ | Lua math |
| `frexp` | ❌ | ✅ | Lua math |
| `modf` | ❌ | ✅ | Lua math |
| `asin` | ❌ | ✅ | Angle calculations |
| `atan` | ❌ | ✅ | Angle calculations |
| `atan2` | ❌ | ✅ | Direction calculations |

> [!CAUTION]
> Lua uses `double` for ALL numbers. When unregistered functions are called, the ARM32 bridge falls through to `bx lr` returning **garbage from R0/R1**. This causes gate timers, trigger distances, and state transitions to compute incorrect values — gates never detect their trigger conditions being met.

Also missing from ARM32: `NewByteArray` (returns NULL), `SetByteArrayRegion`, `nanosleep`, `sched_yield`, `usleep`, `pthread_join`, `pthread_detach`.

---

Swordigo Mini's `32patch.c` reveals that **32-bit and 64-bit handle Lua programs differently**:
- **64-bit**: Executes the `String` field (Lua source text)
- **32-bit**: Executes the `Bytes` field (precompiled Lua bytecode)

The bytecode compiled for one architecture is **NOT compatible** with another. SwMini has a specific patch to swap these fields on 32-bit so mods work the same way. 

> [!IMPORTANT]
> Our ARM32 port may be executing the wrong Lua representation! If gates/obstacles are controlled by Lua scripts, executing incompatible bytecode would cause them to **silently fail** — no error, just nothing happens.

### Files:
- [32patch.c](file:///run/media/quantumcreeper/TVPG/Prenxy Packages/SwordigoDesktop/reference/swordigo mini (a swordigo mod loader)/app/src/main/cpp/patches/32patch.c)

---

## 🔴 CRITICAL FINDING 2: Delta Time Clamping (Vita Port)

The Vita port clamps `deltaTime`:
```c
// Prevent player from falling out of bounds when transitioning between stages
if (deltaSecond > 0.1f)
    deltaSecond = 0.016666668f;
```

**Impact**: Without clamping, large delta spikes during scene transitions can:
- Break physics (player falls through floor)
- Break timer-based events (gates/spikes skip their activation window)
- Cause infinite loops in event processing (ARM64 freeze?)

### Files:
- [main.c:1889-1891](file:///run/media/quantumcreeper/TVPG/Prenxy Packages/SwordigoDesktop/reference/vita/vistaPort/swordigo-vita-master/loader/main.c#L1889-L1891)

---

## 🔴 CRITICAL FINDING 3: ProgramState Timer Mechanism (panic.c)

SwMini reveals the Lua coroutine timer system:

| Offset (32/64) | Field | Type |
|---|---|---|
| 0x0/0x0 | `lua_State *L` | pointer |
| 0x4/0x8 | thread indicator | pointer (NULL = not thread) |
| 0x10/0x20 | `sceneObject` | pointer |
| 0x24/0x48 | `isSuspended` | int (1=suspended) |
| 0x28/0x4c | `sleepTime` | float (countdown timer) |
| 0x2d/0x51 | active flag | bool |
| 0x2e/0x52 | `paused` | bool |
| 0x2f/0x53 | `completed` | bool |
| 0x30/0x54 | `speedScaling` | float |

**Timer mechanic**: When `isSuspended == 1`, each `Update(deltaTime)` decrements `sleepTime`. When `sleepTime < 0`, `lua_resume()` is called. This drives `Scene.Wait()`, `Cutscene.Wait()`, etc.

> [!WARNING]
> On ARM64, `ProgramState::Update` has the resume logic **inlined** (unlike ARM32 where it calls `Resume` separately). SwMini adds `branch_within_engine(0x4c164c, 0x4c1684)` to prevent timers from ticking **twice**. This could be relevant to our ARM64 freeze.

### Files:
- [panic.c](file:///run/media/quantumcreeper/TVPG/Prenxy Packages/SwordigoDesktop/reference/swordigo mini (a swordigo mod loader)/app/src/main/cpp/patches/panic.c)

---

## 🟡 JNI Bridge Gap Analysis

### Missing Bridges — HIGH RISK

| Method | Risk | Issue |
|--------|------|-------|
| `getStoreName()` | 🔴 HIGH | Returns NULL string ref — could crash C++ |
| `quitApplication()` | 🔴 HIGH | Quit button doesn't work |
| `getLongFromSP()` | 🔴 HIGH | Always returns 0 — timing prefs broken |
| `getInterstitialAdInterval()` | 🟡 MED | Returns 0 — could cause rapid polling |

### Properly Implemented

All age/consent, SharedPrefs (bool/int), snapshot, and GPGS bridges are correctly implemented.

---

## 🔵 Relevant Engine Components (from components.h)

Gates/obstacles likely use these components:
- `DoorControllerComponent` — Doors/gates
- `ElevatorControllerComponent` — Moving platforms
- `PressureTriggerComponent` — Pressure plates
- `ObjectLinkControllerComponent` — Switch→gate links
- `KeyframeAnimationComponent` — Gate open/close animation
- `ProgramComponent` — Lua script on objects

---

## 📋 Action Plan

### For ARM32 Gate Bug:
1. **Check if Lua bytecode vs source is the issue** — The 32-bit `.so` may be trying to execute precompiled bytecode. Does our emulator handle this correctly? The ProtoProgram structure at offsets above determines which gets executed.
2. **Add deltaTime clamping** — Clamp to 16.6ms if > 100ms, matching Vita port.
3. **Check timer/event timing** — Are Lua coroutine timers ticking properly?

### For ARM64 Freeze Bug:
1. **Add deltaTime clamping** — Same as above.
2. **Check ProgramState::Update double-tick** — The inlined resume logic might cause timers to tick twice, leading to rapid state changes → infinite loops.
3. **Re-enable pthread_create selectively** — The entity_setup linked list is empty because worker threads are discarded.

### Quick Fixes (JNI):
1. Add `getStoreName` → return empty string handle
2. Add `getLongFromSP` dispatch in CallStaticLongMethodV
3. Add `quitApplication` → call exit/SDL_Quit
4. Add `getInterstitialAdInterval` → return large value (999999)
