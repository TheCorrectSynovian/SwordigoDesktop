/*
 * mod_config.cpp — Mod configuration parser (host side)
 *
 * Simple line-by-line TOML-subset parser for mini.toml.
 * Populates a ModConfig struct that the caller writes to guest memory.
 *
 * Supported syntax:
 *   [section]           — section header
 *   key = value         — bare value (integers, floats)
 *   key = "value"       — quoted string value
 *   # comment           — line comments
 *
 * This runs on the HOST (C++ launcher), not inside the emulated guest,
 * so we can freely use <fstream>, <filesystem>, etc.
 */

#include "game/mod_config.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

/* =========================================================================
 * String helpers
 * ========================================================================= */

/* Trim leading and trailing whitespace */
static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r");
    return s.substr(start, end - start + 1);
}

/* Remove surrounding double quotes if present */
static std::string unquote(const std::string& s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
        return s.substr(1, s.size() - 2);
    return s;
}

/* Case-insensitive string comparison for section names */
static std::string to_lower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return out;
}

/* =========================================================================
 * parse_mod_config — Main parser
 * ========================================================================= */

ModConfig parse_mod_config(const std::string& assets_dir) {
    ModConfig cfg;

    /* ------------------------------------------------------------------
     * Locate mini.toml
     * ------------------------------------------------------------------ */
    std::string config_path = assets_dir + "/mini.toml";
    if (!fs::exists(config_path)) {
        config_path = assets_dir + "/resources/mini.toml";
        if (!fs::exists(config_path)) {
            std::cout << "[ModConfig] No mini.toml found in " << assets_dir
                      << std::endl;
            return cfg;
        }
    }

    std::ifstream f(config_path);
    if (!f) {
        std::cerr << "[ModConfig] Cannot open: " << config_path << std::endl;
        return cfg;
    }

    std::cout << "[ModConfig] Loading: " << config_path << std::endl;

    /* ------------------------------------------------------------------
     * Line-by-line parse
     * ------------------------------------------------------------------ */
    std::string section;
    std::string line;
    int entries_loaded = 0;

    while (std::getline(f, line)) {
        line = trim(line);

        /* Skip empty lines and comments */
        if (line.empty() || line[0] == '#')
            continue;

        /* Section header: [section_name] */
        if (line.front() == '[' && line.back() == ']') {
            section = to_lower(line.substr(1, line.size() - 2));
            continue;
        }

        /* Key = Value */
        auto eq = line.find('=');
        if (eq == std::string::npos)
            continue;

        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));

        /* ----------------------------------------------------------
         * [mod] — Mod identification
         * ---------------------------------------------------------- */
        if (section == "mod") {
            if (key == "name") {
                cfg.name = unquote(val);
                entries_loaded++;
            } else if (key == "version") {
                cfg.version = unquote(val);
                entries_loaded++;
            } else if (key == "author") {
                cfg.author = unquote(val);
                entries_loaded++;
            }
        }
        /* ----------------------------------------------------------
         * [options] — Gameplay settings
         * ---------------------------------------------------------- */
        else if (section == "options") {
            if (key == "coin_limit") {
                try {
                    int limit = std::stoi(val);
                    if (limit < 0)     limit = 0;
                    if (limit > 65535) limit = 65535;
                    cfg.coin_limit = limit;
                    std::cout << "[ModConfig]   coin_limit = " << limit
                              << std::endl;
                    entries_loaded++;
                } catch (...) {
                    std::cerr << "[ModConfig]   WARNING: invalid coin_limit: "
                              << val << std::endl;
                }
            } else if (key == "game_speed") {
                try {
                    float speed = std::stof(val);
                    if (speed > 0.0f && speed <= 10.0f) {
                        cfg.game_speed = speed;
                        std::cout << "[ModConfig]   game_speed = " << speed
                                  << std::endl;
                        entries_loaded++;
                    } else {
                        std::cerr << "[ModConfig]   WARNING: game_speed out of "
                                     "range (0, 10]: " << val << std::endl;
                    }
                } catch (...) {
                    std::cerr << "[ModConfig]   WARNING: invalid game_speed: "
                              << val << std::endl;
                }
            } else if (key == "too_rich_threshold") {
                try {
                    int threshold = std::stoi(val);
                    if (threshold < 0)     threshold = 0;
                    if (threshold > 65535) threshold = 65535;
                    cfg.too_rich_threshold = threshold;
                    std::cout << "[ModConfig]   too_rich_threshold = "
                              << threshold << std::endl;
                    entries_loaded++;
                } catch (...) {
                    std::cerr << "[ModConfig]   WARNING: invalid "
                                 "too_rich_threshold: " << val << std::endl;
                }
            }
        }
        /* ----------------------------------------------------------
         * [armor_models] — Model swap entries
         *
         * Format:
         *   item_id = "model_name"
         *   default = "model_name"   (sets default player model)
         * ---------------------------------------------------------- */
        else if (section == "armor_models") {
            std::string item_id = key;
            std::string model   = unquote(val);

            if (to_lower(item_id) == "default") {
                cfg.default_player_model = model;
                std::cout << "[ModConfig]   default_player_model = " << model
                          << std::endl;
            } else {
                cfg.armor_models.emplace_back(item_id, model);
                std::cout << "[ModConfig]   armor: " << item_id << " -> "
                          << model << std::endl;
            }
            entries_loaded++;
        }
        /* ----------------------------------------------------------
         * [cstrings] — String replacement entries
         *
         * Format:
         *   Original Text = "Replacement Text"
         * ---------------------------------------------------------- */
        else if (section == "cstrings") {
            std::string original    = key;
            std::string replacement = unquote(val);
            cfg.cstrings.emplace_back(original, replacement);
            std::cout << "[ModConfig]   cstring: \"" << original << "\" -> \""
                      << replacement << "\"" << std::endl;
            entries_loaded++;
        }
        /* ----------------------------------------------------------
         * [armor_attributes] — Damage multipliers per armor
         *
         * Format:
         *   item_id = 1.5
         * ---------------------------------------------------------- */
        else if (section == "armor_attributes") {
            std::string item_id = key;
            try {
                float multiplier = std::stof(val);
                cfg.armor_attributes.emplace_back(item_id, multiplier);
                std::cout << "[ModConfig]   armor_attr: " << item_id << " = "
                          << multiplier << "x" << std::endl;
                entries_loaded++;
            } catch (...) {
                std::cerr << "[ModConfig]   WARNING: invalid "
                             "armor_attributes value for " << item_id << ": "
                          << val << std::endl;
            }
        }
    }

    cfg.loaded = true;
    std::cout << "[ModConfig] Loaded " << entries_loaded
              << " config entries from mini.toml" << std::endl;

    /* Log mod identity if present */
    if (!cfg.name.empty()) {
        std::cout << "[ModConfig] Mod: " << cfg.name;
        if (!cfg.version.empty())
            std::cout << " v" << cfg.version;
        if (!cfg.author.empty())
            std::cout << " by " << cfg.author;
        std::cout << std::endl;
    }

    return cfg;
}
