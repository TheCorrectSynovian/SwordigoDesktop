/*
 * mod_config.h — Mod configuration parser (host side)
 *
 * Reads mini.toml from the active mod's assets directory and parses
 * it into a ModConfig struct. The caller (main.cpp / jni_bridge.cpp)
 * is responsible for writing the parsed values into guest memory via
 * the Unicorn memory mapping.
 *
 * Supported TOML sections:
 *   [mod]              — name, version, author
 *   [options]          — coin_limit, game_speed, too_rich_threshold
 *   [armor_models]     — item_id → model_name mappings
 *   [cstrings]         — original → replacement string pairs
 *   [armor_attributes] — item_id → damage multiplier
 */

#pragma once

#include <string>
#include <vector>
#include <utility>

struct ModConfig {
    /* ---- [mod] section ---- */
    std::string name;
    std::string version;
    std::string author;

    /* ---- [options] section ---- */
    int   coin_limit         = 999;
    float game_speed         = 1.0f;
    int   too_rich_threshold = 999;

    /* ---- [armor_models] section ---- */
    /* Each pair: item_id → model_name */
    std::vector<std::pair<std::string, std::string>> armor_models;
    std::string default_player_model = "hiro";

    /* ---- [cstrings] section ---- */
    /* Each pair: original → replacement */
    std::vector<std::pair<std::string, std::string>> cstrings;

    /* ---- [armor_attributes] section ---- */
    /* Each pair: item_id → damage_multiplier */
    std::vector<std::pair<std::string, float>> armor_attributes;

    /* True if mini.toml was found and parsed successfully */
    bool loaded = false;
};

/*
 * parse_mod_config — Parse mini.toml from an assets directory
 *
 * Looks for mini.toml at:
 *   1. <assets_dir>/mini.toml
 *   2. <assets_dir>/resources/mini.toml
 *
 * Returns a populated ModConfig struct. Check .loaded to see if
 * parsing succeeded.
 */
ModConfig parse_mod_config(const std::string& assets_dir);
