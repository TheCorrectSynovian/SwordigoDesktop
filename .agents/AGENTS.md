# Workspace Rules

## CLI Commands
- **NEVER run CLI commands using tools.** The user will run commands manually and paste output.
- Only provide commands as text for the user to copy-paste.

## General
- Preserve all existing comments and docstrings unrelated to changes.
- Do not use CMake for building — the project uses a Makefile.

## File Locations — CRITICAL: Read Carefully
- **ALWAYS edit source files in `~/SwordigoDesktop/` (ext4).** This is the compilation directory.
- **NEVER edit source files on the TVPG drive** (`/run/media/quantumcreeper/TVPG/Prenxy Packages/SwordigoDesktop/`). The `src/` there is a sync copy for git commits — do NOT edit it directly.
- The TVPG drive is for the git repo, reference material, and backups only.
- When syncing, copy FROM ext4 TO TVPG (not the other way around).

### Why source files live on ext4, NOT the NTFS TVPG drive
The TVPG drive is an external NTFS partition. NTFS on Linux (via ntfs3/ntfs-3g) has
known issues that break C/C++ builds:

1. **File locking**: `make` and `gcc` create temporary files rapidly. NTFS-3G's
   userspace FUSE layer occasionally fails to release locks in time, causing
   "Text file busy" or "Permission denied" errors mid-build.
2. **Case sensitivity**: NTFS is case-insensitive. Two headers like `GUI.h` and
   `gui.h` would collide. ext4 is fully case-sensitive.
3. **Symlink/permission quirks**: NTFS cannot store Unix permissions or symlinks
   natively. Build artifacts and `.so` files need `+x` which NTFS fakes via
   mount options, sometimes inconsistently.
4. **Performance**: NTFS on Linux is significantly slower for the many small
   random reads/writes that a C++ build produces.

**What happened**: We originally built directly on TVPG. Intermittent build
failures forced us to create `~/SwordigoDesktop/` on ext4 with just the `src/`
tree copied over. The TVPG drive keeps the full git repo, assets, engine
binaries, research notes, and reference material. Source edits happen ONLY on
ext4, then sync back to TVPG for git commits.

**Directory layout on TVPG**:
- `src/` — synced copy of ext4 source (for git commits, DO NOT EDIT directly)
- `trash/old_src_snapshots/` — old snapshots moved here for archival
- All other dirs (`engine/`, `assets/`, `research/`, etc.) — live on TVPG as normal

## User Capabilities
- **User has VS Code** with powerful search — can search strings in even 22MB decompiled .so files easily.
- When needing string/symbol lookups in large files, ASK the user to search in VS Code rather than attempting grep on massive files.
- User approves all plans automatically — only pause for CLI commands or VS Code searches.

## Project Philosophy
- **Playability over faithfulness** — we do NOT need accuracy or faithfulness to the original game.
- **Modularity** — the project should be modular, with clean separation of concerns.
- **Custom content gateway** — the architecture should support modding and custom content.
- **No more binary patching** — instead of fighting libswordigo.so with NOP patches, we build a proper runtime (libsre.so) that intercepts and replaces problematic functions.
- **Leverage reverse engineering** — use decompiled source, modloaders (SwKiwi, Swordigo Mini), and community knowledge to understand and rewrite game internals.

## Architecture
- **SRT** (Swordigo Runtime) — the overall desktop runtime architecture
- **SRE** (Swordigo Runtime Engine / libsre.so) — ARM64 guest-side library that hooks/replaces engine functions
- **Primary target** — v1.4.12 ARM64 (arm64-v8a)
