#include "platform/save_editor.h"
#include "platform/protobuf_reader.h"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <sstream>
#include <algorithm>

namespace fs = std::filesystem;

// ============================================================
// Protobuf field numbers — from FileRift's block_formats.py
// ============================================================

// PlayerProfile fields
namespace PP {
    const uint32_t Name = 1;
    const uint32_t ExperienceLevel = 2;
    const uint32_t TimePlayed = 3;
    const uint32_t GameState = 4;
    const uint32_t EquippedWeaponName = 5;
    const uint32_t EquippedArmorName = 6;
    const uint32_t WeaponTrinketName = 7;
    const uint32_t ArmorTrinketName = 8;
    const uint32_t CurrentLevelTitle = 9;
    const uint32_t LastPlayedTime = 10;
    const uint32_t PercentCompleted = 11;
    const uint32_t Counter = 12;
    const uint32_t CheatEnabled = 13;
    const uint32_t Identifier = 14;
}

// GameState fields
namespace GS {
    const uint32_t CharacterState = 1;
    const uint32_t LevelState = 2;
    const uint32_t CurrentLevel = 3;
    const uint32_t CurrentSpawnPoint = 4;
    const uint32_t CurrentMapNodeName = 5;
    const uint32_t QuestState = 7;
    const uint32_t Properties = 8;
    const uint32_t SelectedMenuTab = 9;
    const uint32_t CarriedObjectTemplate = 10;
    const uint32_t CarriedObjectIdentifier = 11;
    const uint32_t QuestText = 12;
    const uint32_t PreviousPortalLevel = 13;
    const uint32_t MenuButtonFlashing = 14;
    const uint32_t SkillToggleButtonFlashing = 15;
    const uint32_t FlashingItemName = 16;
    const uint32_t FlashingSkillName = 17;
    const uint32_t GuideEnabled = 18;
    const uint32_t GuideToggled = 19;
    const uint32_t CoinDoublerEnabled = 20;
    const uint32_t CoinDoublerToggled = 21;
}

// CharacterState fields
namespace CS {
    const uint32_t CurrentHealth = 2;
    const uint32_t CurrentMana = 4;
    const uint32_t CurrentCoins = 5;
    const uint32_t ExperiencePoints = 6;
    const uint32_t ExperienceLevel = 7;
    const uint32_t Item = 11;
    const uint32_t EquippedWeapon = 12;
    const uint32_t EquippedArmor = 13;
    const uint32_t Skill = 15;
    const uint32_t CurrentSkill = 16;
    const uint32_t WeaponTrinket = 17;
    const uint32_t ArmorTrinket = 18;
    const uint32_t SkillTrinket = 19;
    const uint32_t HealthAttribute = 20;
    const uint32_t AttackAttribute = 21;
    const uint32_t MagicAttribute = 22;
}

// Item fields
namespace IT {
    const uint32_t Name = 1;
    const uint32_t Count = 2;
}

// LevelState fields
namespace LS {
    const uint32_t LevelName = 1;
    const uint32_t Visited = 2;
    const uint32_t Properties = 3;
    const uint32_t NumTreasures = 4;
    const uint32_t TreasuresFound = 5;
}

// QuestState fields
namespace QS {
    const uint32_t QuestName = 1;
    const uint32_t Completed = 2;
}

// StateProperties fields
namespace SP {
    const uint32_t Flag = 1;
}

// ============================================================
// Decoders
// ============================================================

static SaveItem decode_item(const std::string& data) {
    proto::Reader r(data);
    SaveItem item = {"", 0};
    proto::Field f;
    while (r.read_field(f)) {
        if (f.field_number == IT::Name) item.name = f.as_string();
        else if (f.field_number == IT::Count) item.count = (int)f.as_int();
    }
    return item;
}

static std::vector<std::string> decode_state_properties(const std::string& data) {
    proto::Reader r(data);
    std::vector<std::string> flags;
    proto::Field f;
    while (r.read_field(f)) {
        if (f.field_number == SP::Flag) flags.push_back(f.as_string());
    }
    return flags;
}

static SaveLevel decode_level(const std::string& data) {
    proto::Reader r(data);
    SaveLevel level = {"", false, 0, 0, {}};
    proto::Field f;
    while (r.read_field(f)) {
        switch (f.field_number) {
            case LS::LevelName: level.name = f.as_string(); break;
            case LS::Visited: level.visited = f.as_bool(); break;
            case LS::Properties: level.flags = decode_state_properties(f.as_string()); break;
            case LS::NumTreasures: level.num_treasures = (int)f.as_int(); break;
            case LS::TreasuresFound: level.treasures_found = (int)f.as_int(); break;
        }
    }
    return level;
}

