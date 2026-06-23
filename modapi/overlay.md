# SRT Overlay — In-Game Inventory Editor

> **Header**: [`src/platform/srt_overlay.h`](file:///home/quantumcreeper/SwordigoDesktop/src/platform/srt_overlay.h)
> **Source**: [`src/platform/srt_overlay.cpp`](file:///home/quantumcreeper/SwordigoDesktop/src/platform/srt_overlay.cpp)
> **Toggle**: **F11** (currently WIP / disabled in v6)

The SRT Overlay is an in-game overlay system for editing save files, managing inventory, and browsing game items. It renders **after** PostFX using the host's OpenGL context and the existing `GuiRenderer` for text and drawing primitives.

> [!WARNING]
> The SRT Overlay is currently disabled in v6 (the F11 handler is commented out in `main.cpp`). The code and API are complete but the feature is work-in-progress.

---

## Architecture

```
Game renders → PostFX passes → Upscale to window
                                     ↓
                              SRT Overlay renders
                              (host-side GL, on top of everything)
                                     ↓
                              Final frame displayed
```

The overlay uses `GuiRenderer` (the same renderer used for the launcher GUI) for all text and button rendering, ensuring consistent styling.

---

## GameItem Struct

Represents a single game item definition from the vanilla catalog.

```cpp
struct GameItem {
    const char* id;     // Internal name (e.g. "brasssword")
    const char* name;   // Display name (e.g. "Brass Sword")
    int type;           // 1=consumable, 2=weapon, 3=armor, 5=quest
    int stat;           // Damage bonus (weapons) or armor % (armor)
};
```

### Item Type Values

| Value | Type | Description |
|-------|------|-------------|
| `0` | Spell | Magic abilities (bolt, bomb, hookshot, dimension rift) |
| `1` | Consumable | Potions, experience sacks, ankhs |
| `2` | Weapon | Swords with damage bonus |
| `3` | Armor | Armor with damage reduction percentage |
| `5` | Quest | Key items and Mageblade shards |

---

## Item Catalogs

All vanilla items organized into static arrays. These catalogs are defined in the header and used by the overlay to populate the item browser.

### ALL_WEAPONS (7 items)

| ID | Display Name | Type | Stat (Damage Bonus) |
|----|-------------|------|---------------------|
| `brasssword` | Brass Sword | 2 | 0 |
| `ironsword` | Iron Sword | 2 | 1 |
| `needle` | The Needle | 2 | 1 |
| `broadsword` | Broad Sword | 2 | 2 |
| `thorn` | The Thorn | 2 | 2 |
| `magicsword` | Magic Sword | 2 | 3 |
| `legendsword` | The Mageblade | 2 | 4 |

### ALL_ARMOR (2 items)

| ID | Display Name | Type | Stat (Armor %) |
|----|-------------|------|----------------|
| `platearmor` | Plate Armor | 3 | 50 |
| `magicarmor` | Magic Armor | 3 | 75 |

### ALL_TRINKETS (3 items)

| ID | Display Name | Type | Stat |
|----|-------------|------|------|
| `firetrinket` | Trinket of Fire | 1 | 0 |
| `icetrinket` | Trinket of Ice | 1 | 0 |
| `shadowtrinket` | Trinket of Shadow | 1 | 0 |

> [!NOTE]
> Trinkets have `type = 1` (same as consumable) in the data, but they function as passive equipment items in-game. They are slotted into `weapon_trinket`, `armor_trinket`, or `skill_trinket` equipment slots.

### ALL_SPELLS (4 items)

| ID | Display Name | Type | Stat |
|----|-------------|------|------|
| `bolt` | Magic Bolt | 0 | 0 |
| `bomb` | Magic Bomb | 0 | 0 |
| `hookshot` | Dragon's Grasp | 0 | 0 |
| `dimension` | Dimension Rift | 0 | 0 |

### ALL_CONSUMABLES (3 items)

| ID | Display Name | Type | Stat |
|----|-------------|------|------|
| `healingpotion` | Healing Potion | 1 | 0 |
| `experiencesack` | Sack of Experience | 1 | 0 |
| `ankh` | Ankh of Resurrection | 1 | 0 |

### ALL_QUEST_ITEMS (5 items)

| ID | Display Name | Type | Stat |
|----|-------------|------|------|
| `key_yellow` | A Key | 5 | 0 |
| `iselon_shard_1` | Mageblade Shard 1 | 5 | 0 |
| `iselon_shard_2` | Mageblade Shard 2 | 5 | 0 |
| `iselon_shard_3` | Mageblade Shard 3 | 5 | 0 |
| `iselon_shard_4` | Mageblade Shard 4 | 5 | 0 |

### Catalog Summary

| Catalog | Count | Purpose |
|---------|-------|---------|
| `ALL_WEAPONS` | 7 | Sword progression from Brass to Mageblade |
| `ALL_ARMOR` | 2 | Plate (50%) and Magic (75%) armor |
| `ALL_TRINKETS` | 3 | Fire, Ice, Shadow passive effects |
| `ALL_SPELLS` | 4 | Unlockable magic abilities |
| `ALL_CONSUMABLES` | 3 | Potions, XP sacks, revival ankhs |
| `ALL_QUEST_ITEMS` | 5 | Keys and Mageblade fragments |
| **Total** | **24** | All vanilla game items |

---

## InventoryItem Struct

Represents a single item in the player's inventory with a quantity count.

```cpp
struct InventoryItem {
    std::string name;   // Item ID (e.g. "brasssword", "healingpotion")
    int count;          // Quantity (1+ for most items)
};
```

---

## InventoryState Struct

Complete player state as read from / written to `.gplayer` save files. This is the primary data structure that the overlay edits.

### Character Stats

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `current_health` | `int` | `3` | Current HP |
| `current_mana` | `int` | `0` | Current MP |
| `coins` | `int` | `0` | Gold coins collected |
| `xp` | `int` | `0` | Experience points |
| `level` | `int` | `1` | Character level |
| `health_attr` | `int` | `0` | Health attribute points spent |
| `attack_attr` | `int` | `0` | Attack attribute points spent |
| `magic_attr` | `int` | `0` | Magic attribute points spent |

### Equipment Slots

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `equipped_weapon` | `std::string` | `""` | Currently equipped weapon ID |
| `equipped_armor` | `std::string` | `""` | Currently equipped armor ID |
| `weapon_trinket` | `std::string` | `""` | Trinket in weapon slot |
| `armor_trinket` | `std::string` | `""` | Trinket in armor slot |
| `skill_trinket` | `std::string` | `""` | Trinket in skill slot |
| `current_skill` | `std::string` | `""` | Currently selected spell |

### Inventory & Skills

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `items` | `std::vector<InventoryItem>` | `{}` | All items in inventory (with counts) |
| `skills` | `std::vector<std::string>` | `{}` | Unlocked spell IDs |

### Level / Location

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `current_level` | `std::string` | `""` | Current scene/level file name |
| `spawn_point` | `std::string` | `""` | Current spawn point within the level |

### Methods

#### Item Management

| Method | Signature | Description |
|--------|-----------|-------------|
| `has_item` | `bool has_item(const std::string& id) const` | Check if inventory contains at least one of the specified item |
| `add_item` | `void add_item(const std::string& id, int count = 1)` | Add items to inventory. If already present, increments count. Otherwise creates a new entry |
| `remove_item` | `void remove_item(const std::string& id)` | Remove all of the specified item from inventory |

#### Skill Management

| Method | Signature | Description |
|--------|-----------|-------------|
| `has_skill` | `bool has_skill(const std::string& id) const` | Check if a spell is unlocked |
| `add_skill` | `void add_skill(const std::string& id)` | Unlock a spell (no-op if already unlocked) |
| `remove_skill` | `void remove_skill(const std::string& id)` | Remove (lock) a spell |

---

## OverlayPanel Enum

Controls which sub-panel is displayed in the overlay.

```cpp
enum OverlayPanel {
    PANEL_NONE = 0,
    PANEL_INVENTORY,
    PANEL_STATS,
    PANEL_ITEMS_ALL,    // "Add all items" confirmation
};
```

| Value | Name | Description |
|-------|------|-------------|
| `0` | `PANEL_NONE` | No panel displayed |
| `1` | `PANEL_INVENTORY` | Main inventory view: shows items, equipment, skills |
| `2` | `PANEL_STATS` | Character stats: health, mana, level, XP, attributes |
| `3` | `PANEL_ITEMS_ALL` | "Add all items" confirmation dialog |

---

## SrtOverlay Class

### Public Methods

| Method | Signature | Description |
|--------|-----------|-------------|
| `SrtOverlay()` | Constructor | Initializes overlay state |
| `render` | `void render(GuiRenderer& gui, int win_w, int win_h, int mouse_x, int mouse_y, bool mouse_click, float dt)` | Main render function — called each frame from the render loop. Draws the complete overlay UI including sidebar, save selector, active panel, and status bar |
| `handle_key` | `bool handle_key(int scancode)` | Process keyboard input. Returns `true` if the key was consumed by the overlay |
| `toggle` | `void toggle()` | Toggle visibility on/off |
| `is_visible` | `bool is_visible() const` | Check if overlay is currently shown |

### Public Fields

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `visible` | `bool` | `false` | Whether the overlay is currently displayed |
| `active_panel` | `OverlayPanel` | `PANEL_INVENTORY` | Which panel is active |
| `save_dir` | `std::string` | `""` | Directory containing `.gplayer` save files |
| `active_save_file` | `std::string` | `""` | Currently selected save file path |
| `inventory` | `InventoryState` | (defaults) | The inventory data currently being edited |
| `inventory_dirty` | `bool` | `false` | `true` if changes have been made since last save |
| `status_message` | `std::string` | `""` | Current status bar message (e.g., "Saved!", "Item added") |
| `status_timer` | `float` | `0` | Time remaining for the status message (seconds) |
| `save_files` | `std::vector<std::string>` | `{}` | List of discovered save file paths |
| `selected_save` | `int` | `0` | Index of selected save in the list |
| `scroll_y` | `int` | `0` | Vertical scroll offset for long lists |

### Private Methods

| Method | Description |
|--------|-------------|
| `render_inventory_panel` | Draws the inventory panel: items list, equipment slots, skills |
| `render_stats_panel` | Draws the stats panel: health, mana, XP, level, attribute points |
| `render_sidebar` | Draws the left sidebar with panel navigation buttons |
| `render_save_selector` | Draws the save file selector bar at the top |
| `render_status_bar` | Draws the status message bar at the bottom |
| `draw_panel_bg` | Draws a semi-transparent panel background rectangle |
| `draw_item_button` | Draws a clickable item button with label, active state, and hover highlight |
| `is_hover` | Hit-test helper: checks if mouse is over a rectangle |
| `scan_saves` | Discover `.gplayer` files in `save_dir` |
| `load_save` | Load a save file into `inventory` |
| `write_save` | Write the current `inventory` back to a save file |

---

## gplayer Namespace

Functions for reading and writing Swordigo's `.gplayer` save files, which use a custom protobuf format.

```cpp
namespace gplayer {
    bool read(const std::string& path, InventoryState& out);
    bool write(const std::string& path, const InventoryState& inv,
               const std::vector<uint8_t>& original_data);
}
```

### gplayer::read

```cpp
bool gplayer::read(const std::string& path, InventoryState& out);
```

Parse a `.gplayer` file and populate an `InventoryState` struct.

| Parameter | Description |
|-----------|-------------|
| `path` | Absolute path to the `.gplayer` file |
| `out` | Output — populated with character stats, items, skills, equipment, and level info |

**Returns**: `true` on success, `false` if file doesn't exist or parse fails.

### gplayer::write

```cpp
bool gplayer::write(const std::string& path, const InventoryState& inv,
                    const std::vector<uint8_t>& original_data);
```

Serialize an `InventoryState` back to `.gplayer` format.

| Parameter | Description |
|-----------|-------------|
| `path` | Output file path |
| `inv` | The inventory state to write |
| `original_data` | The raw bytes of the original save file — used to preserve unknown protobuf fields that the overlay doesn't understand |

**Returns**: `true` on success.

> [!IMPORTANT]
> The `original_data` parameter is critical for save integrity. The `.gplayer` format contains many protobuf fields that `InventoryState` doesn't model (scene state, trigger flags, quest progress, etc.). By providing the original file data, `write()` can preserve these unknown fields while only modifying the fields that `InventoryState` tracks.

---

## Overlay UI Layout

```
┌──────────────────────────────────────────────────────────┐
│  Save: [slot1.gplayer ▼]  [Load]  [Save]  [Reload]      │  ← Save selector
├──────┬───────────────────────────────────────────────────┤
│      │                                                   │
│  INV │  ┌─ Inventory ──────────────────────────────────┐ │
│  ──  │  │ 🗡️ Brass Sword (×1)    [Remove]              │ │
│ STATS│  │ 🧪 Healing Potion (×3)  [Remove]             │ │
│  ──  │  │ 🔮 Magic Bolt           [Remove]             │ │
│ ALL  │  │                                               │ │
│      │  │ Equipment:                                    │ │
│      │  │   Weapon: [Brass Sword ▼]                     │ │
│      │  │   Armor:  [none ▼]                            │ │
│      │  │   Skill:  [Magic Bolt ▼]                      │ │
│      │  └───────────────────────────────────────────────┘ │
├──────┴───────────────────────────────────────────────────┤
│  ✓ Saved successfully                                    │  ← Status bar
└──────────────────────────────────────────────────────────┘
```

---

## Save File Locations

Save files are stored in the SRT data directory:

```
~/.local/share/swordigo-desktop/save/
├── slot1.gplayer        # Save slot 1
├── slot2.gplayer        # Save slot 2
├── slot3.gplayer        # Save slot 3
├── controls_arm64.ini   # Input configuration
└── ...
```

The overlay's `scan_saves()` method discovers all `.gplayer` files in this directory and populates the `save_files` list.

---

## Workflow Example

A typical editing session:

1. **Open overlay** — Press F11 (when enabled)
2. **Select save file** — Click on a save slot in the selector bar
3. **Load save** — The file is parsed and `inventory` is populated
4. **Edit inventory** — Add/remove items, change equipment, modify stats
5. **Save changes** — Write modified inventory back to the `.gplayer` file
6. **Close overlay** — Press F11 again

> [!CAUTION]
> Editing a save file while the game is actively using it may cause conflicts. It's safest to edit saves when the game is paused or at the main menu. The overlay does not coordinate with the game's own save system.
