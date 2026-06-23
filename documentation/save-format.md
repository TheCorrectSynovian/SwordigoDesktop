# Swordigo Save File Format

> `.gplayer` File Specification — Protobuf Wire Format

This document describes the binary format of Swordigo save files, the fields within
them, and the C++ API provided by Swordigo Desktop for reading and writing saves.

---

## Table of Contents

- [File Location](#file-location)
- [Format Overview](#format-overview)
- [Protobuf Wire Format Primer](#protobuf-wire-format-primer)
- [Top-Level: PlayerProfile](#top-level-playerprofile)
- [GameState Message](#gamestate-message)
- [CharacterState Message](#characterstate-message)
- [Nested Messages](#nested-messages)
  - [SaveLevel](#savelevel)
  - [SaveQuest](#savequest)
  - [SaveItem](#saveitem)
- [Save Editor API](#save-editor-api)
  - [Data Structures](#data-structures)
  - [Functions](#functions)
- [Protobuf Helpers](#protobuf-helpers)
- [Examples](#examples)
- [Source Files](#source-files)

---

## File Location

```
~/.local/share/swordigo-desktop/save/Documents/*.gplayer
```

Save files are stored in the runtime data directory under `save/Documents/`. Each
save slot produces one `.gplayer` file. The filename is typically a UUID or profile
identifier.

> [!IMPORTANT]
> Save files are **never** packaged in RPM/DEB distributions. They live exclusively
> in the user's home directory and are fully user-accessible (Minecraft-style layout).

---

## Format Overview

Swordigo save files use the **Protocol Buffers (protobuf) wire format** — the same
binary encoding used by Google's protobuf library, but Swordigo does **not** use
generated protobuf code. The engine has a hand-rolled serializer.

Key characteristics:

- **Binary format** — not human-readable
- **Tag-Length-Value (TLV)** encoding with varint tags
- **No `.proto` schema file** — field numbers were reverse-engineered
- **Forward-compatible** — unknown fields are preserved as raw bytes on re-serialization

```
┌─────────────────────────────────────┐
│           .gplayer file             │
├─────────────────────────────────────┤
│  PlayerProfile (top-level message)  │
│  ├── name (field 1)                 │
│  ├── experience_level (field 2)     │
│  ├── time_played (field 3)          │
│  ├── percent_completed (field 4)    │
│  ├── cheat_enabled (field 5)        │
│  ├── identifier (field 6)          │
│  ├── equipped_weapon_name (field ?) │
│  ├── equipped_armor_name (field ?)  │
│  ├── current_level_title (field ?)  │
│  └── game_state (field 13)          │
│       ├── character (field 1)       │
│       ├── current_level (field 3)   │
│       ├── current_spawn (field 4)   │
│       ├── levels[] (field 7)        │
│       ├── quests[] (field 8)        │
│       └── ...                       │
└─────────────────────────────────────┘
```

---

## Protobuf Wire Format Primer

Each field in the binary stream is encoded as a **tag** followed by the **value**.

### Tag Encoding

```
tag = (field_number << 3) | wire_type
```

The tag itself is a varint. The bottom 3 bits encode the wire type:

| Wire Type | ID | Encoding | Used For |
|-----------|----|----------|----------|
| Varint | 0 | Variable-length integer | `int32`, `int64`, `uint32`, `uint64`, `bool`, `enum` |
| 64-bit (I64) | 1 | Fixed 8 bytes (little-endian) | `double`, `fixed64`, `sfixed64` |
| Length-delimited (LEN) | 2 | Varint length + bytes | `string`, `bytes`, nested messages, packed repeated |
| 32-bit (I32) | 5 | Fixed 4 bytes (little-endian) | `float`, `fixed32`, `sfixed32` |

### Varint Encoding

Varints use 7 bits per byte, with the MSB as a continuation flag:

```
Value 300 = 0b100101100
Encoded:    0xAC 0x02
            ↓         ↓
            1_0101100  0_0000010
            (continue) (final)
            = 0101100 | 0000010_0000000
            = 44 + 256 = 300
```

---

## Top-Level: PlayerProfile

The `.gplayer` file is a single protobuf message with these known fields:

| Field # | Name | Wire Type | Protobuf Type | Description |
|---------|------|-----------|---------------|-------------|
| 1 | `name` | LEN (2) | `string` | Player display name |
| 2 | `experience_level` | Varint (0) | `int32` | Character experience level (shown in profile) |
| 3 | `time_played` | I64 (1) | `double` | Total playtime in seconds |
| 4 | `percent_completed` | I32 (5) | `float` | Completion percentage, range `[0.0, 1.0]` |
| 5 | `cheat_enabled` | Varint (0) | `bool` | Whether cheats are active |
| 6 | `identifier` | LEN (2) | `string` | Save profile UUID (matches `Mini.GetProfileID()`) |
| 13 | `game_state` | LEN (2) | nested message | The full game state (see below) |

Additional top-level fields observed but with uncertain field numbers:

| Name | Wire Type | Description |
|------|-----------|-------------|
| `equipped_weapon_name` | LEN (2) / `string` | Display name of equipped weapon (profile summary) |
| `equipped_armor_name` | LEN (2) / `string` | Display name of equipped armor (profile summary) |
| `current_level_title` | LEN (2) / `string` | Human-readable name of current level |

> [!NOTE]
> Unknown top-level fields are preserved as `raw_fields` (field number + raw bytes)
> during load and re-emitted during save. This ensures saves remain intact even when
> the editor doesn't understand every field.

---

## GameState Message

**Field 13** of the PlayerProfile is a nested message containing the full game state:

| Field # | Name | Wire Type | Type | Description |
|---------|------|-----------|------|-------------|
| 1 | `character` | LEN (2) | nested message | Character stats, inventory, equipment |
| 3 | `current_level` | LEN (2) | `string` | Internal level name (e.g., `"town_herohouse"`) |
| 4 | `current_spawn` | LEN (2) | `string` | Spawn point ID (e.g., `"spawn_default"`) |
| 5 | `current_map_node` | LEN (2) | `string` | World map node identifier |
| 7 | `levels` | LEN (2) | repeated nested | Per-level progress data |
| 8 | `quests` | LEN (2) | repeated nested | Quest completion status |

Additional GameState fields (preserved as raw data):

| Name | Description |
|------|-------------|
| `raw_properties` | Game properties / flags (StateProperties) |
| `raw_quest_texts` | Quest description text overrides |
| `raw_misc` | Catch-all for any other fields |
| `selected_menu_tab` | Last selected tab in the pause menu |
| `menu_button_flashing` | Whether the menu button pulsing indicator is active |
| `guide_enabled` | Whether the tutorial guide is enabled |

---

## CharacterState Message

**Field 1** of GameState. Contains the player's editable stats:

| Field # | Name | Wire Type | Type | Default | Description |
|---------|------|-----------|------|---------|-------------|
| 1 | `health` | Varint (0) | `int32` | `100` | Current health points |
| 2 | `mana` | Varint (0) | `int32` | — | Current mana points |
| 3 | `coins` | Varint (0) | `int32` | `0` | Gold coins held |
| 4 | `xp` | Varint (0) | `int32` | — | Experience points |
| 5 | `level` | Varint (0) | `int32` | `1` | Character level |
| 6 | `equipped_weapon` | LEN (2) | `string` | — | Internal weapon ID |
| 7 | `equipped_armor` | LEN (2) | `string` | — | Internal armor ID |
| 8 | `current_skill` | LEN (2) | `string` | — | Active magic skill ID |
| 9 | `weapon_trinket` | LEN (2) | `string` | — | Trinket in weapon slot |
| 10 | `armor_trinket` | LEN (2) | `string` | — | Trinket in armor slot |
| 11 | `skill_trinket` | LEN (2) | `string` | — | Trinket in skill slot |
| 12 | `health_attr` | Varint (0) | `int32` | — | Health attribute points spent |
| 13 | `attack_attr` | Varint (0) | `int32` | — | Attack attribute points spent |
| 14 | `magic_attr` | Varint (0) | `int32` | — | Magic attribute points spent |
| 15 | `items` | LEN (2) | repeated nested | — | Inventory items (name + count) |
| 16 | `skills` | LEN (2) | repeated `string` | — | Learned skill IDs |

### Known Weapon IDs

| ID | Name |
|----|------|
| `"sword_rusty"` | Rusty Sword |
| `"sword_iron"` | Iron Sword |
| `"sword_holy"` | Holy Sword |
| `"sword_fire"` | Fire Sword |
| `"sword_ice"` | Frost Sword |
| `"sword_dragon"` | Dragon Sword |

### Known Skill IDs

| ID | Name |
|----|------|
| `"magic_bolt"` | Magic Bolt |
| `"fire_burst"` | Fire Burst |
| `"frost_strike"` | Frost Strike |
| `"dragon_blast"` | Dragon Blast |

> [!TIP]
> To give a character all items and max stats, set `health` to a high value,
> `coins` to 9999, and populate the `items` and `skills` arrays with known IDs.
> The save editor UI exposes all of these fields.

---

## Nested Messages

### SaveLevel

Repeated in GameState field 7. Tracks per-level progress:

| Field | Name | Type | Description |
|-------|------|------|-------------|
| — | `name` | `string` | Internal level name |
| — | `visited` | `bool` | Whether the player has entered this level |
| — | `num_treasures` | `int32` | Total treasure chests in level |
| — | `treasures_found` | `int32` | Chests the player has opened |
| — | `flags` | repeated `string` | StateProperties set for this level |

### SaveQuest

Repeated in GameState field 8. Tracks quest status:

| Field | Name | Type | Description |
|-------|------|------|-------------|
| — | `name` | `string` | Quest identifier |
| — | `completed` | `bool` | Whether the quest is finished |

### SaveItem

Repeated in CharacterState field 15. Inventory entries:

| Field | Name | Type | Description |
|-------|------|------|-------------|
| — | `name` | `string` | Item identifier |
| — | `count` | `int32` | Stack count |

---

## Save Editor API

**Source**: `src/platform/save_editor.h`, `src/game/save_editor_logic.h`

The save editor provides a C++ API for loading, inspecting, modifying, and writing
save files.

### Data Structures

#### SaveFile (Top-Level)

```cpp
struct SaveFile {
    std::string filepath;               // Path to the .gplayer file

    // --- PlayerProfile fields ---
    std::string name;                   // Player name
    int         experience_level;       // Level shown in profile
    double      time_played;            // Seconds
    float       percent_completed;      // 0.0–1.0
    bool        cheat_enabled;
    std::string identifier;             // UUID
    std::string equipped_weapon_name;   // Display name
    std::string equipped_armor_name;    // Display name
    std::string current_level_title;    // Human-readable level name

    // --- Nested ---
    SaveGameState game_state;           // Full game state

    // --- Preservation ---
    std::vector<std::pair<uint32_t, std::string>> raw_fields;  // Unknown fields
    bool modified;                      // Dirty flag
};
```

#### SaveGameState

```cpp
struct SaveGameState {
    SaveCharacter               character;
    std::vector<SaveLevel>      levels;
    std::string                 current_level;       // e.g., "town_herohouse"
    std::string                 current_spawn;       // e.g., "spawn_default"
    std::string                 current_map_node;
    std::vector<SaveQuest>      quests;
    std::string                 selected_menu_tab;
    bool                        menu_button_flashing;
    bool                        guide_enabled;

    // Raw preservation
    std::string                 raw_properties;
    std::vector<std::string>    raw_quest_texts;
    std::string                 raw_misc;
};
```

#### SaveCharacter

```cpp
struct SaveCharacter {
    int         health;
    int         mana;
    int         coins;
    int         xp;
    int         level;
    std::string equipped_weapon;       // Internal ID
    std::string equipped_armor;        // Internal ID
    std::string current_skill;         // Active skill ID
    std::string weapon_trinket;
    std::string armor_trinket;
    std::string skill_trinket;
    int         health_attr;
    int         attack_attr;
    int         magic_attr;
    std::vector<SaveItem>       items;
    std::vector<std::string>    skills;
};
```

#### Supporting Structs

```cpp
struct SaveItem {
    std::string name;
    int count;
};

struct SaveLevel {
    std::string name;
    bool visited;
    int num_treasures;
    int treasures_found;
    std::vector<std::string> flags;    // StateProperties
};

struct SaveQuest {
    std::string name;
    bool completed;
};
```

### Functions

#### `save_load`

```cpp
bool save_load(const std::string& filepath, SaveFile& out);
```

Loads a `.gplayer` file and populates the `SaveFile` struct.

- **Returns**: `true` on success, `false` on file read or parse error.
- **Behavior**: Reads the entire file into memory, then walks the protobuf fields.
  Unknown fields are stored in `raw_fields` for lossless re-serialization.

#### `save_write`

```cpp
bool save_write(const std::string& filepath, const SaveFile& sf);
```

Writes a `SaveFile` struct back to a `.gplayer` file.

- **Returns**: `true` on success, `false` on write error.
- **Behavior**: Re-encodes all known fields and appends preserved `raw_fields`.
  The output is a valid protobuf binary.

#### `save_summary`

```cpp
std::string save_summary(const SaveFile& sf);
```

Returns a human-readable multi-line summary of the save file. Useful for UI display
and debugging.

Example output:
```
Player: Hero
Level: 12
Coins: 2450
Health: 180
Scene: forgotten_keep
Playtime: 14520s (4.0h)
Completion: 67.3%
```

#### `save_list_dir`

```cpp
std::vector<std::string> save_list_dir(const std::string& dir_path);
```

Lists all `.gplayer` files in the given directory.

- **Returns**: Vector of full file paths.
- **Typical usage**: `save_list_dir("~/.local/share/swordigo-desktop/save/Documents/")`

### Low-Level Protobuf Helpers

**Source**: `src/game/save_editor_logic.h`

These are also available for direct protobuf manipulation:

```cpp
// Decode a varint from data at position pos (advances pos)
uint64_t decode_varint(const uint8_t* data, size_t& pos, size_t max_pos);

// Encode a varint and append to buf
void encode_varint(std::vector<uint8_t>& buf, uint64_t val);

// Decode a length-delimited string from data (advances pos)
std::string decode_string(const uint8_t* data, size_t& pos, size_t max_pos);

// Encode a string field (field header + length + bytes)
void encode_string(std::vector<uint8_t>& buf, uint32_t field_num,
                   const std::string& str);

// Encode just a field header (field_num << 3 | wire_type)
void encode_field_header(std::vector<uint8_t>& buf, uint32_t num, uint32_t type);
```

---

## Examples

### Reading a Save File

```cpp
#include "platform/save_editor.h"
#include <iostream>

int main() {
    SaveFile sf;
    if (save_load("~/.local/share/swordigo-desktop/save/Documents/profile.gplayer", sf)) {
        std::cout << save_summary(sf) << std::endl;
        std::cout << "Coins: " << sf.game_state.character.coins << std::endl;
        std::cout << "Level: " << sf.game_state.current_level << std::endl;
    }
    return 0;
}
```

### Modifying and Saving

```cpp
SaveFile sf;
save_load("save.gplayer", sf);

// Give max coins
sf.game_state.character.coins = 9999;

// Add an item
sf.game_state.character.items.push_back({"potion_health", 10});

// Change level
sf.game_state.current_level = "florennum_city";
sf.game_state.current_spawn = "spawn_entrance";

save_write("save.gplayer", sf);
```

### Binary Inspection

To manually inspect a `.gplayer` file, use a hex editor and the varint decoding
rules above. Each field starts with a varint tag:

```
Hex dump (first bytes):
0A 04 48 65 72 6F 10 0C 19 ...
│  │  └──────────┘  │  │  │
│  │   "Hero"       │  │  └─ time_played (double, I64)
│  │                │  └─ tag 0x10 = field 2, varint → experience_level = 12
│  └─ length 4      │
└─ tag 0x0A = field 1, LEN → name
```

> [!WARNING]
> Always use the save editor API for modifications. Hand-editing protobuf binaries
> is error-prone — a single wrong varint length will corrupt the entire file.
> The API preserves unknown fields automatically.

---

## Source Files

| File | Description |
|------|-------------|
| `src/platform/save_editor.h` | `SaveFile`, `SaveGameState`, `SaveCharacter` structs + API declarations |
| `src/game/save_editor_logic.h` | `SwordigoSave` (simplified struct) + low-level protobuf helpers |