static SaveQuest decode_quest(const std::string& data) {
    proto::Reader r(data);
    SaveQuest quest = {"", false};
    proto::Field f;
    while (r.read_field(f)) {
        if (f.field_number == QS::QuestName) quest.name = f.as_string();
        else if (f.field_number == QS::Completed) quest.completed = f.as_bool();
    }
    return quest;
}

static SaveCharacter decode_character(const std::string& data) {
    proto::Reader r(data);
    SaveCharacter ch = {};
    proto::Field f;
    while (r.read_field(f)) {
        switch (f.field_number) {
            case CS::CurrentHealth: ch.health = (int)f.as_int(); break;
            case CS::CurrentMana: ch.mana = (int)f.as_int(); break;
            case CS::CurrentCoins: ch.coins = (int)f.as_int(); break;
            case CS::ExperiencePoints: ch.xp = (int)f.as_int(); break;
            case CS::ExperienceLevel: ch.level = (int)f.as_int(); break;
            case CS::Item: ch.items.push_back(decode_item(f.as_string())); break;
            case CS::EquippedWeapon: ch.equipped_weapon = f.as_string(); break;
            case CS::EquippedArmor: ch.equipped_armor = f.as_string(); break;
            case CS::Skill: ch.skills.push_back(f.as_string()); break;
            case CS::CurrentSkill: ch.current_skill = f.as_string(); break;
            case CS::WeaponTrinket: ch.weapon_trinket = f.as_string(); break;
            case CS::ArmorTrinket: ch.armor_trinket = f.as_string(); break;
            case CS::SkillTrinket: ch.skill_trinket = f.as_string(); break;
            case CS::HealthAttribute: ch.health_attr = (int)f.as_int(); break;
            case CS::AttackAttribute: ch.attack_attr = (int)f.as_int(); break;
            case CS::MagicAttribute: ch.magic_attr = (int)f.as_int(); break;
        }
    }
    return ch;
}

static SaveGameState decode_game_state(const std::string& data) {
    proto::Reader r(data);
    SaveGameState gs = {};
    proto::Field f;
    while (r.read_field(f)) {
        switch (f.field_number) {
            case GS::CharacterState: gs.character = decode_character(f.as_string()); break;
            case GS::LevelState: gs.levels.push_back(decode_level(f.as_string())); break;
            case GS::CurrentLevel: gs.current_level = f.as_string(); break;
            case GS::CurrentSpawnPoint: gs.current_spawn = f.as_string(); break;
            case GS::CurrentMapNodeName: gs.current_map_node = f.as_string(); break;
            case GS::QuestState: gs.quests.push_back(decode_quest(f.as_string())); break;
            case GS::Properties: gs.raw_properties = f.as_string(); break;
            case GS::SelectedMenuTab: gs.selected_menu_tab = f.as_string(); break;
            case GS::QuestText: gs.raw_quest_texts.push_back(f.as_string()); break;
            case GS::MenuButtonFlashing: gs.menu_button_flashing = f.as_bool(); break;
            case GS::GuideEnabled: gs.guide_enabled = f.as_bool(); break;
            default: {
                // Preserve unknown fields as raw
                proto::Writer tmp;
                tmp.write_field(f);
                gs.raw_misc += tmp.to_string();
                break;
            }
        }
    }
    return gs;
}

// ============================================================
// Encoders
// ============================================================

static std::string encode_item(const SaveItem& item) {
    proto::Writer w;
    w.write_string_field(IT::Name, item.name);
    if (item.count != 0) w.write_varint_field(IT::Count, item.count);
    return w.to_string();
}

static std::string encode_state_properties(const std::vector<std::string>& flags) {
    proto::Writer w;
    for (const auto& flag : flags) {
        w.write_string_field(SP::Flag, flag);
    }
    return w.to_string();
}

static std::string encode_level(const SaveLevel& level) {
    proto::Writer w;
    w.write_string_field(LS::LevelName, level.name);
    if (level.visited) w.write_varint_field(LS::Visited, 1);
    if (!level.flags.empty()) {
        w.write_bytes_field(LS::Properties, encode_state_properties(level.flags));
    }
    w.write_varint_field(LS::NumTreasures, level.num_treasures);
    w.write_varint_field(LS::TreasuresFound, level.treasures_found);
    return w.to_string();
}

static std::string encode_quest(const SaveQuest& quest) {
    proto::Writer w;
    w.write_string_field(QS::QuestName, quest.name);
    if (quest.completed) w.write_varint_field(QS::Completed, 1);
    return w.to_string();
}

