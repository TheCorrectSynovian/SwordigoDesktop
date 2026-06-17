#pragma once
#include <string>

// Resolve a relative path to the game data directory.
// Priority: SWORDIGO_DATA_DIR env > ./ > /usr/share/swordigo/ > /usr/local/share/swordigo/
std::string get_data_path(const std::string& relative_path);
