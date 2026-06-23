# Binary Selector — Instance Management

> **Scope**: This document describes how Swordigo Desktop discovers, hashes,
> classifies, and selects game binaries. It covers the `BinarySelector` class,
> the manifest/instance JSON formats, and the boot sequence.
>
> **Source of truth**: [`src/platform/binary_selector.h`](file:///home/quantumcreeper/SwordigoDesktop/src/platform/binary_selector.h),
> [`src/platform/binary_selector.cpp`](file:///home/quantumcreeper/SwordigoDesktop/src/platform/binary_selector.cpp)

---

## 1. Overview

The Binary Selector manages multiple game binary instances. A single install
can contain several versions of `libswordigo.so` — vanilla releases, RLSwordigo
mods, the 3D camera mod, and user-imported custom binaries. The selector:

1. Discovers binaries from `manifest.json` (fast path) or filesystem scan (fallback)
2. Computes SHA256 hashes for integrity verification
3. Matches hashes against a built-in database of known binaries
4. Presents a selection UI when multiple binaries are available
5. Persists user preferences and custom instances to JSON

---

## 2. Enumerations

### BinaryStatus

Tracks the verification state of a game binary.

```cpp
enum class BinaryStatus {
    TESTED,     // Known stable — hash matches a verified release
    TESTING,    // User is actively testing this binary
    UNKNOWN     // Unrecognised hash — could be a mod or unknown version
};
```

| Value | Display String | Meaning |
|-------|---------------|---------|
| `TESTED` | `"Stable"` | Hash is in the built-in `KNOWN_HASHES` table |
| `TESTING` | `"Testing"` | User has marked this binary for testing |
| `UNKNOWN` | `"Unknown"` | Hash not recognised — version inferred from directory name |

### BinaryArch

CPU architecture of the ARM binary.

```cpp
enum class BinaryArch {
    ARM32,      // armeabi-v7a — 32-bit ARM (Thumb-2)
    ARM64       // arm64-v8a  — 64-bit AArch64 (primary target)
};
```

| Value | Directory Name | Display String | Pointer Width |
|-------|---------------|----------------|---------------|
| `ARM32` | `armeabi-v7a` | `"ARM32"` | 32-bit |
| `ARM64` | `arm64-v8a` | `"ARM64"` | 64-bit |

---

## 3. BinaryInfo Struct

Complete description of a detected game binary. This is the core data structure
that flows through the entire selector system.

```cpp
struct BinaryInfo {
    // ── Identity ──
    std::string filename;       // Always "libswordigo.so"
    std::string filepath;       // Full relative path: "engine/v1.4.12/arm64-v8a/libswordigo.so"
    std::string sha256;         // SHA256 hex string (64 characters)
    std::string version;        // Extracted version: "1.4.12", "6.6-rl", "sw3d"
    std::string label;          // Display label: "v1.4.12 [ARM64] (Stable) [Latest]"
    std::string version_dir;    // Parent directory name: "v1.4.12", "rl-v6.1", "custom-MyMod"

    // ── Classification ──
    BinaryStatus status;        // TESTED / TESTING / UNKNOWN
    BinaryArch arch;            // ARM32 / ARM64
    size_t file_size;           // File size in bytes
    bool is_default;            // true if this is the default boot binary

    // ── Game Type ──
    std::string game_type;      // "Swordigo" or "RLSwordigo"
    std::string assets_dir;     // "assets" (vanilla) or "rl_assets" (RLSwordigo)
    std::string icon_path;      // Custom icon PNG/JPG (empty = use default for game_type)

    // ── Dependencies (mod binaries only) ──
    std::vector<std::string> dependencies;  // Companion .so filenames: {"libmini.so", "libGlossHook.so"}
    std::vector<std::string> dep_paths;     // Full paths to each dependency .so file
};
```

### Field Details

| # | Field | Type | Source | Example |
|---|-------|------|--------|---------|
| 1 | `filename` | `string` | Filesystem | `"libswordigo.so"` |
| 2 | `filepath` | `string` | Filesystem | `"engine/v1.4.12/arm64-v8a/libswordigo.so"` |
| 3 | `sha256` | `string` | Computed | `"08d49dd6f7f8639a..."` |
| 4 | `version` | `string` | Hash lookup or dir name | `"1.4.12"` |
| 5 | `label` | `string` | Computed | `"v1.4.12 [ARM64] (Stable) [Latest]"` |
| 6 | `version_dir` | `string` | Filesystem | `"v1.4.12"` |
| 7 | `status` | `BinaryStatus` | Hash lookup | `TESTED` |
| 8 | `arch` | `BinaryArch` | Directory name / ELF header | `ARM64` |
| 9 | `file_size` | `size_t` | Filesystem | `8523456` |
| 10 | `is_default` | `bool` | Comparison with `default_binary` | `true` |
| 11 | `game_type` | `string` | Directory prefix | `"Swordigo"` |
| 12 | `assets_dir` | `string` | Directory prefix | `"assets"` |
| 13 | `icon_path` | `string` | Manifest / user config | `""` (empty = default) |
| 14 | `dependencies` | `vector<string>` | Filesystem scan | `{"libmini.so"}` |
| 15 | `dep_paths` | `vector<string>` | Filesystem scan | `{"engine/rl-v6.1/armeabi-v7a/libmini.so"}` |

### Label Format

Labels are constructed with these patterns:

| Game Type | Pattern | Example |
|-----------|---------|---------|
| Vanilla | `v{version} [{arch}] ({status})` | `v1.4.12 [ARM64] (Stable)` |
| Vanilla latest | `v{version} [{arch}] ({status}) [Latest]` | `v1.4.12 [ARM64] (Stable) [Latest]` |
| RLSwordigo | `[RL] v{version} [{arch}] ({status})` | `[RL] v6.6-rl [ARM32] (Tested)` |
| 3D Mod | `[3D] [{arch}] ({status})` | `[3D] [ARM64] (Unknown)` |
| Custom | `[Custom] {name} [{arch}] ({status})` | `[Custom] MyMod [ARM64] (Unknown)` |
| Custom RL | `[RL] [Custom] {name} [{arch}] ({status})` | `[RL] [Custom] RLTest [ARM32] (Unknown)` |

---

## 4. BinarySelector Class

### Constructor

```cpp
BinarySelector::BinarySelector()
    : default_binary("engine/v1.4.12/armeabi-v7a/libswordigo.so") {}
```

The default binary is v1.4.12 ARM32 — the original primary target before the
ARM64 migration.

### Public Methods (17 total)

#### JSON Manifest System (Primary Path)

| Method | Signature | Description |
|--------|-----------|-------------|
| `load_manifest` | `void load_manifest(const string& manifest_path)` | Load system manifest (read-only, shipped with RPM/DEB). Resolves relative paths against manifest's parent directory. Skips entries where the file is missing. |
| `load_user_instances` | `void load_user_instances(const string& json_path)` | Load user-added custom instances from `~/.config/swordigo-desktop/instances.json`. Deduplicates against already-loaded system binaries. |
| `save_user_instances` | `void save_user_instances(const string& json_path) const` | Save only `custom-*` instances to the user config file. Creates parent directories if needed. |
| `generate_manifest` | `void generate_manifest(const string& engine_path, const string& output_path)` | Full scan + hash + write `manifest.json`. **Called by packaging scripts**, not at runtime. |

#### Legacy Scan (Fallback)

| Method | Signature | Description |
|--------|-----------|-------------|
| `scan_engine_directory` | `void scan_engine_directory(const string& engine_path)` | Scan `engine/` directory tree. Iterates version dirs → arch dirs → finds `libswordigo.so`. Computes SHA256, builds labels. |
| `scan_directory` | `void scan_directory(const string& dir_path)` | Entry point: if `engine/` exists, delegates to `scan_engine_directory`. Otherwise, performs legacy flat scan for `libswordigo*.so` files in project root. |

#### JSON Registry (Legacy)

| Method | Signature | Description |
|--------|-----------|-------------|
| `load_registry` | `void load_registry(const string& json_path)` | Load old `swordigo_binaries.json`. Extracts `default` field only. |
| `save_registry` | `void save_registry(const string& json_path)` | Save registry in old format (keyed by SHA256). |

#### Selection

| Method | Signature | Description |
|--------|-----------|-------------|
| `get_binaries` | `const vector<BinaryInfo>& get_binaries() const` | Returns all detected binaries. |
| `get_default` | `string get_default() const` | Returns the default binary filepath. |
| `set_default` | `void set_default(const string& filepath)` | Sets default, updates `is_default` flags on all entries. |
| `get_loaded_info` | `const BinaryInfo* get_loaded_info() const` | Returns info for the currently loaded binary. `nullptr` if nothing loaded. |
| `set_loaded` | `void set_loaded(const string& filepath)` | Records which binary was loaded by the boot sequence. |
| `add_custom_instance` | `bool add_custom_instance(const string& so_path, const string& name, const string& assets_dir)` | Import a custom `.so` file. Reads ELF header for arch detection, copies to `engine/custom-{name}/{arch}/`, hashes, and registers. |
| `remove_instance` | `void remove_instance(int index)` | Remove a binary from the in-memory list by index. |
| `should_show_selector` | `bool should_show_selector() const` | Returns `true` if more than one binary was found. |

#### Utilities

| Method | Signature | Description |
|--------|-----------|-------------|
| `compute_sha256` | `static string compute_sha256(const string& filepath)` | Compute SHA256 of any file. Uses a minimal built-in implementation (no OpenSSL dependency). |
| `arch_string` | `static const char* arch_string(BinaryArch arch)` | Returns `"ARM64"` or `"ARM32"`. |

### Public Members

| Member | Type | Description |
|--------|------|-------------|
| `selected_index` | `int` | Current selection in the boot GUI (0-indexed). |

### Private Members

| Member | Type | Description |
|--------|------|-------------|
| `binaries` | `vector<BinaryInfo>` | All detected binary instances. |
| `default_binary` | `string` | Filepath of the default binary. |
| `loaded_binary` | `string` | Filepath of the currently loaded binary. |
| `KNOWN_HASHES` | `static map<string, pair<string, BinaryStatus>>` | Built-in hash → (version, status) lookup. |

### Private Methods

| Method | Description |
|--------|-------------|
| `scan_version_dir` | Scan a single version directory for `armeabi-v7a/` and `arm64-v8a/` subdirs. |
| `scan_arch_dir` | Scan a single architecture directory. Finds `libswordigo.so`, detects game type and dependencies, hashes, builds label. |
| `parse_instance_json` | Deserialize a JSON object block into `BinaryInfo`. |
| `instance_to_json` | Serialize a `BinaryInfo` to a JSON object string. |

---

## 5. Sort Order

When multiple binaries are found, they are sorted with this priority:

1. **Default binary first** (`is_default == true` sorts first)
2. **Status** (TESTED > TESTING > UNKNOWN — lower enum value first)
3. **Architecture within same version** (ARM32 before ARM64)
4. **Version** (newer first — lexicographic descending on `version_dir`)

---

## 6. Known Binary Hashes

Hardcoded fallback hashes for when no manifest is available:

| SHA256 (truncated) | Version | Status | Arch |
|--------------------|---------|--------|------|
| `cee15dd273074626...` | 1.4.6 | TESTED | ARM32 |
| `08d49dd6f7f8639a...` | 1.4.12 | TESTED | ARM32 |
| `a7c00ff6f3ed0d5b...` | 6.6-rl | TESTED | ARM32 |

> [!NOTE]
> These hashes only cover ARM32 binaries. ARM64 binaries are not yet in the
> hardcoded table — they rely on the manifest system or directory-name inference.

---

## 7. manifest.json Format

The manifest is generated by `generate_manifest()` during packaging and loaded
at boot time via `load_manifest()`.

```json
{
  "default": "engine/v1.4.12/arm64-v8a/libswordigo.so",
  "instances": [
    {
      "name": "v1.4.12 [ARM64] (Stable) [Latest]",
      "filename": "libswordigo.so",
      "filepath": "engine/v1.4.12/arm64-v8a/libswordigo.so",
      "sha256": "08d49dd6f7f8639a4c59f290ff2bb79254accf710f530bf53c2fce1659191c9e",
      "version": "1.4.12",
      "version_dir": "v1.4.12",
      "arch": "ARM64",
      "file_size": 8523456,
      "is_default": true,
      "status": "tested",
      "game_type": "Swordigo",
      "assets_dir": "assets",
      "icon_path": "",
      "dependencies": [],
      "dep_paths": []
    },
    {
      "name": "[RL] v6.6-rl [ARM32] (Tested)",
      "filename": "libswordigo.so",
      "filepath": "engine/rl-v6.1/armeabi-v7a/libswordigo.so",
      "sha256": "a7c00ff6f3ed0d5b3221158d6e214bba03288c1e6782be3dc2c736ae80eb19df",
      "version": "6.6-rl",
      "version_dir": "rl-v6.1",
      "arch": "ARM32",
      "file_size": 7245312,
      "is_default": false,
      "status": "tested",
      "game_type": "RLSwordigo",
      "assets_dir": "rl_assets",
      "icon_path": "",
      "dependencies": ["libmini.so", "libGlossHook.so"],
      "dep_paths": [
        "engine/rl-v6.1/armeabi-v7a/libmini.so",
        "engine/rl-v6.1/armeabi-v7a/libGlossHook.so"
      ]
    }
  ]
}
```

### Path Resolution in Manifests

- Paths in `manifest.json` are **relative** to the manifest's grandparent
  directory (i.e., the data root, since the manifest lives at
  `<data_dir>/engine/manifest.json`).
- `load_manifest()` resolves these against the manifest base directory automatically.
- Absolute paths (starting with `/`) are used as-is.

---

## 8. instances.json Format

User-added custom instances are stored separately from the system manifest:

**Location**: `~/.config/swordigo-desktop/instances.json`

```json
{
  "instances": [
    {
      "name": "[Custom] MyMod [ARM64] (Unknown)",
      "filename": "libswordigo.so",
      "filepath": "/home/user/.local/share/swordigo-desktop/engine/custom-MyMod/arm64-v8a/libswordigo.so",
      "sha256": "deadbeef...",
      "version": "MyMod",
      "version_dir": "custom-MyMod",
      "arch": "ARM64",
      "file_size": 9000000,
      "is_default": false,
      "status": "unknown",
      "game_type": "Swordigo",
      "assets_dir": "assets",
      "icon_path": "",
      "dependencies": [],
      "dep_paths": []
    }
  ]
}
```

> [!IMPORTANT]
> Only `custom-*` instances are written to `instances.json`. System-provided
> binaries always come from `manifest.json` and are never duplicated.

---

## 9. Custom Binary Import

The `add_custom_instance()` method handles importing a user-provided `.so` file:

```
Input: /tmp/my_modded_swordigo.so, name="MyMod", assets_dir="assets"

1. Validate file exists and is a regular file
2. Read ELF header (first 5 bytes):
   - Verify magic: 0x7F 'E' 'L' 'F'
   - Read e_ident[EI_CLASS]:
     - 1 → ARM32 → arch_dir = "armeabi-v7a"
     - 2 → ARM64 → arch_dir = "arm64-v8a"
3. Create directory: engine/custom-MyMod/arm64-v8a/
4. Copy .so → engine/custom-MyMod/arm64-v8a/libswordigo.so
5. Compute SHA256 of the copied file
6. Build BinaryInfo with game_type from assets_dir
7. Look up hash in KNOWN_HASHES for status
8. Build display label
9. Add to binaries list
```

---

## 10. Boot Sequence

The following sequence describes how the binary selector integrates into the
Swordigo Desktop startup:

```
┌───────────────────────────────────────────┐
│ 1. Parse command-line arguments           │
│    --lib <path>     Override binary path   │
│    --assets <dir>   Override assets dir    │
│    --version <ver>  Select by version dir  │
│    --vulkan         Use Vulkan backend     │
├───────────────────────────────────────────┤
│ 2. Ensure user data (first-run setup)     │
│    ensure_user_data()                     │
├───────────────────────────────────────────┤
│ 3. Load binary manifest                   │
│    load_manifest(data/engine/manifest.json│)
│    load_user_instances(~/.config/...)     │
│    OR fallback: scan_directory()          │
├───────────────────────────────────────────┤
│ 4. If --lib was specified:                │
│    → Use that binary directly             │
│    If --version was specified:            │
│    → Find matching version_dir            │
│    Else if multiple binaries found:       │
│    → Show launcher UI for selection       │
│    Else:                                  │
│    → Use default binary                   │
├───────────────────────────────────────────┤
│ 5. Set g_lib_name and g_assets_dir        │
│    Detect architecture from path          │
│    (arm64-v8a → g_is_arm64 = true)        │
├───────────────────────────────────────────┤
│ 6. Init display (SDL3 + OpenGL/Vulkan)    │
│    Init infrastructure (init_all())       │
├───────────────────────────────────────────┤
│ 7. Load and boot the selected binary      │
│    ARM32 → load_and_boot()                │
│    ARM64 → load_and_boot_arm64()          │
├───────────────────────────────────────────┤
│ 8. Enter game loop                        │
└───────────────────────────────────────────┘
```

### Architecture Detection

The architecture is determined by examining the binary path:

```cpp
// main.cpp — after binary selection
g_is_arm64 = (g_lib_name.find("arm64") != std::string::npos);
```

This gates the entire execution path:
- **ARM32**: Uses `ElfLoader`, `Emulator`, `JniBridge` (32-bit Unicorn ARM mode)
- **ARM64**: Uses `ElfLoaderArm64`, `EmulatorArm64`, `JniBridge64` (64-bit Unicorn AArch64 mode)

### SRE Compatibility Check

The Swordigo Runtime Engine (`libsre.so`) contains hardcoded function offsets
that are **only valid for v1.4.12 ARM64**. The boot sequence checks:

```cpp
bool sre_compatible = (g_lib_name.find("arm64") != std::string::npos &&
                       (g_lib_name.find("v1.4.12") != std::string::npos ||
                        g_lib_name.find("rl-") != std::string::npos));
```

If the binary is not SRE-compatible, the engine runs without hook support. The
STXR patcher and generic fixes still apply (they are version-independent).

---

## 11. Game Type Classification Logic

Game type is determined from the version directory name prefix:

```cpp
bool is_rl      = (version_dir.find("rl-") == 0);     // "rl-v6.1" → RLSwordigo
bool is_vanilla  = (version_dir[0] == 'v');             // "v1.4.12" → Swordigo
bool is_mod      = !is_vanilla;                          // Any non-vanilla

if (is_rl) {
    game_type  = "RLSwordigo";
    assets_dir = "rl_assets";
} else {
    game_type  = "Swordigo";
    assets_dir = "assets";
}
```

### Dependency Detection

Mod directories (`is_mod == true`) are scanned for companion `.so` files.
Any `.so` file in the architecture directory that isn't `libswordigo.so` is
recorded as a dependency:

```
rl-v6.1/armeabi-v7a/
├── libswordigo.so       ← main binary (not a dependency)
├── libmini.so           ← dependency
└── libGlossHook.so      ← dependency
```

> [!TIP]
> Vanilla directories (`v*`) are NOT scanned for dependencies — this
> optimisation avoids unnecessary filesystem operations for the common case.