static std::string encode_character(const SaveCharacter& ch) {
    proto::Writer w;
    if (ch.health != 0) w.write_varint_field(CS::CurrentHealth, ch.health);
    if (ch.mana != 0) w.write_varint_field(CS::CurrentMana, ch.mana);
    if (ch.coins != 0) w.write_varint_field(CS::CurrentCoins, ch.coins);
    if (ch.xp != 0) w.write_varint_field(CS::ExperiencePoints, ch.xp);
    if (ch.level != 0) w.write_varint_field(CS::ExperienceLevel, ch.level);
    for (const auto& item : ch.items) {
        w.write_bytes_field(CS::Item, encode_item(item));
    }
    if (!ch.equipped_weapon.empty()) w.write_string_field(CS::EquippedWeapon, ch.equipped_weapon);
    if (!ch.equipped_armor.empty()) w.write_string_field(CS::EquippedArmor, ch.equipped_armor);
    for (const auto& skill : ch.skills) {
        w.write_string_field(CS::Skill, skill);
    }
    if (!ch.current_skill.empty()) w.write_string_field(CS::CurrentSkill, ch.current_skill);
    if (!ch.weapon_trinket.empty()) w.write_string_field(CS::WeaponTrinket, ch.weapon_trinket);
    if (!ch.armor_trinket.empty()) w.write_string_field(CS::ArmorTrinket, ch.armor_trinket);
    if (!ch.skill_trinket.empty()) w.write_string_field(CS::SkillTrinket, ch.skill_trinket);
    if (ch.health_attr != 0) w.write_varint_field(CS::HealthAttribute, ch.health_attr);
    if (ch.attack_attr != 0) w.write_varint_field(CS::AttackAttribute, ch.attack_attr);
    if (ch.magic_attr != 0) w.write_varint_field(CS::MagicAttribute, ch.magic_attr);
    return w.to_string();
}

static std::string encode_game_state(const SaveGameState& gs) {
    proto::Writer w;
    w.write_bytes_field(GS::CharacterState, encode_character(gs.character));
    for (const auto& level : gs.levels) {
        w.write_bytes_field(GS::LevelState, encode_level(level));
    }
    if (!gs.current_level.empty()) w.write_string_field(GS::CurrentLevel, gs.current_level);
    if (!gs.current_spawn.empty()) w.write_string_field(GS::CurrentSpawnPoint, gs.current_spawn);
    if (!gs.current_map_node.empty()) w.write_string_field(GS::CurrentMapNodeName, gs.current_map_node);
    for (const auto& quest : gs.quests) {
        w.write_bytes_field(GS::QuestState, encode_quest(quest));
    }
    if (!gs.raw_properties.empty()) w.write_bytes_field(GS::Properties, gs.raw_properties);
    if (!gs.selected_menu_tab.empty()) w.write_string_field(GS::SelectedMenuTab, gs.selected_menu_tab);
    for (const auto& qt : gs.raw_quest_texts) {
        w.write_bytes_field(GS::QuestText, qt);
    }
    if (gs.menu_button_flashing) w.write_varint_field(GS::MenuButtonFlashing, 1);
    if (gs.guide_enabled) w.write_varint_field(GS::GuideEnabled, 1);
    // Append unknown fields preserved from original
    // (raw_misc is already encoded protobuf bytes)
    // We append them directly
    return w.to_string() + gs.raw_misc;
}

// ============================================================
// Public API
// ============================================================

