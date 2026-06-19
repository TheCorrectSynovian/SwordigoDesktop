#pragma once
#include <string>
#include <vector>
#include <map>

// ============================================================
// Swordigo Save Editor — parses .gplayer (PlayerProfile) files
// Uses our lightweight protobuf reader/writer.
// ============================================================

// Inventory item
struct SaveItem {
    std::string name;
    int count;
};

// Level progress
struct SaveLevel {
    std::string name;
    bool visited;
    int num_treasures;
    int treasures_found;
    std::vector<std::string> flags; // StateProperties
};

// Quest state
struct SaveQuest {
    std::string name;
    bool completed;
};

// Character state (the main editable data)
struct SaveCharacter {
    int health;
    int mana;
    int coins;
    int xp;
    int level;
    std::string equipped_weapon;
    std::string equipped_armor;
    std::string current_skill;
    std::string weapon_trinket;
    std::string armor_trinket;
    std::string skill_trinket;
    int health_attr;
    int attack_attr;
    int magic_attr;
    std::vector<SaveItem> items;
    std::vector<std::string> skills;
};

// GameState
struct SaveGameState {
    SaveCharacter character;
    std::vector<SaveLevel> levels;
    std::string current_level;
    std::string current_spawn;
    std::string current_map_node;
    std::vector<SaveQuest> quests;
    std::string selected_menu_tab;
    bool menu_button_flashing;
    bool guide_enabled;
    
    // Raw bytes for fields we don't edit (preserve on re-encode)
    std::string raw_properties;
    std::vector<std::string> raw_quest_texts;
    std::string raw_misc; // catch-all for unknown fields
};

// PlayerProfile (.gplayer file)
struct SaveFile {
    std::string filepath;           // Path to the .gplayer file
    
    // Top-level PlayerProfile fields
    std::string name;               // Player name
    int experience_level;           // Level shown in profile
    double time_played;             // Seconds
    float percent_completed;        // 0.0-1.0
    bool cheat_enabled;
    std::string identifier;         // UUID
    std::string equipped_weapon_name;
    std::string equipped_armor_name;
    std::string current_level_title;
    
    // The game state
    SaveGameState game_state;
    
    // Raw bytes for unknown top-level fields
    std::vector<std::pair<uint32_t, std::string>> raw_fields;
    
    // Dirty flag
    bool modified;
};

// ============================================================
// API
// ============================================================

// Load a .gplayer file. Returns true on success.
bool save_load(const std::string& filepath, SaveFile& out);

// Write a SaveFile back to disk. Returns true on success.
bool save_write(const std::string& filepath, const SaveFile& sf);

// Get human-readable summary string
std::string save_summary(const SaveFile& sf);

// List all .gplayer files in a directory
std::vector<std::string> save_list_dir(const std::string& dir_path);
