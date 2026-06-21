# Workspace Rules

## CLI Commands
- **NEVER run CLI commands using tools.** The user will run commands manually and paste output.
- Only provide commands as text for the user to copy-paste.

## General
- Preserve all existing comments and docstrings unrelated to changes.
- Do not use CMake for building — the project uses a Makefile.

## File Locations
- **ALWAYS edit source files in `~/SwordigoDesktop/` (ext4).** This is the compilation directory.
- **NEVER edit source files on the TVPG drive** (`/run/media/quantumcreeper/TVPG/Prenxy Packages/SwordigoDesktop/`). The `src` directory there has been renamed to `.src` to avoid confusion.
- The TVPG drive is for the git repo, reference material, and backups only.
- When syncing, copy FROM ext4 TO TVPG (not the other way around).

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