bool save_load(const std::string& filepath, SaveFile& out) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        std::cerr << "[SaveEditor] Cannot open: " << filepath << std::endl;
        return false;
    }
    
    std::string data((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());
    file.close();
    
    if (data.empty()) {
        std::cerr << "[SaveEditor] Empty file: " << filepath << std::endl;
        return false;
    }
    
    out = SaveFile{};
    out.filepath = filepath;
    out.modified = false;
    
    try {
        proto::Reader r(data);
        proto::Field f;
        while (r.read_field(f)) {
            switch (f.field_number) {
                case PP::Name: out.name = f.as_string(); break;
                case PP::ExperienceLevel: out.experience_level = (int)f.as_int(); break;
                case PP::TimePlayed: out.time_played = f.as_double(); break;
                case PP::GameState: out.game_state = decode_game_state(f.as_string()); break;
                case PP::EquippedWeaponName: out.equipped_weapon_name = f.as_string(); break;
                case PP::EquippedArmorName: out.equipped_armor_name = f.as_string(); break;
                case PP::CurrentLevelTitle: out.current_level_title = f.as_string(); break;
                case PP::PercentCompleted: out.percent_completed = f.as_float(); break;
                case PP::CheatEnabled: out.cheat_enabled = f.as_bool(); break;
                case PP::Identifier: out.identifier = f.as_string(); break;
                default: {
                    // Preserve unknown fields
                    proto::Writer tmp;
                    tmp.write_field(f);
                    out.raw_fields.push_back({f.field_number, tmp.to_string()});
                    break;
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[SaveEditor] Parse error: " << e.what() << std::endl;
        return false;
    }
    
    std::cout << "[SaveEditor] Loaded: " << filepath 
              << " (" << data.size() << " bytes, "
              << out.game_state.levels.size() << " levels, "
              << out.game_state.character.items.size() << " items)" << std::endl;
    return true;
}

bool save_write(const std::string& filepath, const SaveFile& sf) {
    proto::Writer w;
    
    // PlayerProfile fields
    w.write_string_field(PP::Name, sf.name);
    if (sf.experience_level != 0) w.write_varint_field(PP::ExperienceLevel, sf.experience_level);
    if (sf.time_played != 0.0) w.write_double_field(PP::TimePlayed, sf.time_played);
    
    // Encode GameState
    std::string gs_bytes = encode_game_state(sf.game_state);
    w.write_bytes_field(PP::GameState, gs_bytes);
    
    if (!sf.equipped_weapon_name.empty()) w.write_string_field(PP::EquippedWeaponName, sf.equipped_weapon_name);
    if (!sf.equipped_armor_name.empty()) w.write_string_field(PP::EquippedArmorName, sf.equipped_armor_name);
    if (!sf.current_level_title.empty()) w.write_string_field(PP::CurrentLevelTitle, sf.current_level_title);
    if (sf.percent_completed != 0.0f) w.write_float_field(PP::PercentCompleted, sf.percent_completed);
    if (sf.cheat_enabled) w.write_varint_field(PP::CheatEnabled, 1);
    if (!sf.identifier.empty()) w.write_string_field(PP::Identifier, sf.identifier);
    
    // Append preserved unknown fields
    for (const auto& [fn, raw] : sf.raw_fields) {
        auto& buf = const_cast<std::string&>(w.to_string()); // hack: direct append
    }
    
    // Write to file
    std::string output = w.to_string();
    // Append raw preserved fields
    for (const auto& [fn, raw] : sf.raw_fields) {
        output += raw;
    }
    
    // Backup original
    if (fs::exists(filepath)) {
        std::string backup = filepath + ".bak";
        try { fs::copy_file(filepath, backup, fs::copy_options::overwrite_existing); } catch (...) {}
    }
    
    std::ofstream file(filepath, std::ios::binary);
    if (!file) {
        std::cerr << "[SaveEditor] Cannot write: " << filepath << std::endl;
        return false;
    }
    file.write(output.data(), output.size());
    file.close();
    
    std::cout << "[SaveEditor] Saved: " << filepath << " (" << output.size() << " bytes)" << std::endl;
    return true;
}

std::string save_summary(const SaveFile& sf) {
    std::stringstream ss;
    const auto& ch = sf.game_state.character;
    
    ss << "=== Save: " << (sf.name.empty() ? "(unnamed)" : sf.name) << " ===\n";
    ss << "  Level: " << sf.experience_level << "  |  " 
       << (int)(sf.percent_completed * 100) << "% complete\n";
    ss << "  Area: " << sf.current_level_title << "\n";
    ss << "  Health: " << ch.health << "  Mana: " << ch.mana 
       << "  Coins: " << ch.coins << "\n";
    ss << "  XP: " << ch.xp << "  Char Level: " << ch.level << "\n";
    ss << "  Stats: HP+" << ch.health_attr << "  ATK+" << ch.attack_attr 
       << "  MAG+" << ch.magic_attr << "\n";
    ss << "  Weapon: " << ch.equipped_weapon << "\n";
    ss << "  Armor: " << ch.equipped_armor << "\n";
    ss << "  Skill: " << ch.current_skill << "\n";
    ss << "  Items (" << ch.items.size() << "): ";
    for (const auto& item : ch.items) {
        ss << item.name << "x" << item.count << " ";
    }
    ss << "\n  Skills (" << ch.skills.size() << "): ";
    for (const auto& s : ch.skills) ss << s << " ";
    ss << "\n  Levels visited: " << sf.game_state.levels.size();
    ss << "\n  Quests: " << sf.game_state.quests.size() << "\n";
    
    return ss.str();
}

std::vector<std::string> save_list_dir(const std::string& dir_path) {
    std::vector<std::string> saves;
    try {
        for (const auto& entry : fs::directory_iterator(dir_path)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                if (ext == ".gplayer") {
                    saves.push_back(entry.path().string());
                }
            }
        }
    } catch (...) {}
    // Sort by file size (largest = most progress) descending
    std::sort(saves.begin(), saves.end(), [](const std::string& a, const std::string& b) {
        return fs::file_size(a) > fs::file_size(b);
    });
    return saves;
}
